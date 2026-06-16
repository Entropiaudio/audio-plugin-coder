# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## What This Is

Audio Plugin Coder (APC) — an AI-driven framework for building professional audio plugins using JUCE 8. Plugins go through a 5-phase lifecycle: Dream → Plan → Design → Implement → Ship. Each phase is triggered by a slash command (`/dream`, `/plan`, `/design`, `/impl`, `/ship`) which loads a corresponding SKILL.md file from `.claude/skills/`.

## Build Commands

**Never run cmake/msbuild manually.** Always use the PowerShell scripts from the repository root.

```powershell
# Full build + install (primary command)
.\scripts\build-and-install.ps1 -PluginName "PluginName"

# Preview WebView UI (open in browser)
# Just open plugins/[Name]/Design/index.html in Edge/Chrome

# System prerequisites check
.\scripts\system-check.ps1
```

There are no tests or linters in this project. Validation is done via `pluginval` during the Ship phase:
```powershell
.\scripts\pluginval-integration.ps1 -PluginName "PluginName"
```

## Architecture

### Monorepo Structure

- `_tools/` — Git submodules: JUCE 8, Visage (optional), pluginval
- `plugins/` — Each plugin is a self-contained subdirectory with its own CMakeLists.txt
- `templates/` — Boilerplate for WebView, Visage, FFGL, and Max/MSP plugins
- `scripts/` — PowerShell automation (build, state management, error detection, backup/rollback)
- `.claude/skills/` — Phase-specific instruction files (SKILL.md per phase)
- `.claude/rules/` — System constraints loaded into every conversation
- `.claude/troubleshooting/` — Auto-captured known issues database (YAML + resolution docs)

### Build System (CMake)

Root `CMakeLists.txt` auto-discovers plugins by scanning `plugins/*/CMakeLists.txt`. Plugin CMakeLists must NOT call `juce_add_modules` (causes duplicate target errors). JUCE is loaded once at root level.

Platform-specific WebView flags are critical:
- **Windows:** `NEEDS_WEBVIEW2 TRUE` (WebView2 SDK)
- **Linux:** `NEEDS_WEB_BROWSER TRUE` (webkit2gtk + GTK includes)
- **macOS:** Neither flag needed (WKWebView is a system framework)

WebView HTML is embedded via `juce_add_binary_data()` and accessed as binary resources at runtime.

### UI Framework Fork

Every plugin chooses one UI path during the Plan phase, stored in `status.json`:

- **WebView (PATH B):** HTML5 Canvas + inline JS/CSS, rendered in `juce::WebBrowserComponent`. Frontend communicates with C++ via `window.__JUCE__`. All assets must be inline or bundled as binary data. ES6 module imports do NOT work in JUCE's WebView.
- **Visage (PATH A):** Pure C++ using the Visage framework. No HTML/CSS/JS. Custom controls inherit from `visage::Frame`.

### State Management

Each plugin has a `status.json` at its root tracking: current phase, UI framework choice, complexity score, phase history, validation flags, and error recovery state. The `scripts/state-management.ps1` module provides `New-PluginState`, `Update-PluginState`, `Test-PluginState`, `Backup-PluginState`, and `Restore-PluginState` functions. Phase gating prevents skipping phases.

### Plugin File Layout

```
plugins/[Name]/
├── .ideas/              # creative-brief.md, parameter-spec.md, architecture.md, plan.md
├── Design/              # UI specs, style guides, index.html (WebView)
├── Source/              # PluginProcessor.h/cpp, PluginEditor.h/cpp
├── CMakeLists.txt
└── status.json          # Project state (MUST read before any phase work)
```

### Audio Thread Patterns

Lock-free communication between audio and UI threads uses `std::atomic<float>`. See SNIPBridge for the canonical example: atomic data bridge updated from `processBlock()`, polled by UI at 30Hz.

### CI/CD

GitHub Actions workflows in `.github/workflows/`:
- `build-pr.yml` — Detects changed plugins, builds Win/Mac/Linux, posts summary to PR
- `build-release.yml` — Tag-triggered release builds
- Linux WebView builds require `xvfb-run` wrapper for LV2/Standalone targets
- macOS universal binaries use `CMAKE_OSX_ARCHITECTURES` at configure time (never per-target `XCODE_ATTRIBUTE_ARCHS`)

## Key Constraints

- **Shell:** PowerShell only. No `mkdir -p`, `rm`, `cp`. Use `New-Item`, `Remove-Item`, `Copy-Item`.
- **Phase gating:** Read `status.json` before any phase work. Complete phases sequentially. Stop after each phase.
- **Root CMakeLists.txt** must include `LANGUAGES C CXX` (C is required for JUCE internals).
- **Troubleshooting:** Check `.claude/troubleshooting/known-issues.yaml` before attempting random fixes. After 3 failed attempts at the same issue, trigger the auto-capture protocol documented in `.claude/rules/agent.md`.
