<#
  setup.ps1 - the Quick start (README, Getting Started). Setup.cmd runs this.

  From a plain download to a "Jazz Jackrabbit" shortcut in this folder:
    1. check the tools (Python 3, IDA 9's idalib, Git, MSYS2's MinGW-w64 gcc
       with CMake and Ninja) and offer to install what can be installed
    2. copy your CD into game\ and set up "no sound card"
    3. fetch pcrecomp's win16-dos branch at the tested commit
    4. export IDA's code map and lift FILE0001.EXE to C (tools\relift.py --ida)
    5. build build\jazz.exe
    6. put a shortcut in this folder

  Every step is skipped when its result is already there (-Force redoes the
  lift), so running it again after a failure carries on. Details go to
  setup.log. It runs exactly the commands README "Step by step" lists.

  Nothing from the game is downloaded or bundled: the files come from your own
  CD, and everything generated from them stays in game\, work\ and build\.

    -GameDir <drive or folder>   the CD (E:\) or a copy of its root
    -Yes                         answer yes to every install question

  Under recomp-launcher (RECOMP_LAUNCHER=1) it never waits for Enter.
#>
param([string]$GameDir = "", [switch]$Yes, [switch]$Force)
$ErrorActionPreference = "Stop"
$Root = Split-Path $PSScriptRoot -Parent
Set-Location $Root
$Log = Join-Path $Root "setup.log"
"=== setup $(Get-Date -Format s)" | Out-File $Log -Encoding utf8

# The toolkit this game needs: pcrecomp's win16-dos branch (not merged into
# main yet), at the commit it was last lifted, built and run against.
$PcrecompUrl = "https://github.com/sp00nznet/pcrecomp.git"
$PcrecompBranch = "win16-dos"
$PcrecompRef = "9d73debe4b2e3d83676e1f00c11dd967df6341f8"
$Msys = "C:\msys64"

function Say($m) { Write-Host $m; $m | Out-File $Log -Append -Encoding utf8 }
function Fail($m) {
    Write-Host ""
    Write-Host "Setup stopped: $m" -ForegroundColor Red
    Write-Host "Details are in $Log. Fix that and run Setup.cmd again; finished steps are skipped."
    if (-not $Yes -and $env:RECOMP_LAUNCHER -ne "1") { Read-Host "Press Enter to close" | Out-Null }
    exit 1
}
trap { Fail "$_" }
function Run($what, [scriptblock]$cmd) {
    Say "  $what"
    # native tools write progress to stderr; under "Stop" PowerShell 5.1 would
    # turn every such line into a terminating error
    $ErrorActionPreference = "Continue"
    $out = & $cmd 2>&1
    $code = $LASTEXITCODE
    $ErrorActionPreference = "Stop"
    $out | Out-File $Log -Append -Encoding utf8
    if ($code -ne 0) { Fail "$what failed (exit code $code)." }
}
function Ask($q) {
    if ($Yes) { Say "$q yes"; return $true }
    $a = Read-Host "$q [y/N]"
    return $a -match '^(y|yes)$'
}
function Have($exe) { return [bool](Get-Command $exe -ErrorAction SilentlyContinue) }
function Refresh-Path { $env:Path = [Environment]::GetEnvironmentVariable("Path", "Machine") + ";" + [Environment]::GetEnvironmentVariable("Path", "User") }
function Winget($name, $id, $size) {
    if (-not (Have "winget")) { Fail "Install $name yourself (winget is not available), then run Setup again." }
    if (-not (Ask "Install $name now with winget ($size)?")) { Fail "Setup needs $name. Install it and run Setup again." }
    Run "installing $name" { winget install --id $id -e --accept-package-agreements --accept-source-agreements }
    Refresh-Path
}

Say "Jazz Jackrabbit (CD, v1.2): static recompilation setup"
Say "You need your own copy of the Jazz Jackrabbit CD (or a copy of its files), IDA 9 with idalib, and about 1 GB free."

