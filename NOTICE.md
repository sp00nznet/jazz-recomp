# Notice

The MIT license in [LICENSE](LICENSE) covers what is ours in this repository:
the DOS machine in `runtime/`, the setup scripts, the tools and the
documentation. It does not cover the game.

*Jazz Jackrabbit* is © 1994 Epic MegaGames (Epic Games), and is not ours to
license. Its executables, data, music and art are not in this repository and
are excluded by `.gitignore`.

- You need your own copy of the game to use any of this. Setup copies your CD
  into `game/`; the pipeline reads it from there.
- Lifted code is a derivative work of the binary it came from. Anything the
  pipeline writes into `work/` or `src/recomp/gen*/` carries whatever license
  the original carries, not this one. The MIT grant stops at our own work.
- `tools/make_soundcrd.py` writes "no sound card" files into `game/`, built
  from your copy's own driver list; they are not committed.

| Path | From | License |
|---|---|---|
| `runtime/vga.c` | the Blake Stone / Mario Is Missing DOS host, [marioismissing-dos](https://github.com/sp00nznet/marioismissing-dos) | MIT, © 2026 sp00nz (the same as this repository) |

The project layout and build come from the
[Space Bucks recomp](https://github.com/sp00nznet/spacebucks-recomp); the NE
front end and Win16 host are [pcrecomp](https://github.com/sp00nznet/pcrecomp)'s,
MIT, fetched by setup and not vendored here. There is no third-party code in
this repository.
