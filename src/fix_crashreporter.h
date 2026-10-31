#pragma once
// Fix #1: repair Bridge Commander's own crash reporter.
//
// BC's unhandled-exception filter writes a nice stbc.RPT (time, exception,
// fault address, registers, stack walk), except it dies on the very first
// thing it does with the fault address: a routine resolves which module a
// code address lives in by hand-parsing that module's PE header, with zero
// validation. For a null/garbage fault address (e.g. a call through a NULL
// pointer) the "module base" is 0 and it reads the null page -> a second crash
// *inside the crash handler*, so the report stops after three lines.
//
// That routine is replaced with a crash-safe version. Everything downstream of
// it already fails gracefully, so this one fix lets the original reporter run
// to completion, so no separate replacement handler is needed.

namespace crashfix {

// Detour that routine to a safe implementation. No-ops on an unexpected build.
void Install();

}  // namespace crashfix
