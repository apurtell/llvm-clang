// RUN: %clang_cc1 -std=c++98 %s -Wdeprecated -verify
// RUN: %clang_cc1 -std=c++11 %s -Wdeprecated -verify
// RUN: %clang_cc1 -std=c++1y %s -Wdeprecated -verify

#include "Inputs/register.h"

void f() throw();
void g() throw(int);
void h() throw(...);
#if __cplusplus >= 201103L
// expected-warning@-4 {{dynamic exception specifications are deprecated}} expected-note@-4 {{use 'noexcept' instead}}
// expected-warning@-4 {{dynamic exception specifications are deprecated}} expected-note@-4 {{use 'noexcept(false)' instead}}
// expected-warning@-4 {{dynamic exception specifications are deprecated}} expected-note@-4 {{use 'noexcept(false)' instead}}
#endif

void stuff() {
  register int n;
#if __cplusplus >= 201103L
  // expected-warning@-2 {{'register' storage class specifier is deprecated}}
#endif

  int k = to_int(n); // no-warning

  bool b;
  ++b; // expected-warning {{incrementing expression of type bool is deprecated}}

  // FIXME: This is ill-formed in C++11.
  char *p = "foo"; // expected-warning {{conversion from string literal to 'char *' is deprecated}}
}

struct S { int n; };
struct T : private S {
  S::n;
#if __cplusplus < 201103L
  // expected-warning@-2 {{access declarations are deprecated; use using declarations instead}}
#else
  // expected-error@-4 {{ISO C++11 does not allow access declarations; use using declarations instead}}
#endif
};

#if __cplusplus >= 201103L
namespace DeprecatedCopy {
  struct Assign {
    Assign &operator=(const Assign&); // expected-warning {{definition of implicit copy constructor for 'Assign' is deprecated because it has a user-declared copy assignment operator}}
  };
  Assign a1, a2(a1); // expected-note {{implicit copy constructor for 'Assign' first required here}}

  struct Ctor {
    Ctor();
    Ctor(const Ctor&); // expected-warning {{definition of implicit copy assignment operator for 'Ctor' is deprecated because it has a user-declared copy constructor}}
  };
  Ctor b1, b2;
  void f() { b1 = b2; } // expected-note {{implicit copy assignment operator for 'Ctor' first required here}}

  struct Dtor {
    ~Dtor();
    // expected-warning@-1 {{definition of implicit copy constructor for 'Dtor' is deprecated because it has a user-declared destructor}}
    // expected-warning@-2 {{definition of implicit copy assignment operator for 'Dtor' is deprecated because it has a user-declared destructor}}
  };
  Dtor c1, c2(c1); // expected-note {{implicit copy constructor for 'Dtor' first required here}}
  void g() { c1 = c2; } // expected-note {{implicit copy assignment operator for 'Dtor' first required here}}
}
#endif
