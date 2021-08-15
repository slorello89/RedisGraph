#pragma once

#include "../arithmetic/arithmetic_expression.h"
#include "pthread.h"
#include "llvm-c/Core.h"
#include "llvm-c/ExecutionEngine.h"
#include "llvm-c/Target.h"
#include "llvm-c/Analysis.h"


typedef struct {
	LLVMModuleRef module;
	LLVMBuilderRef builder;
	LLVMValueRef addRecord_func;
	LLVMValueRef addToRecord_func;
	LLVMValueRef createRecord_func;
	LLVMValueRef AR_EXP_Evaluate_func;
	LLVMValueRef iter_next_func;
	LLVMValueRef getNode_func;
	LLVMValueRef addNode_func;
	LLVMValueRef r;
	LLVMValueRef g;
	void **arr;
} EmitCtx;

void EmitCtx_Init();
EmitCtx *EmitCtx_Get();

void JIT_Init(void *g);
void JIT_CreateRecord(void *opBase);
void JIT_Result(void *rsVal);
void JIT_Project(void *opBase, AR_ExpNode **exps, uint exp_count, uint *record_offsets);
void JIT_LabelScan(void *iter, int nodeIdx);