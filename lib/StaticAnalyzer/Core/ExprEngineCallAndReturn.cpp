//=-- ExprEngineCallAndReturn.cpp - Support for call/return -----*- C++ -*-===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
//
//  This file defines ExprEngine's support for calls and returns.
//
//===----------------------------------------------------------------------===//

#include "clang/Analysis/Analyses/LiveVariables.h"
#include "clang/StaticAnalyzer/Core/CheckerManager.h"
#include "clang/StaticAnalyzer/Core/PathSensitive/ExprEngine.h"
#include "clang/StaticAnalyzer/Core/PathSensitive/CallEvent.h"
#include "clang/AST/DeclCXX.h"
#include "llvm/ADT/SmallSet.h"
#include "llvm/Support/SaveAndRestore.h"

using namespace clang;
using namespace ento;

void ExprEngine::processCallEnter(CallEnter CE, ExplodedNode *Pred) {
  // Get the entry block in the CFG of the callee.
  const StackFrameContext *calleeCtx = CE.getCalleeContext();
  const CFG *CalleeCFG = calleeCtx->getCFG();
  const CFGBlock *Entry = &(CalleeCFG->getEntry());
  
  // Validate the CFG.
  assert(Entry->empty());
  assert(Entry->succ_size() == 1);
  
  // Get the solitary sucessor.
  const CFGBlock *Succ = *(Entry->succ_begin());
  
  // Construct an edge representing the starting location in the callee.
  BlockEdge Loc(Entry, Succ, calleeCtx);

  ProgramStateRef state = Pred->getState();
  
  // Construct a new node and add it to the worklist.
  bool isNew;
  ExplodedNode *Node = G.getNode(Loc, state, false, &isNew);
  Node->addPredecessor(Pred, G);
  if (isNew)
    Engine.getWorkList()->enqueue(Node);
}

// Find the last statement on the path to the exploded node and the
// corresponding Block.
static std::pair<const Stmt*,
                 const CFGBlock*> getLastStmt(const ExplodedNode *Node) {
  const Stmt *S = 0;
  const StackFrameContext *SF =
          Node->getLocation().getLocationContext()->getCurrentStackFrame();

  // Back up through the ExplodedGraph until we reach a statement node.
  while (Node) {
    const ProgramPoint &PP = Node->getLocation();

    if (const StmtPoint *SP = dyn_cast<StmtPoint>(&PP)) {
      S = SP->getStmt();
      break;
    } else if (const CallExitEnd *CEE = dyn_cast<CallExitEnd>(&PP)) {
      S = CEE->getCalleeContext()->getCallSite();
      if (S)
        break;
      // If we have an implicit call, we'll probably end up with a
      // StmtPoint inside the callee, which is acceptable.
      // (It's possible a function ONLY contains implicit calls -- such as an
      // implicitly-generated destructor -- so we shouldn't just skip back to
      // the CallEnter node and keep going.)
    } else if (const CallEnter *CE = dyn_cast<CallEnter>(&PP)) {
      // If we reached the CallEnter for this function, it has no statements.
      if (CE->getCalleeContext() == SF)
        break;
    }

    Node = *Node->pred_begin();
  }

  const CFGBlock *Blk = 0;
  if (S) {
    // Now, get the enclosing basic block.
    while (Node && Node->pred_size() >=1 ) {
      const ProgramPoint &PP = Node->getLocation();
      if (isa<BlockEdge>(PP) &&
          (PP.getLocationContext()->getCurrentStackFrame() == SF)) {
        BlockEdge &EPP = cast<BlockEdge>(PP);
        Blk = EPP.getDst();
        break;
      }
      Node = *Node->pred_begin();
    }
  }

  return std::pair<const Stmt*, const CFGBlock*>(S, Blk);
}

