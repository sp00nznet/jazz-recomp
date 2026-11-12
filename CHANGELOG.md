# Changelog

All notable changes to this project are documented here. The format follows
[Keep a Changelog](https://keepachangelog.com/en/1.1.0/), and the project uses
[Semantic Versioning](https://semver.org/).

## [Unreleased]

### Added
- `NOTICE.md` (replacing `NOTICE`), `CONTRIBUTING.md`, `CONTRIBUTORS.md`,
  `docs/building.md`, `docs/testing.md`, and a `setup.sh` that says plainly
  there is no Linux or macOS build yet.
- `JAZZ_KEYS` takes `key*N` to hold a key for N seconds.
- `Setup.cmd` (`tools/setup.ps1`): checks for Python, Git and MSYS2's
  MinGW-w64 gcc, CMake and Ninja, offering to install them; copies the CD into
  `game\`, sets up "no sound card", fetches pcrecomp's `pcrecomp-ne-jazz`
  branch at the tested commit, lifts, builds and leaves a shortcut.
- `launcher/launcher.json` for recomp-launcher, and an installer workflow
  (manual only while the repo is private).
- A recomp-netlab recipe (`jazz-recomp`, mingw builder; a compile check).

### Fixed
- In game: Jazz, the enemies and the masked tiles are compiled sprites (x86
  the game generates at run time); the runtime now runs them through a small
  interpreter behind pcrecomp's new `recomp_dispatch_miss_hook`
  (`pcrecomp-ne-jazz`). Diamondus 1 plays.
- The IDA-free lift stopped with Runtime error 105 before the intro: six
  functions the native code map misses are seeded in `tools/entry_seeds.json`.
- `launcher/launcher.json` pinned pcrecomp at `3097e4a`, before the run-time
  code hook; it now pins `9c79acc`, the commit setup uses.

### Changed
- The lift needs no IDA: pcrecomp builds the code map itself
  (`tools/ne/ne_codemap.py`). `tools/relift.py` lost `--ida`, and `Setup.cmd`
  no longer checks for idalib.
- README in the ra2 house style; the step-by-step build moved to
  `docs/building.md` and the test variables to `docs/testing.md`. The
  prerequisites no longer list `capstone`, which nothing in the lift imports.
- `.gitignore`: `build-*/` (netlab's `build-farm*`), `setup.log`, the shortcut.

## [0.1.0] - 2026-11-08

### Added
- `FILE0001.EXE` lifted with pcrecomp's ne_link (6,754 functions, 0
  unresolved) on its Win16 host, with a DOS machine in `runtime/`: DPMI, DOS,
  the BIOS, RTM's heap, the VGA in Mode X, the PIT and the keyboard delivered
  to the game's own handlers, a window, and scripted keys and screenshots.
- It runs the intro, the title, the menus and the episode select, and enters a
  level with the status panel working. The playfield is not right yet.
- `tools/make_soundcrd.py`: "no sound card", as SETUP would set it up.
- `docs/lifting.md`.