# ---- 1. tools ---------------------------------------------------------------
Say "[1/6] Checking the tools"
# `python` may be the Microsoft Store alias that only opens the Store; the py
# launcher is what a python.org install puts on PATH.
if (-not (Have "py")) { Say "Missing: Python 3, which runs the lifter."; Winget "Python 3.12" "Python.Python.3.12" "about 30 MB" }
if (-not (Have "py")) { Fail "Python installed, but this window can't see it. Close it and run Setup.cmd again." }
if (-not (Have "git")) { Say "Missing: Git, which fetches the pcrecomp toolkit."; Winget "Git" "Git.Git" "about 60 MB" }

# IDA's code map is what the lift is seeded from today: without it the lifted
# game stops with Runtime error 105 (ROADMAP). IDA can't be installed for you.
$haveMap = Test-Path "work\ida\FILE0001.json"
if (-not $haveMap -or $Force) {
    $ErrorActionPreference = "Continue"
    & py -3.11 -c "import idapro" *>> $Log
    $idaOk = $LASTEXITCODE -eq 0
    $ErrorActionPreference = "Stop"
    if (-not $idaOk) { Fail "IDA 9's idalib is not usable from 'py -3.11' (import idapro). Install IDA 9 and its idalib Python package for Python 3.11, then run Setup again." }
    Say "  IDA idalib: py -3.11"
}

# MSYS2's MinGW-w64 gcc compiles it (the CMake file uses GNU flags), with
# MSYS2's CMake and Ninja.
if (-not (Test-Path "$Msys\usr\bin\bash.exe")) {
    Say "Missing: MSYS2, which provides the MinGW-w64 gcc that compiles the lifted C."
    Winget "MSYS2" "MSYS2.MSYS2" "about 100 MB"
    if (-not (Test-Path "$Msys\usr\bin\bash.exe")) { Fail "MSYS2 did not install to $Msys. Install it there yourself." }
}
$mingw = "$Msys\mingw64\bin"
if (-not ((Test-Path "$mingw\gcc.exe") -and (Test-Path "$mingw\cmake.exe") -and (Test-Path "$mingw\ninja.exe"))) {
    Say "Missing: MSYS2's mingw-w64-x86_64 gcc, cmake and ninja packages."
    if (-not (Ask "Install them now with pacman (about 300 MB)?")) { Fail "Setup needs MinGW-w64 gcc, CMake and Ninja from MSYS2." }
    Run "installing gcc, cmake and ninja (pacman)" { & "$Msys\usr\bin\bash.exe" -lc "pacman -S --noconfirm --needed mingw-w64-x86_64-gcc mingw-w64-x86_64-cmake mingw-w64-x86_64-ninja" }
}
$env:Path = "$mingw;$env:Path"
Say "  gcc: $mingw\gcc.exe"

# ---- 2. the game ------------------------------------------------------------------
Say "[2/6] Copying your game into game\"
$dest = Join-Path $Root "game"
# A folder is the game when FILE0001.EXE (the game; JAZZ.EXE only starts it) is in it.
function Game-Folder($p) { if ($p -and (Test-Path (Join-Path $p "FILE0001.EXE"))) { return $p }; return $null }
if (Test-Path (Join-Path $dest "FILE0001.EXE")) {
    Say "  already copied."
} else {
    $src = Game-Folder $GameDir
    if (-not $src) {
        foreach ($d in (Get-PSDrive -PSProvider FileSystem)) { $src = Game-Folder $d.Root; if ($src) { break } }
    }
    if (-not $src -and -not $Yes -and $env:RECOMP_LAUNCHER -ne "1") {
        $src = Game-Folder (Read-Host "Where is the game? (the CD's drive, or a folder holding FILE0001.EXE)")
    }
    if (-not $src) { Fail "FILE0001.EXE was not found. Insert the Jazz Jackrabbit CD (or mount its image) and run Setup again, or give -GameDir <folder holding FILE0001.EXE>." }
    Say "  copying from $src"
    New-Item -ItemType Directory -Force $dest | Out-Null
    Copy-Item -Path (Join-Path $src "*") -Destination $dest -Recurse -Force
    # a CD's files arrive read-only, and the game writes its settings beside them
    Get-ChildItem $dest -Recurse -File | ForEach-Object { $_.IsReadOnly = $false }
}
if (-not (Test-Path (Join-Path $dest "SOUNDCRD.INF"))) {
    Run "py -3 tools\make_soundcrd.py (no sound card)" { py -3 tools\make_soundcrd.py }
}

