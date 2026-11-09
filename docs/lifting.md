# Lifting Jazz Jackrabbit: what it took

`FILE0001.EXE` is an NE program for Borland's RTM: 10 code segments, imports
from `KERNEL` (memory, selectors, and the selectors for real-mode memory) and
three from `RTM` itself. pcrecomp's `ne_link` lifts it with an IDA code map,
the same way as Space Bucks. pcrecomp's Win16 host loads it and runs `InitTask`
and the rest of KERNEL. Everything a DOS game does that a Windows program
doesn't is in `runtime/jazz.c` and `runtime/vga.c`. The toolkit half is
pcrecomp's `win16-dos` branch.

## 1. What IDA could not name

The executable says its target is OS/2, so IDA names none of its imports.
pcrecomp's fallback table now names the rest (GlobalHandle, GetWinFlags, the
selector calls, GlobalDOSAlloc/Free). It also had **GlobalFree and
GlobalReAlloc swapped** (KERNEL 17 is GlobalFree). Borland Pascal's heap
frees with `GlobalHandle`, `GlobalUnlock(h)`, `GlobalFree(h)`, one argument.

`__A000H`, `__B800H`, `__0040H` and the like are constants the loader patches
in. They become selectors numbered like the paragraph (0xA000, ...). The
runtime points them into a 1 MB block of conventional memory, and A000-AFFF
goes to the VGA through `RECOMP_MEM_HOOK`.

RTM's three exports are Borland Pascal's heap: 5 allocates (14 bytes of
arguments, the selector returned through a far pointer), 6 frees, 21 returns
the largest free block.

## 2. DOS on the Win16 CPU

pcrecomp's Win16 CPU header now has three opt-ins for a program like this:
- `RECOMP_PORT_IO`: real `in`/`out`;
- `RECOMP_MEM_HOOK`: planar video memory;
- `RECOMP_IRQ`, which ticks at function entry as well as at loop back-edges.
  IDA splits a wait loop into functions that tail-call each other, and such a
  loop has no back-edge.

The game installs its timer and keyboard handlers with DOS's set-vector call
(INT 21h AH=25h), which under DPMI sets the protected-mode vector. The host
calls them from those ticks, saving and restoring the whole register file,
because the lifted `iret` only returns. The timer handler was never called
directly, so IDA did not find it: `tools/entry_seeds.json` adds it.

## 3. Borland Pascal's runtime

- **The CRT unit's delay loop** counts loops per BIOS tick and divides by
  55. Native code is far too fast for the 16-bit quotient. That is Borland's
  "Runtime error 200" on fast CPUs: DelayCnt is set to its maximum after the
  calibration.
- **The snow check** polls the CRTC status port it finds in the BIOS data area
  (0040:0063). The runtime fills that area in as a VGA BIOS would.
- **`Dos.Intr`** patches the interrupt number into its own `int 00`, through a
  data alias of its code segment. The lifted code still says `int 0`, so the
  runtime reads the number back from where the patch went. The same routine
  checks DS and ES with `VERR`, which pcrecomp's 16-bit decoder did not know.
  `0F 00 E0` became `add al,ah`, and ES got loaded from the sum.

## 4. Sound off

The game loads a MASI sound driver named in `SOUNDCRD.INF`, which SETUP
writes. The driver is 16-bit machine code loaded at run time, so it cannot be
lifted. The game checks the driver against the INF and then compares its ID
with `NOSNDCRD`, which turns sound off, but no shipped driver carries that ID.
`tools/make_soundcrd.py` writes `NOSOUND.MUS`: the PC-speaker driver with that
ID and a name. With sound off, the timer handler never calls into a driver.
The music engine (`MDRV000D.MUS`) is still loaded and called three times at
start. The host skips those calls.

## Next: the playfield

In a level, video memory holds the status panel, then three playfield pages
and a tile cache, and the panel is a split screen (line compare). The start
address begins at the top of page 1, then slides 97 rows down over ten frames
and stays there. So each frame shows half of one page and half of the next,
and most foreground tiles are missing. Next: the camera and the tile blitter,
which copies from the tile cache with the latches.
