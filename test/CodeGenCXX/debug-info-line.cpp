// RUN: %clang_cc1 -gline-tables-only -std=c++11 -fexceptions -fcxx-exceptions -S -emit-llvm %s -o - | FileCheck %s
// RUN: %clang_cc1 -gline-tables-only -std=c++11 -fexceptions -fcxx-exceptions -S -emit-llvm %s -o - -triple i686-linux-gnu | FileCheck %s

// XFAIL: win32

int &src();
int *sink();
extern "C" __complex float complex_src();
extern "C" __complex float *complex_sink();

// CHECK-LABEL: define
void f1() {
#line 100
  * // The store for the assignment should be attributed to the start of the
      // assignment expression here, regardless of the location of subexpressions.
      sink() = src();
  // CHECK: store {{.*}}, !dbg [[DBG_F1:!.*]]
}

struct foo {
  int i;
  int &j;
  __complex float k;
  foo();
};

// CHECK-LABEL: define
foo::foo()
    :
#line 200
      i // CHECK: store i32 {{.*}} !dbg [[DBG_FOO_VALUE:!.*]]
      (src()),
      j // CHECK: store i32* {{.*}} !dbg [[DBG_FOO_REF:!.*]]
      (src()),
      k // CHECK: store float {{.*}} !dbg [[DBG_FOO_COMPLEX:!.*]]
      (complex_src()) {
}

// CHECK-LABEL: define {{.*}}f2{{.*}}
void f2() {
#line 300
  * // CHECK: store float {{.*}} !dbg [[DBG_F2:!.*]]
      complex_sink() = complex_src();
}

// CHECK-LABEL: define
void f3() {
#line 400
  * // CHECK: store float {{.*}} !dbg [[DBG_F3:!.*]]
      complex_sink() += complex_src();
}

// CHECK-LABEL: define
void f4() {
#line 500
  auto x // CHECK: store {{.*}} !dbg [[DBG_F4:!.*]]
      = src();
}

// CHECK-LABEL: define
void f5() {
#line 600
  auto x // CHECK: store float {{.*}} !dbg [[DBG_F5:!.*]]
      = complex_src();
}

struct agg { int i; };
agg agg_src();

// CHECK-LABEL: define
void f6() {
  agg x;
#line 700
  x // CHECK: call void @llvm.memcpy{{.*}} !dbg [[DBG_F6:!.*]]
      = agg_src();
}

// CHECK-LABEL: define
void f7() {
  int *src1();
  int src2();
#line 800
  int x = ( // CHECK: load {{.*}} !dbg [[DBG_F7:!.*]]
      src1())[src2()];
}

// CHECK-LABEL: define
void f8() {
  int src1[1];
  int src2();
#line 900
  int x = ( // CHECK: load {{.*}} !dbg [[DBG_F8:!.*]]
      src1)[src2()];
}

// CHECK-LABEL: define
void f9(int i) {
  int src1[1][i];
  int src2();
#line 1000
  auto x = ( // CHECK: getelementptr {{.*}} !dbg [[DBG_F9:!.*]]
      src1)[src2()];
}

inline void *operator new(decltype(sizeof(1)), void *p) noexcept { return p; }

// CHECK-LABEL: define
void f10() {
  void *void_src();
  ( // CHECK: icmp {{.*}} !dbg [[DBG_F10_ICMP:.*]]
    // CHECK: store {{.*}} !dbg [[DBG_F10_STORE:!.*]]
#line 1100
      new (void_src()) int(src()));
}

// noexcept just to simplify the codegen a bit
void fn() noexcept(true);

struct bar {
  bar();
  // noexcept(false) to convolute the global dtor
  ~bar() noexcept(false);
};
// global ctor cleanup
// CHECK-LABEL: define
// CHECK: invoke{{ }}
// CHECK: invoke{{ }}
// CHECK:   to label {{.*}}, !dbg [[DBG_GLBL_CTOR_B:!.*]]

// terminate caller
// CHECK-LABEL: define

// global dtor cleanup
// CHECK-LABEL: define
// CHECK: invoke{{ }}
// CHECK: invoke{{ }}
// CHECK:   to label {{.*}}, !dbg [[DBG_GLBL_DTOR_B:!.*]]
#line 1500
bar b[1] = { //
    (fn(),   //
     bar())};

// CHECK-LABEL: define
__complex double f11() {
  __complex double f;
// CHECK: store {{.*}} !dbg [[DBG_F11:!.*]]
#line 1200
  return f;
}

// CHECK-LABEL: define
void f12() {
  int f12_1();
  void f12_2(int = f12_1());
// CHECK: call {{(signext )?}}i32 {{.*}} !dbg [[DBG_F12:!.*]]
#line 1300
  f12_2();
}

// CHECK-LABEL: define
void f13() {
// CHECK: call {{.*}} !dbg [[DBG_F13:!.*]]
#define F13_IMPL 1, src()
  1,
#line 1400
  F13_IMPL;
}

// CHECK: [[DBG_F1]] = !{i32 100,
// CHECK: [[DBG_FOO_VALUE]] = !{i32 200,
// CHECK: [[DBG_FOO_REF]] = !{i32 202,
// CHECK: [[DBG_FOO_COMPLEX]] = !{i32 204,
// CHECK: [[DBG_F2]] = !{i32 300,
// CHECK: [[DBG_F3]] = !{i32 400,
// CHECK: [[DBG_F4]] = !{i32 500,
// CHECK: [[DBG_F5]] = !{i32 600,
// CHECK: [[DBG_F6]] = !{i32 700,
// CHECK: [[DBG_F7]] = !{i32 800,
// CHECK: [[DBG_F8]] = !{i32 900,
// CHECK: [[DBG_F9]] = !{i32 1000,
// CHECK: [[DBG_F10_ICMP]] = !{i32 1100,
// CHECK: [[DBG_F10_STORE]] = !{i32 1100,
// CHECK: [[DBG_GLBL_CTOR_B]] = !{i32 1500,
// CHECK: [[DBG_GLBL_DTOR_B]] = !{i32 1500,
// CHECK: [[DBG_F11]] = !{i32 1200,
// CHECK: [[DBG_F12]] = !{i32 1300,
// CHECK: [[DBG_F13]] = !{i32 1400,
