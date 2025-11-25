// SPDX-FileCopyrightText: 2024 Google LLC
// SPDX-License-Identifier: Apache-2.0

#include "clang/StaticAnalyzer/Core/Checker.h"
#include "clang/StaticAnalyzer/Core/PathSensitive/CheckerContext.h"
#include "clang/StaticAnalyzer/Core/PathSensitive/CallEvent.h"
#include "clang/StaticAnalyzer/Core/CheckerRegistry.h"
#include "clang/StaticAnalyzer/Core/BugReporter/BugType.h"
#include "llvm/ADT/StringRef.h"
#include "llvm/ADT/StringSet.h"

using namespace clang;
using namespace ento;

namespace std {
  void terminate( void ) _NOEXCEPT {
   abort();
  }
}

// Need to specialize for any custom types used in traits
// Look in include/clang/StaticAnalyzer/Core/PathSensitive/ProgramStateTrait.h
namespace clang {
  namespace ento {
    template <> struct ProgramStatePartialTrait<const FunctionDecl *> {
      typedef const FunctionDecl * data_type;

      static inline data_type MakeData(void *const* p) {
        return p ? (const FunctionDecl *)*p : data_type();
      }

      static inline void *MakeVoidPtr(data_type d) {
        return const_cast<FunctionDecl *>(d);
      }
    };
  }
}

/* Ultimately this would work better / be more thorough if it made use of the Analyzer's
 * taint checking, but there is no infrastructure to remove taint at the moment.
 */

REGISTER_TRAIT_WITH_PROGRAMSTATE(CurrentSyscallState, const FunctionDecl *);
// Args are tracked by their MemRegion
REGISTER_SET_WITH_PROGRAMSTATE(TaintedArgsState, const MemRegion *);

