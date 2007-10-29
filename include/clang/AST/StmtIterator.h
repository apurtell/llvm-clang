//===--- StmtIterator.h - Iterators for Statements ------------------------===//
//
//                     The LLVM Compiler Infrastructure
//
// This file was developed by Ted Kremenek and is distributed under
// the University of Illinois Open Source License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
//
// This file defines the StmtIterator and ConstStmtIterator classes.
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_CLANG_AST_STMT_ITR_H
#define LLVM_CLANG_AST_STMT_ITR_H

#include "llvm/ADT/iterator"
#include <cassert>

namespace clang {

class Stmt;
class ScopedDecl;
class VariableArrayType;
  
class StmtIteratorBase {
protected:
  enum { DeclMode = 0x1 };
  union { Stmt** stmt; ScopedDecl* decl; };
  uintptr_t RawVAPtr;

  bool inDeclMode() const { 
    return RawVAPtr & DeclMode ? true : false;
  }
  
  VariableArrayType* getVAPtr() const {
    return reinterpret_cast<VariableArrayType*>(RawVAPtr & ~DeclMode);
  }
  
  void setVAPtr(VariableArrayType* P) {
    assert (inDeclMode());
    RawVAPtr = reinterpret_cast<uintptr_t>(P) | DeclMode;
  }
  
  void NextDecl(bool ImmediateAdvance = true);
  void NextVA();
  
  Stmt*& GetDeclExpr() const;

  StmtIteratorBase(Stmt** s) : stmt(s), RawVAPtr(0) {}
  StmtIteratorBase(ScopedDecl* d);
  StmtIteratorBase() : stmt(NULL), RawVAPtr(0) {}
};
  
  
template <typename DERIVED, typename REFERENCE>
class StmtIteratorImpl : public StmtIteratorBase, 
                         public std::iterator<std::forward_iterator_tag,
                                              REFERENCE, ptrdiff_t, 
                                              REFERENCE, REFERENCE> {  
protected:
  StmtIteratorImpl(const StmtIteratorBase& RHS) : StmtIteratorBase(RHS) {}
public:
  StmtIteratorImpl() {}                                                
  StmtIteratorImpl(Stmt** s) : StmtIteratorBase(s) {}
  StmtIteratorImpl(ScopedDecl* d) : StmtIteratorBase(d) {}

  
  DERIVED& operator++() {
    if (inDeclMode()) {
      if (getVAPtr()) NextVA();
      else NextDecl();
    }
    else ++stmt;
      
    return static_cast<DERIVED&>(*this);
  }
    
  DERIVED operator++(int) {
    DERIVED tmp = static_cast<DERIVED&>(*this);
    operator++();
    return tmp;
  }
  
  bool operator==(const DERIVED& RHS) const {
    return stmt == RHS.stmt && RawVAPtr == RHS.RawVAPtr;
  }
  
  bool operator!=(const DERIVED& RHS) const {
    return stmt != RHS.stmt || RawVAPtr != RHS.RawVAPtr;
  }
  
  REFERENCE operator*() const { 
    return (REFERENCE) (inDeclMode() ? GetDeclExpr() : *stmt);
  }
  
  REFERENCE operator->() const { return operator*(); }   
};

struct StmtIterator : public StmtIteratorImpl<StmtIterator,Stmt*&> {
  explicit StmtIterator() : StmtIteratorImpl<StmtIterator,Stmt*&>() {}
  StmtIterator(Stmt** S) : StmtIteratorImpl<StmtIterator,Stmt*&>(S) {}
  StmtIterator(ScopedDecl* D) : StmtIteratorImpl<StmtIterator,Stmt*&>(D) {}
};

struct ConstStmtIterator : public StmtIteratorImpl<ConstStmtIterator,
                                                   const Stmt*> {
  explicit ConstStmtIterator() : 
    StmtIteratorImpl<ConstStmtIterator,const Stmt*>() {}

  ConstStmtIterator(const StmtIterator& RHS) : 
    StmtIteratorImpl<ConstStmtIterator,const Stmt*>(RHS) {}
};

} // end namespace clang

#endif
