#pragma once
// Fix for the big-battle crash: Python 1.5.2's Py_None has a NULL tp_dealloc
// (the CPython source even comments it "never called"). BC leaks a reference on
// None somewhere in its script paths; in a big battle that leak accumulates
// until None's refcount hits 0, Py_DECREF calls the NULL destructor, and the
// game jumps to address 0. None is made immortal, exactly like modern CPython.

namespace pyfix {

// Pin Py_None's refcount and install a safe backstop destructor. Verifies the
// known signature first, so it quietly does nothing on an unexpected build.
// Returns true if the patch was applied.
bool ImmortalizeNone();

}  // namespace pyfix
