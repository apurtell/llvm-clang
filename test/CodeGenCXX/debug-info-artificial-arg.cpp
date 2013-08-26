// RUN: %clang_cc1 -emit-llvm -g -triple x86_64-apple-darwin %s -o - | FileCheck %s

template<class X> class B {
public:
  explicit B(X* p = 0);
};

class A
{
public:
  A(int value) : m_a_value(value) {};
  A(int value, A* client_A) : m_a_value (value), m_client_A (client_A) {}

  virtual ~A() {}

private:
  int m_a_value;
  B<A> m_client_A;
};

int main(int argc, char **argv) {
  A reallyA (500);
}

// CHECK: ![[ARTARG:.*]] = {{.*}} ; [ DW_TAG_pointer_type ] [line 0, size 64, align 64, offset 0] [artificial] [from A]
// CHECK: ![[CLASSTYPE:.*]] = {{.*}} ; [ DW_TAG_class_type ] [A]
// CHECK: metadata ![[CLASSTYPE]], {{.*}} ; [ DW_TAG_subprogram ] [line 12] [A]
// CHECK: metadata [[FUNCTYPE:![0-9]*]], i32 0, null{{.*}}} ; [ DW_TAG_subroutine_type ]
// CHECK: [[FUNCTYPE]] = metadata !{null, metadata ![[ARTARG]], metadata !{{.*}}, metadata !{{.*}}}
