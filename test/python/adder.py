import cppyy

cppyy.cppdef("""\
namespace test {
  int add(int i, int j) { return i + j; }
}""")

add = cppyy.gbl.test.add

