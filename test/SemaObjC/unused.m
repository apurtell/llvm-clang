// RUN: %clang_cc1 -verify -Wused-but-marked-unused -Wno-objc-protocol-method-implementation -Wunused -Wunused-parameter -fsyntax-only -Wno-objc-root-class %s

int printf(const char *, ...);

@interface Greeter
+ (void) hello;
@end

@implementation Greeter
+ (void) hello { printf("Hello, World!\n"); }
@end

int test1(void) {
  [Greeter hello];
  return 0;
}

@interface NSObject @end
@interface NSString : NSObject 
- (int)length;
@end

void test2() {
  @"pointless example call for test purposes".length; // expected-warning {{property access result unused - getters should not be used for side effects}}
}

@interface foo
- (int)meth: (int)x: (int)y: (int)z ;
@end

@implementation foo
- (int) meth: (int)x:  // expected-warning {{parameter name used as selector may result in incomplete method selector name}} \
                       // expected-note {{did you mean to use meth:Name2:Name3: as the selector name instead of meth:::}}
(int)y: // expected-warning{{unused}}  expected-warning {{parameter name used as selector may result in incomplete method selector name}}
(int) __attribute__((unused))z { return x; }
@end

//===------------------------------------------------------------------------===
// The next test shows how clang accepted attribute((unused)) on ObjC
// instance variables, which GCC does not.
//===------------------------------------------------------------------------===

#if __has_feature(attribute_objc_ivar_unused)
#define UNUSED_IVAR __attribute__((unused))
#else
#error __attribute__((unused)) not supported on ivars
#endif

@interface TestUnusedIvar {
  id y __attribute__((unused)); // no-warning
  id x UNUSED_IVAR; // no-warning
}
@end

// rdar://10777111
static NSString *x = @"hi"; // expected-warning {{unused variable 'x'}}

// rdar://12233989
@interface TestTransitiveUnused
- (void) a __attribute__((unused));
- (void) b __attribute__((unused));
@end

@interface TestTransitiveUnused(CAT)
@end

@implementation TestTransitiveUnused(CAT)
- (void) b {}
- (void) a { [self b]; }
@end
