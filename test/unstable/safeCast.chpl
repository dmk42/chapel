var i1 : int =  35;
var i2 : int(16) = 32;
var i3 : int(8) = 16;
var i4 : int = 1;
writeln(i1.safeCast(int));
writeln(i1.safeCast(uint));
writeln(i2.safeCast(uint(32)));
writeln(i3.safeCast(uint(8)));
writeln(i4.safeCast(bool));
var u1 : uint = 43;
var u2 : uint(16) = 4;
var u3 : uint(8) = 32;
var u4 : uint = 0;
writeln(u1.safeCast(uint));
writeln(u1.safeCast(int));
writeln(u2.safeCast(int(32)));
writeln(u3.safeCast(int(8)));
writeln(u4.safeCast(bool));
var b = true;
writeln(b.safeCast(uint(32)));
writeln(b.safeCast(int));
writeln(b.safeCast(bool));
