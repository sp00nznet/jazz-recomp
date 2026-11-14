# Testing Jazz Jackrabbit

There is no conformance harness yet ([ROADMAP.md](../ROADMAP.md)). Runs are
scripted with environment variables and checked from their screenshots:

```
set JAZZ_HEADLESS=1
set JAZZ_SECS=25
set JAZZ_SHOT=8:C:\tmp\intro.bmp,20:C:\tmp\title.bmp
build\jazz.exe
```

| Variable | |
|---|---|
| `JAZZ_HEADLESS=1` | no window (use it for every scripted run, so nothing takes a screen) |
| `JAZZ_SECS=N` | exit after N seconds |
| `JAZZ_SHOT="S:file.bmp,..."` | the screen at S seconds; paths must be absolute, because the game runs in `game\` |
| `JAZZ_KEYS="S:key,..."` | scripted keys at S seconds (`enter space esc up down left right ctrl alt y n f1..f10 1..9 a..z`); `key*N` holds it N seconds |
| `JAZZ_LOG=1` | every DPMI and DOS call |

## Record and replay

Keys at wall-clock seconds drift: a slow machine misses the menus. A
recording is keyed to the game's own clock instead, the IRQ0 ticks it has
been given (its PIT, about 70 a second), and under `--record`, `--replay` or
`f` keys time itself is virtual: every 500 loop back-edges of lifted code are
150 us (`JAZZ_VUS`), so tick N comes after the same work on any machine and
loading takes no time. Replays of one recording are bit-identical.

```
set PCRECOMP_MUTE=1
set JAZZ_HEADLESS=1
build\jazz.exe --replay tests\diamondus.rec
```

[tests/diamondus.rec](../tests/diamondus.rec) goes title, New Game, Medium,
Turtle Terror, into Diamondus 1, then right and fire. It writes `menu.bmp`,
`approaching.bmp`, `level.bmp` and `moved.bmp` in the current directory and
prints a hash per shot. Three replays, run side by side, gave the same four:
`F8CA98DD`, `109AC633`, `7E35AED6`, `50569821`.

To make a recording, drive a run with `JAZZ_KEYS`/`JAZZ_SHOT` as above and
add `--record FILE`: every key the game is given and every shot is written
as `TICK EVENT`, plus `end`. Shot paths can be edited to bare names (they
resolve against the directory the run started in). Keys can also be given in
ticks directly, `JAZZ_KEYS=f2162:enter,f3633:right*210` (`*N` holds N ticks)
or `--keys f2162:enter,...`.

| Line | |
|---|---|
| `TICK key SC down\|up` | a scan code (`e04d` = right) |
| `TICK NAME[*N]` | a press by name, held N ticks (default 3) |
| `TICK shot FILE` | a checkpoint: the screen as .bmp and its FNV hash |
| `TICK end` | exit |

Sound is off (the drivers are machine code loaded at run time), so runs are
silent. The milestones a harness will count are the intro, title, menu,
episode select and Diamondus 1.
