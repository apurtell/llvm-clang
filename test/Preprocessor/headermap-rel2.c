// This uses a headermap with this entry:
//   someheader.h -> Product/someheader.h

// RUN: %clang_cc1 -fsyntax-only %s -iquote %S/Inputs/headermap-rel2/project-headers.hmap -isysroot %S/Inputs/headermap-rel2/system -I %S/Inputs/headermap-rel2 -H 2> %t.out
// RUN: FileCheck %s -input-file %t.out

// CHECK: Product/someheader.h
// CHECK: system/usr/include/someheader.h
// CHECK: system/usr/include/someheader.h

#include "someheader.h"
#include <someheader.h>
#include <someheader.h>
