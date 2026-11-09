"""Write game/SOUNDCRD.INF for "no sound card", as SETUP.EXE would, plus the
driver it names.

The game (seg004_3D31) reads the INF -- a driver's first 24 bytes, a length
byte, the driver's file name -- loads that driver, checks its first 24 bytes
against the INF, and checks that the name its header points to (dword at
+0x30) is not empty. Then seg002_551B compares the 8-byte ID at +12 with
NOSNDCRD and PCSPEAKR: NOSNDCRD turns sound off (DS:0A04 = 0) and the timer
handler then never calls into the driver.

The drivers are 16-bit machine code loaded at run time, which a static
recompile cannot run, so sound off is what this project wants. No shipped
driver carries the NOSNDCRD ID, so NOSOUND.MUS is MASI's PC-speaker driver
(MDRV002D.MUS) with that ID and a name. It is derived from your copy of the
game and stays in game/.
"""
import os

game = os.path.join(os.path.dirname(os.path.abspath(__file__)), '..', 'game')
drv = bytearray(open(os.path.join(game, 'MDRV002D.MUS'), 'rb').read())
drv[12:20] = b'NOSNDCRD'
name_at = int.from_bytes(drv[0x30:0x34], 'little')
drv[name_at:name_at + 9] = b'No Sound\0'
open(os.path.join(game, 'NOSOUND.MUS'), 'wb').write(drv)
name = b'NOSOUND.MUS'
open(os.path.join(game, 'SOUNDCRD.INF'), 'wb').write(bytes(drv[:24]) + bytes([len(name)]) + name)
print('wrote SOUNDCRD.INF and NOSOUND.MUS (no sound card)')
