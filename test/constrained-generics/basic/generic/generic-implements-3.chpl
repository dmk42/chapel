/*
This is generic-implements-2.chpl
modified to hide the implementations in a module
so that the needed implements statements cannot be inferred implicitly.
*/

//-----------

module Lib {

record MyRec {
  var   myVar;
  type  myType;
  param myParam;
  proc  reqMethod(): void {
    writeln("  MyRec.reqMethod");
    writeln("    this    = ", this, " : ", this.type:string);
  }
}

private
proc reqFun(reqArg: MyRec(?)): void {
  writeln("  reqFun.MyRec");
  writeln("    reqArg  = ", reqArg, " : ", reqArg.type:string);
}

//-----------

record NotherRec {
  param myParam;
  type  myType;
  var   myVar;
  proc  reqMethod(): void {
    writeln("  NotherRec.reqMethod");
    writeln("    this    = ", this, " : ", this.type:string);
  }
  // see also reqFun/1 and reqFun/2 below
}

// no proc reqFun(reqArg: NotherRec)

//-----------

private
proc reqFun(reqArg1: MyRec(?), reqArg2: MyRec(?)): void {
  writeln("  reqFun.MyRec.MyRec");
  writeln("    reqArg1 = ", reqArg1, " : ", reqArg1.type:string);
  writeln("    reqArg2 = ", reqArg2, " : ", reqArg2.type:string);
}

private
proc reqFun(reqArg1: MyRec(?), reqArg2: NotherRec(?)): void {
  writeln("  reqFun.MyRec.NotherRec");
  writeln("    reqArg1 = ", reqArg1, " : ", reqArg1.type:string);
  writeln("    reqArg2 = ", reqArg2, " : ", reqArg2.type:string);
}

private
proc reqFun(reqArg1: NotherRec(?), reqArg2: MyRec(?)): void {
  writeln("  reqFun.NotherRec.MyRec");
  writeln("    reqArg1 = ", reqArg1, " : ", reqArg1.type:string);
  writeln("    reqArg2 = ", reqArg2, " : ", reqArg2.type:string);
}

//-----------

interface IFC1 {
  proc reqFun(reqFormal: Self);
  proc Self.reqMethod();
  // associated types are not tested here
}

proc cgFun(cgArg: ?Q) where Q implements IFC1 {
  writeln();
  writeln("cgFun.IFC1");
  reqFun(cgArg);
  cgArg.reqMethod();
}

MyRec implements IFC1;

//would be incorrect:
// NotherRec implements IFC1;

//-----------

interface IFC2(T1, T2) {
  proc reqFun(reqFormal1: T1, reqFormal2: T2);
  proc T1.reqMethod();
  proc T2.reqMethod();
  // associated types are currently disallowed for multi-arg interfaces
}

proc cgFun(cgArg1: ?Q1, cgArg2: ?Q2) where implements IFC2(Q1, Q2) {
  writeln();
  writeln("cgFun.IFC2");
  reqFun(cgArg1, cgArg2);
  cgArg1.reqMethod();
  cgArg2.reqMethod();
}

implements IFC2(MyRec, MyRec);
implements IFC2(MyRec, NotherRec);
implements IFC2(NotherRec, MyRec);

} // module Lib

//-----------

module App {
use Lib;

var myInst1 = new MyRec(11, real, 111);
var myInst2 = new MyRec("hi", int, false);
var myInst3 = new NotherRec(1.78, string, "by");
writeln("myInst1 = ", myInst1);
writeln("myInst2 = ", myInst2);
writeln("myInst3 = ", myInst3);

proc main {
  cgFun(myInst1);
  cgFun(myInst2);
  //would be incorrect: cgFun(myInst3);

  cgFun(myInst1, myInst2);
  cgFun(myInst2, myInst3);
  cgFun(myInst3, myInst1);

  writeln();
  writeln("done");
}
}
