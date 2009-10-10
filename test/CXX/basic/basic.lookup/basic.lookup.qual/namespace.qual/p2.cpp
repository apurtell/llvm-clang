// RUN: clang-cc -fsyntax-only -verify %s

namespace Ints {
  int zero = 0; // expected-note {{candidate found by name lookup is 'Ints::zero'}}
  void f(int); // expected-note 3 {{candidate function}}
  void g(int);
}

namespace Floats {
  float zero = 0.0f; // expected-note {{candidate found by name lookup is 'Floats::zero'}}
  void f(float); // expected-note 3 {{candidate function}}
  void g(float);
}

namespace Numbers {
  using namespace Ints;
  using namespace Floats;
}

void test() {
  int i = Ints::zero;
  Ints::f(i);
  
  float f = Floats::zero;
  Floats::f(f);
  
  double n = Numbers::zero; // expected-error {{reference to 'zero' is ambiguous}}
  Numbers::f(n); // expected-error{{call to 'f' is ambiguous}}
  Numbers::f(i);
  Numbers::f(f);
}

namespace Numbers {
  struct Number {
    explicit Number(double d) : d(d) {}
    double d;
  };
  Number zero(0.0f);
  void g(Number);
}

void test2() {
  Numbers::Number n = Numbers::zero;
  Numbers::f(n); // expected-error {{no matching function for call to 'f'}}
  Numbers::g(n);
}

namespace Numbers2 {
  using Numbers::f;
  using Numbers::g;
}

void test3() {
  Numbers::Number n = Numbers::zero;
  Numbers2::f(n); // expected-error {{no matching function for call to 'f'}}
  Numbers2::g(n);

  int i = Ints::zero;
  Numbers2::f(i);
  Numbers2::g(i); // expected-error {{incompatible type passing 'int'}}

  float f = Floats::zero;
  Numbers2::f(f);
  Numbers2::g(f); // expected-error {{incompatible type passing 'float'}}
}
