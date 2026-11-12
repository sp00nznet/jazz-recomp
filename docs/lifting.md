# Lifting Jazz Jackrabbit: what it took

`FILE0001.EXE` is an NE program for Borland's RTM: 10 code segments, imports
from `KERNEL` (memory, selectors, and the selectors for real-mode memory) and
three from `RTM` itself. pcrecomp's `ne_link` lifts it with its own code map (`ne_codemap.py`; IDA's at first),
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

## 5. Compiled sprites

In a level, Jazz, the enemies and the masked tiles are compiled sprites: x86
the game generates into data blocks and calls far, with DS:SI at the screen
spot. Each is a run of `mov [si+d],imm`, `add si,imm` per row, and a plane
step (`rol al,1; adc di,0; mov si,di; out dx,al`), then `retf`. Nothing was
lifted from there, so those far calls missed and were dropped: no Jazz, no
enemies. pcrecomp's Win16 host now offers a weak `recomp_dispatch_miss_hook`,
and `runtime/jazz.c` defines it as a small interpreter for exactly those
instructions (a call whose first byte is not a `mov` is left alone, so the
sound driver stubs keep being skipped). An unknown opcode is logged as one
`[x86]` line and the sprite returns.

The camera that "settled mid-page" was fine: with the sprites drawn the
playfield scrolls normally.

## 6. The native code map

The IDA-free code map misses six functions IDA found; without them the game
stops with Runtime error 105 before the intro. `tools/entry_seeds.json`
seeds them.
