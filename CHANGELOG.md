# Changelog

All notable changes to this project are documented here. The format follows
[Keep a Changelog](https://keepachangelog.com/en/1.1.0/), and the project uses
[Semantic Versioning](https://semver.org/).

## [Unreleased]

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
