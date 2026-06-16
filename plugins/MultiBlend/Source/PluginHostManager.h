#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_audio_utils/juce_audio_utils.h>
#include <atomic>
#include <thread>
#include <set>


class PluginHostManager
{
public:
    PluginHostManager()
    {
        juce::addDefaultFormatsToManager (formatManager);
        loadKnownPluginListFromDisk();
        recoverFromCrashedScan();
    }

    ~PluginHostManager()
    {
        stopScan();
    }

    //==========================================================================
    // Persistence — XML in app data folder
    //==========================================================================
    juce::File getPluginListFile() const
    {
        auto dir = juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory)
                       .getChildFile ("Entropia Audio")
                       .getChildFile ("MultiBlend");
        dir.createDirectory();
        return dir.getChildFile ("known-plugins.xml");
    }

    juce::File getScanScratchFile() const
    {
        return getPluginListFile().getParentDirectory().getChildFile ("scanning-now.txt");
    }

    // If scratch file exists at startup, last scan crashed on that plugin.
    // Blacklist it + save list + delete scratch so it won't be rescanned.
    void recoverFromCrashedScan()
    {
        auto scratch = getScanScratchFile();
        if (! scratch.existsAsFile()) return;
        auto crashedPath = scratch.loadFileAsString().trim();
        scratch.deleteFile();
        if (crashedPath.isEmpty()) return;
        DBG ("Blacklisting crashed plugin: " + crashedPath);
        knownPluginList.addToBlacklist (crashedPath);
        saveKnownPluginListToDisk();
    }

    void loadKnownPluginListFromDisk()
    {
        auto file = getPluginListFile();
        if (! file.existsAsFile()) return;
        if (auto xml = juce::XmlDocument::parse (file))
            knownPluginList.recreateFromXml (*xml);
    }

    void saveKnownPluginListToDisk()
    {
        if (auto xml = knownPluginList.createXml())
        {
            auto file = getPluginListFile();
            xml->writeTo (file);
        }
    }

    //==========================================================================
    // Async plugin instance creation
    //==========================================================================
    void createPluginAsync (const juce::PluginDescription& desc,
                            double sampleRate,
                            int blockSize,
                            std::function<void (std::unique_ptr<juce::AudioPluginInstance>, const juce::String&)> callback)
    {
        formatManager.createPluginInstanceAsync (desc, sampleRate, blockSize,
            [cb = std::move (callback)] (std::unique_ptr<juce::AudioPluginInstance> instance,
                                          const juce::String& error)
            {
                cb (std::move (instance), error);
            });
    }

    //==========================================================================
    // Access plugin list
    //==========================================================================
    juce::Array<juce::PluginDescription> getAvailablePlugins() const
    {
        return knownPluginList.getTypes();
    }

    int getNumPlugins() const { return knownPluginList.getNumTypes(); }

    //==========================================================================
    // Background scan with progress
    //==========================================================================
    // enabledFormats: format names ("VST3", "AudioUnit", "VST"). Empty = all formats.
    void startScan (std::function<void()> onComplete = nullptr,
                    std::set<juce::String> enabledFormats = {})
    {
        if (scanInProgress.load()) return;
        stopScan();

        scanInProgress.store (true);
        scanProgress.store (0.0f);
        scanCurrentFile = "";

        {
            const juce::ScopedLock l (formatFilterLock);
            activeFormatFilter = std::move (enabledFormats);
        }

        scanThread = std::thread ([this, cb = std::move (onComplete)]()
        {
            runScan();
            scanInProgress.store (false);
            scanProgress.store (1.0f);

            juce::MessageManager::callAsync ([this, cb]()
            {
                saveKnownPluginListToDisk();
                if (cb) cb();
            });
        });
    }

    void stopScan()
    {
        shouldStopScan.store (true);
        if (scanThread.joinable())
            scanThread.join();
        shouldStopScan.store (false);
    }

    bool isScanning() const { return scanInProgress.load(); }
    float getScanProgress() const { return scanProgress.load(); }

    juce::String getCurrentScanFile()
    {
        const juce::ScopedLock lock (currentFileLock);
        return scanCurrentFile;
    }

    juce::AudioPluginFormatManager formatManager;
    juce::KnownPluginList knownPluginList;

