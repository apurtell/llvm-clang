// RUNxX: %clang_cc1 -triple powerpc64le-unknown-linux-gnu -emit-llvm -o - %s | FileCheck %s --check-prefix=PPC
// RUN: %clang_cc1 -mfloat-abi hard -triple armv7-unknown-linux-gnu -emit-llvm -o - %s | FileCheck %s --check-prefix=ARM32
// RUN: %clang_cc1 -mfloat-abi hard -triple aarch64-unknown-linux-gnu -emit-llvm -o - %s | FileCheck %s --check-prefix=ARM64

// Test that C++ classes are correctly classified as homogeneous aggregates.

struct Base1 {
  int x;
};
struct Base2 {
  double x;
};
struct Base3 {
  double x;
};
struct D1 : Base1 {  // non-homogeneous aggregate
  double y, z;
};
struct D2 : Base2 {  // homogeneous aggregate
  double y, z;
};
struct D3 : Base1, Base2 {  // non-homogeneous aggregate
  double y, z;
};
struct D4 : Base2, Base3 {  // homogeneous aggregate
  double y, z;
};

struct I1 : Base2 {};
struct I2 : Base2 {};
struct I3 : Base2 {};
struct D5 : I1, I2, I3 {}; // homogeneous aggregate

// PPC: define void @_Z7func_D12D1(%struct.D1* noalias sret %agg.result, [3 x i64] %x.coerce)
// ARM32: define arm_aapcs_vfpcc void @_Z7func_D12D1(%struct.D1* noalias sret %agg.result, { [3 x i64] } %x.coerce)
// ARM64: define void @_Z7func_D12D1(%struct.D1* noalias sret %agg.result, %struct.D1* %x)
D1 func_D1(D1 x) { return x; }

// PPC: define [3 x double] @_Z7func_D22D2([3 x double] %x.coerce)
// ARM32: define arm_aapcs_vfpcc %struct.D2 @_Z7func_D22D2(%struct.D2 %x.coerce)
// ARM64: define %struct.D2 @_Z7func_D22D2(double %x.0, double %x.1, double %x.2)
D2 func_D2(D2 x) { return x; }

// PPC: define void @_Z7func_D32D3(%struct.D3* noalias sret %agg.result, [4 x i64] %x.coerce)
// ARM32: define arm_aapcs_vfpcc void @_Z7func_D32D3(%struct.D3* noalias sret %agg.result, { [4 x i64] } %x.coerce)
// ARM64: define void @_Z7func_D32D3(%struct.D3* noalias sret %agg.result, %struct.D3* %x)
D3 func_D3(D3 x) { return x; }

// PPC: define [4 x double] @_Z7func_D42D4([4 x double] %x.coerce)
// ARM32: define arm_aapcs_vfpcc %struct.D4 @_Z7func_D42D4(%struct.D4 %x.coerce)
// ARM64: define %struct.D4 @_Z7func_D42D4(double %x.0, double %x.1, double %x.2, double %x.3)
D4 func_D4(D4 x) { return x; }

D5 func_D5(D5 x) { return x; }
// PPC: define [3 x double] @_Z7func_D52D5([3 x double] %x.coerce)
// ARM32: define arm_aapcs_vfpcc %struct.D5 @_Z7func_D52D5(%struct.D5 %x.coerce)

// The C++ multiple inheritance expansion case is a little more complicated, so
// do some extra checking.
//
// ARM64-LABEL: define %struct.D5 @_Z7func_D52D5(double %x.0, double %x.1, double %x.2)
// ARM64: bitcast %struct.D5* %{{.*}} to %struct.I1*
// ARM64: bitcast %struct.I1* %{{.*}} to %struct.Base2*
// ARM64: getelementptr inbounds %struct.Base2* %{{.*}}, i32 0, i32 0
// ARM64: store double %x.0, double*
// ARM64: getelementptr inbounds i8* %{{.*}}, i64 8
// ARM64: getelementptr inbounds %struct.Base2* %{{.*}}, i32 0, i32 0
// ARM64: store double %x.1, double*
// ARM64: getelementptr inbounds i8* %{{.*}}, i64 16
// ARM64: getelementptr inbounds %struct.Base2* %{{.*}}, i32 0, i32 0
// ARM64: store double %x.2, double*

void call_D5(D5 *p) {
  func_D5(*p);
}

// Check the call site.
//
// ARM64-LABEL: define void @_Z7call_D5P2D5(%struct.D5* %p)
// ARM64: bitcast %struct.D5* %{{.*}} to %struct.I1*
// ARM64: bitcast %struct.I1* %{{.*}} to %struct.Base2*
// ARM64: getelementptr inbounds %struct.Base2* %{{.*}}, i32 0, i32 0
// ARM64: load double*
// ARM64: getelementptr inbounds i8* %{{.*}}, i64 8
// ARM64: bitcast i8* %{{.*}} to %struct.I2*
// ARM64: bitcast %struct.I2* %{{.*}} to %struct.Base2*
// ARM64: getelementptr inbounds %struct.Base2* %{{.*}}, i32 0, i32 0
// ARM64: load double*
// ARM64: getelementptr inbounds i8* %{{.*}}, i64 16
// ARM64: bitcast i8* %{{.*}} to %struct.I3*
// ARM64: bitcast %struct.I3* %{{.*}} to %struct.Base2*
// ARM64: getelementptr inbounds %struct.Base2* %{{.*}}, i32 0, i32 0
// ARM64: load double*
// ARM64: call %struct.D5 @_Z7func_D52D5(double %{{.*}}, double %{{.*}}, double %{{.*}})
