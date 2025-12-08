"""This module defines a simple C++ function using cppyy."""
import cppyy

cppyy.cppdef("""\
namespace test {
  int add(int i, int j) { return i + j; }
}""")

add = cppyy.gbl.test.add
