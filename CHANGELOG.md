# Changelog

All notable changes to this project are documented here. The format follows
[Keep a Changelog](https://keepachangelog.com/en/1.1.0/), and the project uses
[Semantic Versioning](https://semver.org/).

## [Unreleased]

### Added
- `Setup.cmd` (`tools/setup.ps1`): checks for Python, Git, IDA's idalib and
  MSYS2's MinGW-w64 gcc, CMake and Ninja, offering to install what can be
  installed; copies the CD into `game\`, sets up "no sound card", fetches
  pcrecomp's `win16-dos` branch at the tested commit, exports the IDA map,
  lifts, builds and leaves a shortcut. Tested from a clean clone.
- `launcher/launcher.json` for recomp-launcher, and an installer workflow
  (manual only while the repo is private).
- A recomp-netlab recipe (`jazz-recomp`, mingw builder; a compile check).

### Changed
- README rewritten in the pcrecomp house style: status table, Quick start
  and Step by step, Usage, Building from source. The prerequisites no longer
  list `capstone`, which nothing in the lift imports.
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
