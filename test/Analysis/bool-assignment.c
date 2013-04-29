// RUN: %clang_cc1 -analyze -analyzer-checker=core,alpha.core.BoolAssignment -analyzer-store=region -verify -std=c99 -Dbool=_Bool %s
// RUN: %clang_cc1 -analyze -analyzer-checker=core,alpha.core.BoolAssignment -analyzer-store=region -verify -x c++ %s

// Test C++'s bool and C's _Bool

void test_stdbool_initialization(int y) {
  if (y < 0) {
    bool x = y; // expected-warning {{Assignment of a non-Boolean value}}
    return;
  }
  if (y > 1) {
    bool x = y; // expected-warning {{Assignment of a non-Boolean value}}
    return;
  }
  bool x = y; // no-warning
}

void test_stdbool_assignment(int y) {
  bool x = 0; // no-warning
  if (y < 0) {
    x = y; // expected-warning {{Assignment of a non-Boolean value}}
    return;
  }
  if (y > 1) {
    x = y; // expected-warning {{Assignment of a non-Boolean value}}
    return;
  }
  x = y; // no-warning
}

// Test Objective-C's BOOL

typedef signed char BOOL;

void test_BOOL_initialization(int y) {
  if (y < 0) {
    BOOL x = y; // expected-warning {{Assignment of a non-Boolean value}}
    return;
  }
  if (y > 1) {
    BOOL x = y; // expected-warning {{Assignment of a non-Boolean value}}
    return;
  }
  BOOL x = y; // no-warning
}

void test_BOOL_assignment(int y) {
  BOOL x = 0; // no-warning
  if (y < 0) {
    x = y; // expected-warning {{Assignment of a non-Boolean value}}
    return;
  }
  if (y > 1) {
    x = y; // expected-warning {{Assignment of a non-Boolean value}}
    return;
  }
  x = y; // no-warning
}


// Test MacTypes.h's Boolean

typedef unsigned char Boolean;

void test_Boolean_initialization(int y) {
  if (y < 0) {
    Boolean x = y; // expected-warning {{Assignment of a non-Boolean value}}
    return;
  }
  if (y > 1) {
    Boolean x = y; // expected-warning {{Assignment of a non-Boolean value}}
    return;
  }
  Boolean x = y; // no-warning
}

void test_Boolean_assignment(int y) {
  Boolean x = 0; // no-warning
  if (y < 0) {
    x = y; // expected-warning {{Assignment of a non-Boolean value}}
    return;
  }
  if (y > 1) {
    x = y; // expected-warning {{Assignment of a non-Boolean value}}
    return;
  }
  x = y; // no-warning
}
