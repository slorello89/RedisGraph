#pragma once

#include "../arithmetic/arithmetic_expression.h"
#include "pthread.h"
#include "llvm-c/Core.h"
#include "llvm-c/Error.h"
#include "llvm-c/Initialization.h"
#include "llvm-c/OrcEE.h"
#include "llvm-c/LLJIT.h"
#include "llvm-c/Support.h"
#include "llvm-c/Target.h"
#include "llvm-c/Analysis.h"


typedef struct {
	LLVMModuleRef module;
	LLVMBuilderRef builder;
	LLVMOrcThreadSafeContextRef TSCtx;
	LLVMContextRef Ctx;
	LLVMOrcThreadSafeModuleRef TSM;
	LLVMTypeRef voidPtr;
	LLVMTypeRef i32;
	LLVMTypeRef i64;
	LLVMTypeRef si_type;
	LLVMTypeRef node_type;
	LLVMValueRef r;
	LLVMValueRef g;
} EmitCtx;

typedef void *(*SymbolResolve)(const char *);

void EmitCtx_Init();
EmitCtx *EmitCtx_Get();

void JIT_Init(void *g);
void JIT_End();
void JIT_Run(SymbolResolve fn);
void JIT_CreateRecord(void *opBase);
void JIT_Result(void *rsVal);
void JIT_Project(void *opBase, AR_ExpNode **exps, uint exp_count, uint *record_offsets);
void JIT_LabelScan(void *iter, int nodeIdx);