namespace {
  class SyscallSecurityChecker : 
        public Checker< eval::Call, check::PreCall, check::Location, check::Bind, check::EndFunction > {

    std::unique_ptr<BugType> NoBoundsCheckBugType;
    std::unique_ptr<BugType> UnsafeCallBugType;

    llvm::StringSet<> unsafeFunctions {};

    const FunctionDecl * getCurrentSyscall(const ProgramStateRef state) const {
      return state->get<CurrentSyscallState>();
    }

    bool inSyscall(const ProgramStateRef state) const {
      return !!getCurrentSyscall(state);
    }

    ProgramStateRef setCurrentSyscall(const ProgramStateRef state, const FunctionDecl *FD) const {
      return state->set<CurrentSyscallState>(FD);
    }

    bool isValTainted(const SVal &arg, const ProgramStateRef state) const {
      const MemRegion *MR = arg.getAsRegion();
      if (!MR) {
        return false;
      }
      const MemRegion *baseMR = MR->getBaseRegion();

      return state->contains<TaintedArgsState>(baseMR);
    }

    void reportUnsanitizedUse(const SVal &arg, const ProgramStateRef state, CheckerContext &C) const {
      ExplodedNode *errNode = C.generateSink();
      if (!errNode) {
        // Already reported an error here
        return;
      }
      BugReport *R = new BugReport(*NoBoundsCheckBugType, 
        "Used an unsanitized argument from syscall", errNode);
      R->markInteresting(arg);
      C.emitReport(R);
    }

    public:
    SyscallSecurityChecker(void)
      : NoBoundsCheckBugType(new BugType(this, "Failed to check bounds", "Pebble Syscall Plugin")),
        UnsafeCallBugType(new BugType(this, "Syscall used dangerous function", "Pebble Syscall Plugin"))
    {
      StringRef funcs[] = { "task_malloc", "task_zalloc", "task_calloc", "app_malloc", "app_zalloc", "app_calloc" };

      // It would be more efficient to look up the IdentifierInfos for each of these and compare against that
      for (StringRef func : funcs) {
        unsafeFunctions.insert(func);
      }
    }

    bool evalCall(const CallExpr *call, CheckerContext &C) const {
      if (!C.getCalleeName(call).equals("syscall_internal_elevate_privilege")) {
        return false;
      }
      // Always return true from syscall_internal_elevate_privilege
      // so the analyzer always thinks privileges have been elevated

      ProgramStateRef state = C.getState();

      SVal ret = C.getSValBuilder().makeTruthVal(true);
      state = state->BindExpr(call, C.getLocationContext(), ret);

      C.addTransition(state);
      return true;
    }

    void checkPreCall(const CallEvent &call, CheckerContext &C) const {
      const IdentifierInfo *identInfo = call.getCalleeIdentifier();
      if(!identInfo) {
        return;
      }
      StringRef funcName = identInfo->getName();

      ProgramStateRef state = C.getState();

      if (funcName.equals("syscall_internal_elevate_privilege")) {
        const LocationContext *LCtx = C.getLocationContext();
        const FunctionDecl *FD = dyn_cast<FunctionDecl>(LCtx->getDecl());
        if (!FD) {
          llvm::errs() << "Privileges elevated outside of function?\n";
          return;
        }

        ExplodedNode *pred = NULL;

        // If we're not at the top level, we generate two new transitions, one for the current syscall
        // executing normally, and one which simulates execution starting at this syscall.
        // This is important, because if a syscall is called by another function, the syscall
        // will not be treated as an entry point by the analyzer.

        if (!C.inTopFrame()) {
          C.addTransition(state);
          state = C.getStateManager().getInitialState(LCtx);
          // Get the first node in the state graph
          pred = C.getPredecessor();
          while (pred->getFirstPred()) {
            pred = pred->getFirstPred();
          }
        }

        for (unsigned i = 0; i < FD->getNumParams(); i++) {
          // We only care about tracking pointer arguments
          const ParmVarDecl *ParamDecl = FD->getParamDecl(i);
          if (ParamDecl->getType()->isPointerType()) {
            // Find the MemRegion associated with the parameter.
            // Seems very roundabout, but it works...
            // Remember to look at state->getRegion
            Loc lValue = state->getLValue(ParamDecl, LCtx);
            SVal valRegion = state->getSVal(lValue);
            if (valRegion == UnknownVal()) {
              llvm::errs() << "Failed to get argument SymbolRef\n";
              continue;
            }
            const MemRegion *MR = valRegion.getAsRegion();
            if (!MR) {
              llvm::errs() << "No region for ptr argument\n";
              continue;
            }
            state = state->add<TaintedArgsState>(MR);
          }
        }
        state = setCurrentSyscall(state, FD);
        C.addTransition(state, pred, nullptr);
      }
      else if (inSyscall(state)) {
        if (funcName.equals("syscall_assert_userspace_buffer")) {
          const MemRegion *MR = call.getArgSVal(0).getAsRegion();

          state = state->remove<TaintedArgsState>(MR);
        }
        else if (funcName.equals("memory_layout_is_cstring_in_region") ||
                 funcName.equals("memory_layout_is_pointer_in_region")) {
          const MemRegion *MR = call.getArgSVal(1).getAsRegion();

          state = state->remove<TaintedArgsState>(MR);
        }
        // Make sure the syscall isn't calling an unsafe function
        else if (unsafeFunctions.count(funcName)) {
          ExplodedNode *errNode = C.generateSink();
          if (!errNode) {
            // Already reported an error here
            return;
          }
          BugReport *R = new BugReport(*UnsafeCallBugType,
            "This function shouldn't be called from privileged code", errNode);
          C.emitReport(R);
          return;
        }
        else { // Any other function, just want to make sure it isn't getting the unsanitized args
          for (unsigned i = 0; i < call.getNumArgs(); i++) {
            SVal argVal = call.getArgSVal(i);
            if (isValTainted(argVal, state)) {
              reportUnsanitizedUse(argVal, state, C);
              return;
            }
          }
        }
        C.addTransition(state);
      }
    }

    void checkLocation(SVal loc, bool isLoad, const Stmt *S, CheckerContext &C) const {
      ProgramStateRef state = C.getState(); 
      
      if (isValTainted(loc, state)) {
        reportUnsanitizedUse(loc, state, C);
      }
    }

    void checkBind(SVal loc, SVal val, const Stmt *S, CheckerContext &C) const {
      ProgramStateRef state = C.getState(); 
      
      if (isValTainted(val, state)) {
        reportUnsanitizedUse(val, state, C);
      }
    }

    void checkEndFunction(CheckerContext &C) const {
      ProgramStateRef state = C.getState();

      const Decl *D = C.getLocationContext()->getDecl();
      const FunctionDecl *FD = dyn_cast<FunctionDecl>(D);
      if (!FD) {
        // Not sure why this would ever be the case...
        llvm::errs() << "Path ended outside of function?\n";
        return;
      }

      if (FD != getCurrentSyscall(state)) {
        return;
      }

      // Since we are effectively emulating every syscall as an entry point from
      // the analyzer's perspective, once the syscall is done, end the path.
      C.generateSink();
    }
  };
}

extern "C" const char clang_analyzerAPIVersionString[] = CLANG_ANALYZER_API_VERSION_STRING;

extern "C" void clang_registerCheckers(CheckerRegistry &registry) {
  registry.addChecker<SyscallSecurityChecker>("pebble.SyscallSecurityChecker", "Checker that makes sure pointer arguments to syscalls are sanitized");
}
