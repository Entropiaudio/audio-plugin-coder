
#pragma once

#include <juce_core/juce_core.h>
#include <juce_data_structures/juce_data_structures.h>
#include <juce_events/juce_events.h>

#define HttpTimeout 10000

namespace Moonbase
{

    class HttpHelper : public juce::Thread
    {
    public:
        // post/get
        HttpHelper (juce::URL inputStreamUrl, const juce::String& header, bool isPost, std::function<void (bool, juce::String, HttpHelper*)> onResponse);
        HttpHelper (juce::URL inputStreamUrl, const juce::StringArray& headers, bool isPost, std::function<void (bool, juce::String, HttpHelper*)> onResponse);
        HttpHelper (juce::URL inputStreamUrl, const juce::String& header, bool isPost, std::function<void (bool, juce::MemoryBlock& responseBlock, HttpHelper*)> onResponse);

        // streaming
        HttpHelper (juce::URL inputStreamUrl, const juce::StringArray& headers, bool isPost, std::function<void (const juce::String& message, bool isStart, bool isEnd, bool isError, HttpHelper*)> onStreamingResponse);
        ~HttpHelper () override;

        int getResponseCode () const;

    private:
        juce::String header;

        juce::URL inputStreamUrl;

        bool isStreaming { false };

        std::function<void (bool, juce::String, HttpHelper*)> onResponse { nullptr };
        std::function<void (bool, juce::MemoryBlock&, HttpHelper*)> onResponseBlock { nullptr };

        std::function<void (const juce::String& message, bool isStart, bool isEnd, bool isError, HttpHelper*)> onStreamingResponse;

        void run () override;

        bool post { false };

        void processBlocking ();
        void processStreaming ();

        juce::MemoryBlock responseBlock;
        juce::Atomic<int> responseCode { -1 };


        juce::String requestUrl;
        bool progressCallback (int bytesSent, int totalBytes) const; 
        int httpStatusCode;
        juce::StringPairArray responseHeaders;

        bool runSync { false }; //not exposed currently, but could be set via constructor to process in sync

        JUCE_DECLARE_WEAK_REFERENCEABLE (HttpHelper)
        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (HttpHelper)
    };

    class ImageDownloader : public juce::Thread
    {
    public:
        ImageDownloader (juce::URL imageUrl, std::function<void (juce::Image, ImageDownloader*)> onImageReceived);
        ~ImageDownloader () override;
    
    private:
        juce::URL imageUrl;
        std::function<void (juce::Image, ImageDownloader*)> onImageReceived;
        void run () override;
    
        int httpTimeout { 8000 };
        
        JUCE_DECLARE_WEAK_REFERENCEABLE (ImageDownloader)
        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ImageDownloader)
    };
    

} // namespace Moonbase
