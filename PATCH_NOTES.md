# Patch Notes

## Fixed: the game's crash reporter (`stbc.RPT`)

When Bridge Commander crashes it's supposed to write a detailed `stbc.RPT` with
the time, the exception, the faulting address, a full register dump and a stack
trace. On modern Windows it instead produces a useless three-line stub, because
the reporter crashes while writing the report: the step that identifies which
module the fault came from reads memory without checking it's valid, so a crash
at a bad address (like a null-pointer call) makes the reporter fault a second
time and the game is killed before it finishes writing.

This patch makes that step crash-safe, so the original reporter runs to
completion again and `stbc.RPT` is genuinely useful. No extra files, just the
game's own report working the way it was meant to. (This one came first: with the
reporter broken, the crash below was invisible.)

## Fixed: crash during large battles

**Symptom.** The game would crash with an access violation during big fights, and
the bigger the battle the sooner it happened. Large fleet actions (e.g. 50-vs-50)
were almost guaranteed to crash given a little time; small skirmishes were
basically never affected.

**Cause.** Bridge Commander runs its ship AI and mission logic on an embedded
Python interpreter. Python keeps a *reference count* on every value and frees the
value when that count reaches zero. The shared `None` value is meant to exist for
the entire session and deliberately has no "free me" routine, because it should
never be freed. A scripting bug drops one reference too many on `None` in some
frequently-run code. One stray drop is harmless, but a big battle runs those
scripts so many times that `None`'s count is eventually pushed all the way to
zero, at which point the game tries to free it, calls a routine that isn't there,
and crashes instantly.

**Fix.** `None` is made immortal, the same approach later versions of Python
adopted for `None`/`True`/`False`. Its reference count is pinned high enough that
the imbalance can't reach zero in any realistic session, with a safety net that
restores it if it ever somehow does. Nothing else about the game's behavior
changes. The fix only applies when it recognizes the retail executable, so it
stays safely inert on anything it doesn't expect.
