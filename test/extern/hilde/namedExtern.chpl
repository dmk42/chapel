// namedExtern.chpl
//
// Tests the ability to supply a backend (C) name for an extern
// as well as a Chapel name.
// The name following "extern" is the name used by the backend.
// The name following "proc" is the name used by Chapel.
//
use CTypes;
extern "printf" proc cprintf(fmt: c_ptrConst(c_char)): int;
extern "printf" proc cprintf(fmt: c_ptrConst(c_char), arg...): int;

var i = 19: int(32); // using an int(32) to up the odds this works with %d
var f = 63.0;
var s: c_ptrConst(c_char) = "pwh";

cprintf("%d %f %s\n", i, f, s);
cprintf("Done.\n");