/// The call exit is simulated with a sequence of nodes, which occur between 
/// CallExitBegin and CallExitEnd. The following operations occur between the 
/// two program points:
/// 1. CallExitBegin (triggers the start of call exit sequence)
/// 2. Bind the return value
/// 3. Run Remove dead bindings to clean up the dead symbols from the callee.
/// 4. CallExitEnd (switch to the caller context)
/// 5. PostStmt<CallExpr>
void ExprEngine::processCallExit(ExplodedNode *CEBNode) {
  // Step 1 CEBNode was generated before the call.

  const StackFrameContext *calleeCtx =
      CEBNode->getLocationContext()->getCurrentStackFrame();
  
  // The parent context might not be a stack frame, so make sure we
  // look up the first enclosing stack frame.
  const StackFrameContext *callerCtx =
    calleeCtx->getParent()->getCurrentStackFrame();
  
  const Stmt *CE = calleeCtx->getCallSite();
  ProgramStateRef state = CEBNode->getState();
  // Find the last statement in the function and the corresponding basic block.
  const Stmt *LastSt = 0;
  const CFGBlock *Blk = 0;
  llvm::tie(LastSt, Blk) = getLastStmt(CEBNode);

  // Step 2: generate node with bound return value: CEBNode -> BindedRetNode.

  // If the callee returns an expression, bind its value to CallExpr.
  if (CE) {
    if (const ReturnStmt *RS = dyn_cast_or_null<ReturnStmt>(LastSt)) {
      const LocationContext *LCtx = CEBNode->getLocationContext();
      SVal V = state->getSVal(RS, LCtx);
      state = state->BindExpr(CE, calleeCtx->getParent(), V);
    }

    // Bind the constructed object value to CXXConstructExpr.
    if (const CXXConstructExpr *CCE = dyn_cast<CXXConstructExpr>(CE)) {
      loc::MemRegionVal This =
        svalBuilder.getCXXThis(CCE->getConstructor()->getParent(), calleeCtx);
      SVal ThisV = state->getSVal(This);

      // Always bind the region to the CXXConstructExpr.
      state = state->BindExpr(CCE, calleeCtx->getParent(), ThisV);
    }
  }

  // Step 3: BindedRetNode -> CleanedNodes
  // If we can find a statement and a block in the inlined function, run remove
  // dead bindings before returning from the call. This is important to ensure
  // that we report the issues such as leaks in the stack contexts in which
  // they occurred.
  ExplodedNodeSet CleanedNodes;
  if (LastSt && Blk) {
    static SimpleProgramPointTag retValBind("ExprEngine : Bind Return Value");
    PostStmt Loc(LastSt, calleeCtx, &retValBind);
    bool isNew;
    ExplodedNode *BindedRetNode = G.getNode(Loc, state, false, &isNew);
    BindedRetNode->addPredecessor(CEBNode, G);
    if (!isNew)
      return;

    NodeBuilderContext Ctx(getCoreEngine(), Blk, BindedRetNode);
    currentBuilderContext = &Ctx;
    // Here, we call the Symbol Reaper with 0 statement and caller location
    // context, telling it to clean up everything in the callee's context
    // (and it's children). We use LastStmt as a diagnostic statement, which
    // which the PreStmtPurge Dead point will be associated.
    removeDead(BindedRetNode, CleanedNodes, 0, callerCtx, LastSt,
               ProgramPoint::PostStmtPurgeDeadSymbolsKind);
    currentBuilderContext = 0;
  } else {
    CleanedNodes.Add(CEBNode);
  }

  for (ExplodedNodeSet::iterator I = CleanedNodes.begin(),
                                 E = CleanedNodes.end(); I != E; ++I) {

    // Step 4: Generate the CallExit and leave the callee's context.
    // CleanedNodes -> CEENode
    CallExitEnd Loc(calleeCtx, callerCtx);
    bool isNew;
    ProgramStateRef CEEState = (*I == CEBNode) ? state : (*I)->getState();
    ExplodedNode *CEENode = G.getNode(Loc, CEEState, false, &isNew);
    CEENode->addPredecessor(*I, G);
    if (!isNew)
      return;

    // Step 5: Perform the post-condition check of the CallExpr and enqueue the
    // result onto the work list.
    // CEENode -> Dst -> WorkList
    ExplodedNodeSet Dst;
    NodeBuilderContext Ctx(Engine, calleeCtx->getCallSiteBlock(), CEENode);
    SaveAndRestore<const NodeBuilderContext*> NBCSave(currentBuilderContext,
        &Ctx);
    SaveAndRestore<unsigned> CBISave(currentStmtIdx, calleeCtx->getIndex());

    // FIXME: This needs to call PostCall.
    // FIXME: If/when we inline Objective-C messages, this also needs to call
    // PostObjCMessage.
    if (CE)
      getCheckerManager().runCheckersForPostStmt(Dst, CEENode, CE, *this, true);
    else
      Dst.Add(CEENode);

    // Enqueue the next element in the block.
    for (ExplodedNodeSet::iterator PSI = Dst.begin(), PSE = Dst.end();
                                   PSI != PSE; ++PSI) {
      Engine.getWorkList()->enqueue(*PSI, calleeCtx->getCallSiteBlock(),
                                    calleeCtx->getIndex()+1);
    }
  }
}

