// This used to produce an internal error (noted in #14167)
proc f()       { return 0; }
proc g() param { return 1; }

class Parent {
  proc method(arg:int = f()) {
    writeln("in Parent.method arg=", arg);
  }
}

class Child : Parent {
  override proc method(arg:int = g()) {
    writeln("in Child.method arg=", arg);
  }
}

proc main() {
  var x:Parent = new Child();
  x.method();
  //   run child's default (virtually dispatched) `g()` and so print out `in Child.method arg=1`

  var y = new Child();
  y.method(); // expect this to run `g` and so print `in Child.method arg=1`

  var z = new Parent();
  z.method();
}
