// RUN: clang -emit-llvm %s -o %t && 

@class C;

// RUN: grep _Z1fP11objc_object %t | count 1 && 
void __attribute__((overloadable)) f(C *c) { }

// RUN: grep _Z1fP1C | count 1
void __attribute__((overloadable)) f(id c) { }