static unsigned getNumberStackFrames(const LocationContext *LCtx) {
  unsigned count = 0;
  while (LCtx) {
    if (isa<StackFrameContext>(LCtx))
      ++count;
    LCtx = LCtx->getParent();
  }
  return count;  
}

// Determine if we should inline the call.
bool ExprEngine::shouldInlineDecl(const Decl *D, ExplodedNode *Pred) {
  AnalysisDeclContext *CalleeADC = AMgr.getAnalysisDeclContext(D);
  const CFG *CalleeCFG = CalleeADC->getCFG();

  // It is possible that the CFG cannot be constructed.
  // Be safe, and check if the CalleeCFG is valid.
  if (!CalleeCFG)
    return false;

  if (getNumberStackFrames(Pred->getLocationContext())
        == AMgr.InlineMaxStackDepth)
    return false;

  if (Engine.FunctionSummaries->hasReachedMaxBlockCount(D))
    return false;

  if (CalleeCFG->getNumBlockIDs() > AMgr.InlineMaxFunctionSize)
    return false;

  // Do not inline variadic calls (for now).
  if (const BlockDecl *BD = dyn_cast<BlockDecl>(D)) {
    if (BD->isVariadic())
      return false;
  }
  else if (const FunctionDecl *FD = dyn_cast<FunctionDecl>(D)) {
    if (FD->isVariadic())
      return false;
  }

  // It is possible that the live variables analysis cannot be
  // run.  If so, bail out.
  if (!CalleeADC->getAnalysis<RelaxedLiveVariables>())
    return false;

  return true;
}

