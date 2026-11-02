LTA MICROWAVE v1.0.0 — WINDOWS VST3 SOURCE + GITHUB BUILD
===========================================================

This is a source project for the actual, playable LTA MICROWAVE VST3.
The ZIP is not an already-compiled Windows VST3. Use the GitHub build below.

STEP 1: GITHUB (same method as LTA Crystalize)
------------------------------------------------
1) Visit github.com and create a NEW PUBLIC repository called lta-microwave.
2) Choose "Add file" > "Upload files".
3) Extract THIS ZIP locally before uploading. Upload its contents:
   - CMakeLists.txt
   - Source/PluginProcessor.cpp and .h, PluginEditor.cpp and .h
   - Assets/microwave_bg.png
   - README_FIRST.txt
   - .github/workflows/build-windows.yml
   Files must have those exact repository paths (not all crammed into root).
4) Commit the files to main. If the .github folder isn't uploaded via browser,
   create workflow file in GitHub: Add file > Create new file, type
   .github/workflows/build-windows.yml and paste the YAML from this project.
5) Go to Actions > Build LTA MICROWAVE VST3 > Run workflow > Run workflow.
   (The first build may spend a couple of minutes downloading JUCE.)
6) Once SUCCESS is green, scroll down to Artifacts, click
   LTA-MICROWAVE-Windows-VST3 and download it.
7) Extract the artifact ZIP and then extract the ZIP inside it if present.
   You want the FOLDER "LTA MICROWAVE.vst3" (a VST3 bundle is a folder).

STEP 2: INSTALL IN FL STUDIO
---------------------------
1) COPY the "LTA MICROWAVE.vst3" folder to:
     C:\Program Files\Common Files\VST3\
2) Open FL Studio > Options > Manage plugins > Find installed plugins.
3) Search for "LTA MICROWAVE" in the plugin manager.
4) Put it in an FX slot on the Master mixer channel (or any other channel).

STEP 3: USE IT
--------------
- Click physical microwave FX buttons to toggle effects ON/OFF.
- The 9 effects STACK together. Each red LED shows the active preset.
- START turns processing on, STOP bypasses it WITHOUT losing selections.
- Drag the POWER knob up/down. Mousewheel over knob also works.
- Drag MIX text on green LCD up/down to change overall wet/dry.
- Hold SHIFT while dragging/wheeling POWER knob to change MIX instead.
- The waveform behind the window reads live audio from the plugin input.
- Settings are stored inside FL Studio projects and automatable by the host.

FX BUTTONS
----------
DISTORT: warm-to-hard soft clipping
OVERCOOK: hard clipping and bit reduction
WARP: analogue-style chorus / detuning
GLITCH: rapid stutter / repeats
DEFROST: dark filtering and flutter
BASS: enhanced low-frequency harmonics
REHEAT: slap / feedback delay
STEAM: compact atmospheric feedback reverb
COMPRESS: linked dynamic compression

IMPORTANT
---------
Master effects can get LOUD. Start with LOW POWER and reduce channel gain
before enabling DISTORT, OVERCOOK or BASS. This is a creative FX processor,
not a calibrated mastering limiter. A simple safety clipper is active only
when effects are running, so keep enough headroom for a finished master.

Internals: JUCE 8.0.6, C++17, sample-rate aware DSP, no dynamic allocations
in the audio processing callback, APVTS state and host parameter automation.
Artwork generated specifically for LTA MICROWAVE and embedded with BinaryData.

Version 1.0.0, Windows 64-bit, VST3 for FL Studio.
