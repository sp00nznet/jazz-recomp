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

Sound is off (the drivers are machine code loaded at run time), so runs are
silent. The milestones a harness will count are the intro, title, menu,
episode select and Diamondus 1.