bool ExprEngine::inlineCall(const CallEvent &Call,
                            ExplodedNode *Pred) {
  if (!getAnalysisManager().shouldInlineCall())
    return false;

  const Decl *D = Call.getRuntimeDefinition();
  if (!D)
    return false;

  const LocationContext *CurLC = Pred->getLocationContext();
  const StackFrameContext *CallerSFC = CurLC->getCurrentStackFrame();
  const LocationContext *ParentOfCallee = 0;

  switch (Call.getKind()) {
  case CE_Function:
  case CE_CXXMember:
  case CE_CXXMemberOperator:
    // These are always at least possible to inline.
    break;
  case CE_CXXConstructor:
  case CE_CXXDestructor: {
    // Only inline constructors and destructors if we built the CFGs for them
    // properly.
    const AnalysisDeclContext *ADC = CallerSFC->getAnalysisDeclContext();
    if (!ADC->getCFGBuildOptions().AddImplicitDtors ||
        !ADC->getCFGBuildOptions().AddInitializers)
      return false;

    // FIXME: We don't handle constructors or destructors for arrays properly.
    const MemRegion *Target = Call.getCXXThisVal().getAsRegion();
    if (Target && isa<ElementRegion>(Target))
      return false;

    // FIXME: This is a hack. We don't handle temporary destructors
    // right now, so we shouldn't inline their constructors.
    if (const CXXConstructorCall *Ctor = dyn_cast<CXXConstructorCall>(&Call)) {
      const CXXConstructExpr *CtorExpr = Ctor->getOriginExpr();
      if (CtorExpr->getConstructionKind() == CXXConstructExpr::CK_Complete)
        if (!Target || !isa<DeclRegion>(Target))
          return false;
    }
    break;
  }
  case CE_CXXAllocator:
    // Do not inline allocators until we model deallocators.
    // This is unfortunate, but basically necessary for smart pointers and such.
    return false;
  case CE_Block: {
    const BlockDataRegion *BR = cast<BlockCall>(Call).getBlockRegion();
    assert(BR && "If we have the block definition we should have its region");
    AnalysisDeclContext *BlockCtx = AMgr.getAnalysisDeclContext(D);
    ParentOfCallee = BlockCtx->getBlockInvocationContext(CallerSFC,
                                                         cast<BlockDecl>(D),
                                                         BR);
    break;
  }
  case CE_ObjCMessage:
    break;
  }

  if (!shouldInlineDecl(D, Pred))
    return false;
  
  if (!ParentOfCallee)
    ParentOfCallee = CallerSFC;

  // This may be NULL, but that's fine.
  const Expr *CallE = Call.getOriginExpr();

  // Construct a new stack frame for the callee.
  AnalysisDeclContext *CalleeADC = AMgr.getAnalysisDeclContext(D);
  const StackFrameContext *CalleeSFC =
    CalleeADC->getStackFrame(ParentOfCallee, CallE,
                             currentBuilderContext->getBlock(),
                             currentStmtIdx);
  
  CallEnter Loc(CallE, CalleeSFC, CurLC);

  // Construct a new state which contains the mapping from actual to
  // formal arguments.
  ProgramStateRef State = Pred->getState()->enterStackFrame(Call, CalleeSFC);

  bool isNew;
  if (ExplodedNode *N = G.getNode(Loc, State, false, &isNew)) {
    N->addPredecessor(Pred, G);
    if (isNew)
      Engine.getWorkList()->enqueue(N);
  }
  return true;
}

static ProgramStateRef getInlineFailedState(ProgramStateRef State,
                                            const Stmt *CallE) {
  void *ReplayState = State->get<ReplayWithoutInlining>();
  if (!ReplayState)
    return 0;

  assert(ReplayState == (const void*)CallE && "Backtracked to the wrong call.");
  (void)CallE;

  return State->remove<ReplayWithoutInlining>();
}

void ExprEngine::VisitCallExpr(const CallExpr *CE, ExplodedNode *Pred,
                               ExplodedNodeSet &dst) {
  // Perform the previsit of the CallExpr.
  ExplodedNodeSet dstPreVisit;
  getCheckerManager().runCheckersForPreStmt(dstPreVisit, Pred, CE, *this);

  // Get the call in its initial state. We use this as a template to perform
  // all the checks.
  CallEventManager &CEMgr = getStateManager().getCallEventManager();
  CallEventRef<SimpleCall> CallTemplate
    = CEMgr.getSimpleCall(CE, Pred->getState(), Pred->getLocationContext());

  // Evaluate the function call.  We try each of the checkers
  // to see if the can evaluate the function call.
  ExplodedNodeSet dstCallEvaluated;
  for (ExplodedNodeSet::iterator I = dstPreVisit.begin(), E = dstPreVisit.end();
       I != E; ++I) {
    evalCall(dstCallEvaluated, *I, *CallTemplate);
  }

  // Finally, perform the post-condition check of the CallExpr and store
  // the created nodes in 'Dst'.
  // Note that if the call was inlined, dstCallEvaluated will be empty.
  // The post-CallExpr check will occur in processCallExit.
  getCheckerManager().runCheckersForPostStmt(dst, dstCallEvaluated, CE,
                                             *this);
}

