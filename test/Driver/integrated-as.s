// RUN: %clang -### -c -integrated-as %s 2>&1 | FileCheck %s

// CHECK: cc1as
// CHECK-NOT: -relax-all