private:
    void runScan()
    {
        // Snapshot format filter (set under lock by startScan)
        std::set<juce::String> filter;
        {
            const juce::ScopedLock l (formatFilterLock);
            filter = activeFormatFilter;
        }

        // Collect all plugin files first to compute progress
        juce::Array<juce::File> allFiles;
        juce::Array<juce::AudioPluginFormat*> fileFormats;

        for (int i = 0; i < formatManager.getNumFormats(); ++i)
        {
            auto* format = formatManager.getFormat (i);
            if (! filter.empty() && filter.count (format->getName()) == 0)
                continue;   // format not enabled
            auto locations = format->getDefaultLocationsToSearch();

            for (int j = 0; j < locations.getNumPaths(); ++j)
            {
                juce::FileSearchPath singlePath;
                singlePath.add (locations[j]);
                auto files = format->searchPathsForPlugins (singlePath, true);
                for (auto& f : files)
                {
                    allFiles.add (f);
                    fileFormats.add (format);
                }
            }
        }

        const int total = allFiles.size();
        if (total == 0) return;

        auto blacklisted = knownPluginList.getBlacklistedFiles();

        for (int idx = 0; idx < total; ++idx)
        {
            if (shouldStopScan.load()) break;

            auto& file = allFiles.getReference (idx);
            auto* format = fileFormats[idx];
            auto fullPath = file.getFullPathName();

            // Skip blacklisted plugins (crashed previously)
            if (blacklisted.contains (fullPath))
            {
                scanProgress.store (static_cast<float> (idx + 1) / static_cast<float> (total));
                continue;
            }

            {
                const juce::ScopedLock lock (currentFileLock);
                scanCurrentFile = file.getFileName();
            }

            scanFileOutOfProcess (fullPath, *format);

            if ((idx % 5) == 0) saveKnownPluginListToDisk();
            scanProgress.store (static_cast<float> (idx + 1) / static_cast<float> (total));
        }
    }

    //==========================================================================
    // Fork child process for each plugin scan. If plugin crashes, only child
    // dies — parent survives + blacklists the file.
    // On Windows: falls back to in-process scan (no fork available).
    //==========================================================================
    juce::File findScannerExecutable() const
    {
        // Scanner is bundled in plugin/app Resources/MultiBlendScanner.
        // Resolve relative to current executable (the plugin's binary inside .vst3 / .component).
        auto exe = juce::File::getSpecialLocation (juce::File::currentExecutableFile);
        // For .vst3, .component, .app: exe is at Bundle.xxx/Contents/MacOS/PluginName.
        // Resources sibling: ../Resources/MultiBlendScanner
        auto resources = exe.getParentDirectory().getParentDirectory().getChildFile ("Resources");
        auto candidate = resources.getChildFile ("MultiBlendScanner");
        if (candidate.existsAsFile()) return candidate;

        // Fallback: search nearby (dev builds)
        auto dev = exe.getParentDirectory().getChildFile ("MultiBlendScanner");
        if (dev.existsAsFile()) return dev;
        return {};
    }

    void scanFileOutOfProcess (const juce::String& fullPath, juce::AudioPluginFormat& format)
    {
        juce::ignoreUnused (format);
        auto scanner = findScannerExecutable();
        if (! scanner.existsAsFile())
        {
            // No scanner available — fall back to in-process with scratch recovery
            auto scratch = getScanScratchFile();
            scratch.replaceWithText (fullPath);
            juce::OwnedArray<juce::PluginDescription> descs;
            knownPluginList.scanAndAddFile (fullPath, true, descs, format);
            scratch.deleteFile();
            return;
        }

        auto resultFile = juce::File::getSpecialLocation (juce::File::tempDirectory)
                              .getChildFile ("mbe_scan_" + juce::String (juce::Random::getSystemRandom().nextInt64()) + ".xml");
        resultFile.deleteFile();

        juce::ChildProcess proc;
        juce::StringArray cmd;
        cmd.add (scanner.getFullPathName());
        cmd.add (fullPath);
        cmd.add (resultFile.getFullPathName());

        if (! proc.start (cmd, juce::ChildProcess::wantStdOut | juce::ChildProcess::wantStdErr))
        {
            knownPluginList.addToBlacklist (fullPath);
            return;
        }

        // Wait up to 30 seconds for scan to complete
        const int timeoutMs = 30000;
        if (! proc.waitForProcessToFinish (timeoutMs))
        {
            proc.kill();
            knownPluginList.addToBlacklist (fullPath);
            resultFile.deleteFile();
            return;
        }

        int exitCode = proc.getExitCode();
        if (exitCode != 0)
        {
            knownPluginList.addToBlacklist (fullPath);
            resultFile.deleteFile();
            return;
        }

        if (resultFile.existsAsFile())
        {
            if (auto xml = juce::XmlDocument::parse (resultFile))
            {
                for (auto* child : xml->getChildIterator())
                {
                    juce::PluginDescription d;
                    if (d.loadFromXml (*child))
                        knownPluginList.addType (d);
                }
            }
            resultFile.deleteFile();
        }
    }

    juce::CriticalSection formatFilterLock;
    std::set<juce::String> activeFormatFilter;   // empty = all formats
    std::thread scanThread;
    std::atomic<bool> scanInProgress { false };
    std::atomic<bool> shouldStopScan { false };
    std::atomic<float> scanProgress { 0.0f };
    juce::String scanCurrentFile;
    juce::CriticalSection currentFileLock;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PluginHostManager)
};
