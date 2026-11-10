# Updating the plugin (the easy way)

You never need Visual Studio. GitHub builds it for you every time you change a file.

## The update loop
1. On GitHub, open the file you want to change and click the pencil icon (or drag new files in with Add file -> Upload files).
2. Click **Commit changes**.
3. Open the **Actions** tab. A new build starts by itself (about 5-10 min; faster after the first one).
4. When it's green, open the run and download `SonyTune-VST3-Windows-<number>`. The number goes up every build, so you always know which is newest.
5. Unzip, right-click `INSTALL.bat` -> Run as administrator, rescan plugins in FL.
   (Close FL first, or the old file may be locked.)

## Want a clean numbered release? 
Create a tag like `v1.1.0` on GitHub (Releases -> Draft a new release -> choose a new tag). The build attaches a zip to that release page so you have a permanent download for each version.

## Where to change things
| I want to change...                      | File                          |
|------------------------------------------|-------------------------------|
| Plugin name or version number            | `CMakeLists.txt` (top, 2 lines) |
| Colours, tagline                         | `Source/Brand.h`              |
| Layout, knob/button look, pitch screen   | `Source/PluginEditor.cpp`     |
| Add / remove / retune a control          | `Source/PluginProcessor.cpp` (parameters) + `PluginEditor.cpp` (knob) |
| Pitch tracking, FAST FLOW, shifting      | `Source/SonyTuneDSP.h`        |

Bumping the version: change `PLUGIN_VERSION` in `CMakeLists.txt`. The footer of the plugin shows it.

## Renaming the plugin
Change `PLUGIN_NAME` in `CMakeLists.txt`. The window title, installer and download name all follow it. If FL treats it as a new plugin, that's normal; presets saved under the old name will not load into it.

## Do NOT change these after you have projects saved
- `PLUGIN_CODE` / `PLUGIN_MANUFACTURER_CODE` in `CMakeLists.txt`
- The parameter IDs in `PluginProcessor.cpp` (`"speed"`, `"key"`, ...)
Changing them makes FL forget saved settings in old projects.

## Testing the sound without building the plugin
    cd tests
    g++ -std=c++17 -O2 -o run_engine run_engine.cpp
    python3 test_engine.py
Run this after any change to `SonyTuneDSP.h`. All checks should say PASS.

## Something broke?
If a build goes red, open the failed run, copy the red error lines and paste them to Claude. A broken build never overwrites the plugin you already installed.
