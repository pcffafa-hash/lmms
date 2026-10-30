# LMMS 1.2.2 for Android - work in progress

Status: **groundwork only. Nothing here has been compiled for Android yet**, because
the Android NDK and Qt for Android were not available when this was written.

## Done
- `LMMS_BUILD_ANDROID` platform detection (`cmake/modules/DetectMachine.cmake`).
- Android block in `CMakeLists.txt`: switches off ALSA/JACK/Pulse/sndio/PortAudio/SDL,
  VST, Carla, GIG, STK. Enables `WANT_OBOE`.
- New audio backend `AudioOboe` (`include/AudioOboe.h`, `src/core/audio/AudioOboe.cpp`),
  wired into `Mixer::tryAudioDevices()` and the setup dialog. It reopens the stream
  when the audio route changes. Syntax-checked against the real Oboe 1.9.3 headers
  (with stubs for Qt/LMMS), not linked or run.
- `plugins/CMakeLists.txt` drops plug-ins that need SysV shared memory + a helper
  process (VeSTige, VstEffect, ZynAddSubFX, Carla) or LADSPA/Wine.
- `lmms` is built as a shared library on Android (needed by `androiddeployqt`).
- `android/build-deps.sh` cross-compiles Oboe, libsamplerate, libsndfile, fftw3f.
  `.github/workflows/android.yml` is a manually triggered build draft.

## Still open (expect the first builds to fail)
1. Qt 5 only: build with `-DWANT_QT5=ON` (Qt 5.15.2 + NDK r21e).
2. Getting the dependency build and the CMake configure to pass.
3. Packaging: `androiddeployqt` + `AndroidManifest.xml` + Gradle wrapper; the
   `plugins/*.so` must end up in the APK's `lib/` dir and be found by `PluginFactory`.
4. Data (samples, presets, themes) must be shipped as assets and copied to app storage
   on first start; `ConfigManager` paths need adapting; scoped storage for projects.
5. Real-time safety: LMMS renders the whole period inside the audio callback. Expect
   underruns on slow phones until buffer sizes/threads are tuned.
6. Touch UI: LMMS 1.2.2 is a mouse-and-keyboard UI with tiny controls.
