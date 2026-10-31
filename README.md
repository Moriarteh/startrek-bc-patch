# Star Trek: Bridge Commander Community Patch

An unofficial, source-available patch for the original retail `stbc.exe` (2002).
It fixes long-standing bugs without touching the executable. Everything ships as
a drop-in proxy DLL.

> See [PATCH_NOTES.md](PATCH_NOTES.md) for what's fixed.

## How it works

`stbc.exe` statically imports `binkw32.dll` (the RAD Bink video runtime). The
patch masquerades as that DLL:

```
binkw32.dll        <- the patch (this project)
binkw32_orig.dll   <- the game's real Bink runtime, renamed
```

Every Bink export is forwarded straight through to `binkw32_orig.dll`, so movie
playback is untouched. Because `binkw32` loads before the game's entry point, the
patch's code runs first, which is the ideal point to install fixes.

## What's in the box

- **Bug fixes**, including a repair to the game's *own* crash reporter so
  `stbc.RPT` is useful again. See [PATCH_NOTES.md](PATCH_NOTES.md).

## Build

Needs the MSVC **x86** toolchain (`vcvars32.bat`). `stbc.exe` is 32-bit, so the
DLL must be too.

```
build.bat
```

Output: `dist\binkw32.dll`. The build auto-regenerates the export forwarders from
`src\binkw32_orig_ref.dll` (a local copy of the game's Bink DLL, not committed)
via `tools\gen_forwards.py`.

## Install

1. In the game folder, rename `binkw32.dll` to `binkw32_orig.dll`.
2. Copy `dist\binkw32.dll` into the game folder.
3. Run the game. After any crash, check `stbc.RPT` in the game folder.

To uninstall: delete the patch's `binkw32.dll` and rename `binkw32_orig.dll` back.

## What the patch writes to disk

On launch the patch creates a `crashlogs` folder next to the game and writes one
small text file there, `patch.log`. It lists which fixes were applied during that
launch (and, if the executable is not recognised, which were skipped and why). It
is rewritten from scratch every launch, so it never grows, and it holds no
personal information. The folder is safe to delete at any time; it is recreated on
the next launch.

The game's own `stbc.RPT` crash report is a separate file, written to the game
folder by the game itself, not by this patch.

## Status and contributions

This patch is provided as-is and is not actively maintained. Anyone is welcome to
fork it, open pull requests, or take it further. Contributions and changes are
welcome.
