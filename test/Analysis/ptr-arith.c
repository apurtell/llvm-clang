// RUN: clang -analyze -checker-simple -analyzer-store=region -verify %s

void f1() {
  int a[10];
  int *p = a;
  ++p;
}

char* foo();

void f2() {
  char *p = foo();
  ++p;
}
