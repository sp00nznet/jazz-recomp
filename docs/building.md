# Building Jazz Jackrabbit by hand

`Setup.cmd` runs exactly these steps (`tools/setup.ps1`).

## Prerequisites

Prerequisites: Windows 10/11, Python 3 (`py -3 --version`), Git, and
[MSYS2](https://www.msys2.org/) with `mingw-w64-x86_64-gcc`,
`mingw-w64-x86_64-cmake` and `mingw-w64-x86_64-ninja`
(`pacman -S mingw-w64-x86_64-gcc mingw-w64-x86_64-cmake mingw-w64-x86_64-ninja`),
with `C:\msys64\mingw64\bin` on `PATH`.

1. **The toolkit**, beside this folder, on `main`:
   ```
   git clone https://github.com/sp00nznet/pcrecomp ..\pcrecomp
   ```
   `tools\relift.py` and CMake look for `..\pcrecomp` (or `PCRECOMP_DIR`).
2. **Your game files**: copy everything from the CD's root into `game\`.
3. **No sound card**, as the game's own SETUP would set it up:
   ```
   > py -3 tools\make_soundcrd.py
   wrote SOUNDCRD.INF and NOSOUND.MUS (no sound card)
   ```
4. **Lift** into `work\` (pcrecomp builds the code map itself):
   ```
   > py -3 tools\relift.py
     FILE0001: native code map, 644 functions, 71757 heads
   Wrote work\src\_unresolved_stubs.c: 0 unresolved stubs (7147 lifted functions, 6934 distinct call targets, 0.00% unresolved)
   ...
   image: 391648 bytes (0.37 MB) -> work\mem_image.bin
   ```
5. **Build**:
   ```
   cmake -S . -B build -G Ninja -DCMAKE_C_COMPILER=C:/msys64/mingw64/bin/gcc.exe -DPCRECOMP_DIR=%PCRECOMP_DIR%
   cmake --build build
   ```
   Expected: `Linking C executable jazz.exe`.
6. **Play**: `build\jazz.exe`. The first run opens the game's options
   screen; pick DONE.

The usual trip-ups: `python` opening the Microsoft Store (that is Windows'
alias placeholder; use `py -3`); a freshly installed tool not being on `PATH`
until you open a new window; and a `..\pcrecomp` on `main`, which
lacks the DOS-extended Win16 host and the native code map.

## Notes

- The toolkit is pcrecomp's `main` (the native NE code map and the run-time
  code hook the compiled sprites need).
- The KERNEL imports are named from pcrecomp's built-in Win16 ordinal table
  (`tools/ne/win16.py`); this game needs no ordinal map file.
- The paths to `game\` and `work\mem_image.bin` are compiled in, so the exe
  runs from this checkout.
- On the lab, `netlab build jazz-recomp` builds it on the MinGW builder (a
  compile check; the game data stays behind).
