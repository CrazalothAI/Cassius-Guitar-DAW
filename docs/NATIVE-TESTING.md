# Native memory and lifetime checks

Run the normal Release build and `ctest --preset release --output-on-failure` first. Five CTest entries cover the full processor suite, repeated sound ZIP/stereo ambience construction/render/destruction, and three NAM architectures. No focused test replaces the full processor run.

The native executable also accepts a pinned NAM fixture followed by `--quality`, `--starting-rigs`, `--sound-pack-stress`, or `--startup` for fault isolation. Sound-pack stress performs sixteen independent lifetimes. Normal CTest keeps the full suite and runs that stress check separately.

On Windows with Visual Studio C++ AddressSanitizer installed, run:

```powershell
./scripts/test-native-memory.ps1
./scripts/test-native-memory.ps1 -Check sound-pack
```

This builds instrumented native tests in `build-memory`, with exception handling preserved, debug symbols, and a separate set of object files. Output and errors are retained under `build-memory/memory-validation`. Only pinned fixtures are used; optional developer sound banks are excluded. `-Check startup` isolates static initialization/shutdown, and `-Check starting-rigs` isolates complete factory recall. The ordinary `build` directory is rejected as a diagnostic target. Do not distribute diagnostic executables or their sanitizer DLLs.

The script uses standard Windows ASan options. Its additional allocator-family mismatch check is off by default on Windows. Enabling it on this static-runtime toolchain produced a startup-only report for JUCE's matching `new char[]`/`delete[]`, where the array allocation hook forwards through the scalar hook; even `--startup` reproduced that diagnostic. This limitation does not disable bounds, use-after-free, or double-free checks. See Microsoft's [ASan options](https://learn.microsoft.com/en-us/cpp/sanitizers/asan-flags) and [runtime limitations](https://learn.microsoft.com/en-us/cpp/sanitizers/asan-known-issues).

If clean CI's ordinary native tests fail, the workflow retains its JUnit/log evidence, then runs the isolated memory diagnostic and retains those logs too. A passing diagnostic does not override a failed ordinary test or permit release publication. Repeated passes do not prove the absence of a timing-dependent fault; keep any unresolved crash recorded until clean CI succeeds and practical acceptance is completed.