void ExprEngine::evalCall(ExplodedNodeSet &Dst, ExplodedNode *Pred,
                          const SimpleCall &Call) {
  // WARNING: At this time, the state attached to 'Call' may be older than the
  // state in 'Pred'. This is a minor optimization since CheckerManager will
  // use an updated CallEvent instance when calling checkers, but if 'Call' is
  // ever used directly in this function all callers should be updated to pass
  // the most recent state. (It is probably not worth doing the work here since
  // for some callers this will not be necessary.)

  // Run any pre-call checks using the generic call interface.
  ExplodedNodeSet dstPreVisit;
  getCheckerManager().runCheckersForPreCall(dstPreVisit, Pred, Call, *this);

  // Actually evaluate the function call.  We try each of the checkers
  // to see if the can evaluate the function call, and get a callback at
  // defaultEvalCall if all of them fail.
  ExplodedNodeSet dstCallEvaluated;
  getCheckerManager().runCheckersForEvalCall(dstCallEvaluated, dstPreVisit,
                                             Call, *this);

  // Finally, run any post-call checks.
  getCheckerManager().runCheckersForPostCall(Dst, dstCallEvaluated,
                                             Call, *this);
}

ProgramStateRef ExprEngine::bindReturnValue(const CallEvent &Call,
                                            const LocationContext *LCtx,
                                            ProgramStateRef State) {
  const Expr *E = Call.getOriginExpr();
  if (!E)
    return State;

  // Some method families have known return values.
  if (const ObjCMethodCall *Msg = dyn_cast<ObjCMethodCall>(&Call)) {
    switch (Msg->getMethodFamily()) {
    default:
      break;
    case OMF_autorelease:
    case OMF_retain:
    case OMF_self: {
      // These methods return their receivers.
      return State->BindExpr(E, LCtx, Msg->getReceiverSVal());
    }
    }
  } else if (const CXXConstructorCall *C = dyn_cast<CXXConstructorCall>(&Call)){
    return State->BindExpr(E, LCtx, C->getCXXThisVal());
  }

  // Conjure a symbol if the return value is unknown.
  QualType ResultTy = Call.getResultType();
  SValBuilder &SVB = getSValBuilder();
  unsigned Count = currentBuilderContext->getCurrentBlockCount();
  SVal R = SVB.getConjuredSymbolVal(0, E, LCtx, ResultTy, Count);
  return State->BindExpr(E, LCtx, R);
}

void ExprEngine::defaultEvalCall(NodeBuilder &Bldr, ExplodedNode *Pred,
                                 const CallEvent &CallTemplate) {
  // Make sure we have the most recent state attached to the call.
  ProgramStateRef State = Pred->getState();
  CallEventRef<> Call = CallTemplate.cloneWithState(State);

  // Try to inline the call.
  // The origin expression here is just used as a kind of checksum;
  // this should still be safe even for CallEvents that don't come from exprs.
  const Expr *E = Call->getOriginExpr();
  ProgramStateRef InlinedFailedState = getInlineFailedState(State, E);

  if (InlinedFailedState) {
    // If we already tried once and failed, make sure we don't retry later.
    State = InlinedFailedState;
  } else if (inlineCall(*Call, Pred)) {
    // If we decided to inline the call, the successor has been manually
    // added onto the work list and we should not perform our generic
    // call-handling steps.
    Bldr.takeNodes(Pred);
    return;
  }

  // If we can't inline it, handle the return value and invalidate the regions.
  unsigned Count = currentBuilderContext->getCurrentBlockCount();
  State = Call->invalidateRegions(Count, State);
  State = bindReturnValue(*Call, Pred->getLocationContext(), State);

  // And make the result node.
  Bldr.generateNode(Call->getProgramPoint(), State, Pred);
}

void ExprEngine::VisitReturnStmt(const ReturnStmt *RS, ExplodedNode *Pred,
                                 ExplodedNodeSet &Dst) {
  
  ExplodedNodeSet dstPreVisit;
  getCheckerManager().runCheckersForPreStmt(dstPreVisit, Pred, RS, *this);

  StmtNodeBuilder B(dstPreVisit, Dst, *currentBuilderContext);
  
  if (RS->getRetValue()) {
    for (ExplodedNodeSet::iterator it = dstPreVisit.begin(),
                                  ei = dstPreVisit.end(); it != ei; ++it) {
      B.generateNode(RS, *it, (*it)->getState());
    }
  }
}