# ---- 3. the toolkit ----------------------------------------------------------------
Say "[3/6] Fetching the pcrecomp toolkit ($PcrecompBranch)"
function Has-Dos($p) { return (Test-Path "$p\runtime\win16\cpu.h") -and (Select-String -Path "$p\runtime\win16\cpu.h" -Pattern "DOS-extended" -SimpleMatch -Quiet) }
$parent = Split-Path $Root -Parent
if ($env:PCRECOMP_DIR) {
    $pcr = $env:PCRECOMP_DIR
    if (-not (Has-Dos $pcr)) { Fail "PCRECOMP_DIR ($pcr) is not on pcrecomp's $PcrecompBranch branch. Check that branch out there, or unset PCRECOMP_DIR." }
} elseif (Has-Dos "$parent\pcrecomp") {
    $pcr = "$parent\pcrecomp"
} else {
    # never switch someone's ..\pcrecomp to another branch: a checkout of its own
    $pcr = "$parent\pcrecomp-$PcrecompBranch"
    if (-not (Has-Dos $pcr)) {
        Run "fetching pcrecomp into $pcr (about 20 MB)" { git clone --quiet --branch $PcrecompBranch $PcrecompUrl $pcr }
        Run "checking out pcrecomp $($PcrecompRef.Substring(0, 7))" { git -C $pcr checkout --quiet $PcrecompRef }
    }
}
$pcr = (Resolve-Path $pcr).Path
$env:PCRECOMP_DIR = $pcr
Say "  pcrecomp: $pcr"

# ---- 4. lift ---------------------------------------------------------------------
Say "[4/6] Lifting FILE0001.EXE to C"
if ((Test-Path "work\src\_dispatch.c") -and (Test-Path "work\mem_image.bin") -and -not $Force) {
    Say "  already lifted (work\)."
} elseif ($haveMap -and -not $Force) {
    Run "py -3 tools\relift.py (with the IDA map in work\ida)" { py -3 tools\relift.py }
} else {
    Run "py -3 tools\relift.py --ida (IDA's code map, then the lift)" { py -3 tools\relift.py --ida }
}

# ---- 5. build ----------------------------------------------------------------------
Say "[5/6] Building (a minute or two)"
Run "configuring" { cmake -S . -B build -G Ninja "-DCMAKE_C_COMPILER=$mingw\gcc.exe" "-DPCRECOMP_DIR=$pcr" }
Run "compiling" { cmake --build build }
$exe = Join-Path $Root "build\jazz.exe"
if (-not (Test-Path $exe)) { Fail "The build finished but $exe is missing." }

# ---- 6. shortcut -------------------------------------------------------------------
Say "[6/6] Making the shortcut"
# jazz.exe needs only Windows' own DLLs, and finds game\ and work\ by the
# paths compiled into it
$lnk = (New-Object -ComObject WScript.Shell).CreateShortcut((Join-Path $Root "Jazz Jackrabbit.lnk"))
$lnk.TargetPath = $exe
$lnk.WorkingDirectory = $Root
$lnk.Save()
Say "Done. Double-click 'Jazz Jackrabbit' in this folder to play. The first run opens the game's options screen; pick DONE."
if (-not $Yes -and $env:RECOMP_LAUNCHER -ne "1") { Read-Host "Press Enter to close" | Out-Null }
