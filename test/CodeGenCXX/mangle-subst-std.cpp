// RUN: clang-cc -emit-llvm %s -o - -triple=x86_64-apple-darwin9 | FileCheck %s

namespace std {
  struct A { A(); };
  
  // CHECK: define void @_ZNSt1AC1Ev
  // CHECK: define void @_ZNSt1AC2Ev
  A::A() { }
};

namespace std {
  template<typename T> struct allocator { allocator(); };
}

// FIXME: typename is really not allowed here, but it's kept 
// as a workaround for PR5061.
// CHECK: define void @_Z1fSaIcESaIiE
void f(typename std::allocator<char>, typename std::allocator<int>) { }
