# SonyTune (CrushAudio) - pitch correction, JUCE 8 / VST3

## Build with GitHub (no compiler needed)
1. Make a new GitHub repo and upload everything in this folder (including the hidden `.github` folder).
2. Open the repo's **Actions** tab -> "Build SonyTune VST3" -> **Run workflow** (it also runs on every push).
3. When it finishes (about 10 min the first time), open the run and download **SonyTune-VST3-Windows**.
4. Unzip it, right-click `INSTALL.bat` -> Run as administrator, rescan plugins in FL Studio.

## Build on Windows yourself (same way as CRUSH and LOCK)
1. Install Visual Studio 2022 (workload: "Desktop development with C++"), CMake and Git.
2. Open a terminal in this folder and run:

       cmake -B build -G "Visual Studio 17 2022" -A x64
       cmake --build build --config Release

   (first configure downloads JUCE 8.0.6; if JUCE is already on disk add
   `-DFETCHCONTENT_SOURCE_DIR_JUCE=C:/path/to/JUCE`)
3. Right-click `INSTALL.bat` -> Run as administrator, then rescan plugins in FL Studio.
   The built plugin is `build\SonyTune_artefacts\Release\VST3\SonyTune.vst3`.

## Updating / changing things
See **UPDATING.md**. Short version: edit a file on GitHub, commit, download the new build from the Actions tab.

## Using it
- KEY / SCALE: the notes it is allowed to snap to (bright lines on the pitch screen).
- SPEED: retune time in ms. 0 = instant hard tune, 15-30 = classic, 100+ = natural.
- AMOUNT: how much correction is applied.
- HUMANIZE: how much of your natural vibrato / inflection is kept.
- FORMANT: shifts the voice character without changing the note (-5..+5 semitones).
- FAST FLOW: tuned for very fast rap (shorter analysis, steadier note selection).
- HQ MODE: looks further ahead for a cleaner pitch track. Adds latency (~62 ms vs ~37 ms);
  FL compensates automatically. Switch it while stopped, then replay.
- Put it FIRST on a dry vocal, before reverb/delay/saturation.

## Test the DSP without JUCE
    cd tests
    g++ -std=c++17 -O2 -o run_engine run_engine.cpp
    python3 test_engine.py        # needs numpy

## Files
- Source/Brand.h        - colours and tagline (restyle here)
- Source/SonyTuneDSP.h  - the whole pitch engine (YIN tracker, scale quantiser, PSOLA resynth)
- Source/PluginProcessor.* - parameters, host latency reporting
- Source/PluginEditor.*    - UI and live pitch screen
