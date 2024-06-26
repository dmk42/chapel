config const cond = false;
config const otherCond = true;

class C { }
record R {
  var x: int = 0;
  var ptr: owned C = new owned C();
  proc init() {
    this.x = 0;
    writeln("init");
  }
  proc init(arg:int) {
    this.x = arg;
    writeln("init ", arg);
  }
  proc init=(other: R) {
    this.x = other.x;
    writeln("init= ", other.x);
  }
}
operator R.=(ref lhs:R, rhs:R) {
  writeln("= ", lhs.x, " ", rhs.x);
  lhs.x = rhs.x;
}

proc test100() {
  writeln("test100");
  var b;
  sync {
    b = new R(1);
  }
  writeln(b);
}
test100();

proc test101() {
  writeln("test101");
  var b: R;
  sync {
    b = new R(1);
  }
  writeln(b);
}
test101();
