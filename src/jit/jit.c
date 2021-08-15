#include "jit.h"
#include "../arithmetic/arithmetic_expression.h"
#include "../util/rmalloc.h"
#include "../util/arr.h"

pthread_key_t _tlsEmitCtxKey;  // Thread local storage query context key.
static const char str_g[] = "g";
static const char str_rs[] = "rs";
static const char str_op[] = "op";
static const char str_AddRecord[] = "ResultSet_AddRecord";
static const char str_CreateRecord[] = "OpBase_CreateRecord";
static const char str_Record_Add[] = "Record_Add";
static const char str_AR_EXP_Evaluate[] = "AR_EXP_Evaluate";
static const char str_RG_MatrixTupleIter_next[] = "RG_MatrixTupleIter_next";
static const char str_Graph_GetNode[] = "Graph_GetNode";
static const char str_Record_AddNode[] = "Record_AddNode";

void EmitCtx_Init() {
	pthread_key_create(&_tlsEmitCtxKey, NULL);
}

EmitCtx *EmitCtx_Get() {
	EmitCtx *ctx = pthread_getspecific(_tlsEmitCtxKey);
	if(!ctx) {
		// Set a new thread-local EmitCtx if one has not been created.
		ctx = rm_calloc(1, sizeof(EmitCtx));
		ctx->module = LLVMModuleCreateWithName("query");
		ctx->arr = array_new(void*, 0);
		pthread_setspecific(_tlsEmitCtxKey, ctx);
	}
	return ctx;    
}

static LLVMValueRef GetOrCreateFunction(const char* name) {
	EmitCtx *ctx = EmitCtx_Get();

	LLVMTypeRef voidPtr = LLVMPointerType(LLVMVoidType(), 0);

	LLVMValueRef f = LLVMGetNamedFunction(ctx->module, name);
	if(f) return f;

	if(strcmp(name, "ResultSet_AddRecord") == 0) {
		LLVMTypeRef addRecord_params[] = { voidPtr, voidPtr }; 
		LLVMTypeRef addRecord_type = LLVMFunctionType(LLVMInt32Type(), addRecord_params, 2, 0);

		ctx->addRecord_func = LLVMAddFunction(ctx->module, "ResultSet_AddRecord", addRecord_type);
		return ctx->addRecord_func;
	}

	if(strcmp(name, "OpBase_CreateRecord") == 0) {
		LLVMTypeRef createRecord_params[] = { voidPtr }; 
		LLVMTypeRef createRecord_type = LLVMFunctionType(voidPtr, createRecord_params, 1, 0);

		ctx->createRecord_func = LLVMAddFunction(ctx->module, "OpBase_CreateRecord", createRecord_type);
		return ctx->createRecord_func;
	}

	LLVMTypeRef x[] = {LLVMInt64Type(), LLVMInt32Type(), LLVMInt32Type()};
	LLVMTypeRef si_type = LLVMStructType(x, 3, 0);

	if(strcmp(name, "AR_EXP_Evaluate") == 0) {
		LLVMTypeRef AR_EXP_Evaluate_params[] = { voidPtr, voidPtr }; 
		LLVMTypeRef AR_EXP_Evaluate_type = LLVMFunctionType(si_type, AR_EXP_Evaluate_params, 2, 0);

		ctx->AR_EXP_Evaluate_func = LLVMAddFunction(ctx->module, "AR_EXP_Evaluate", AR_EXP_Evaluate_type);
		return ctx->AR_EXP_Evaluate_func;
	}

	if(strcmp(name, "Record_Add") == 0) {
		LLVMTypeRef addRecord_params[] = { voidPtr, LLVMInt32Type(), si_type }; 
		LLVMTypeRef addRecord_type = LLVMFunctionType(voidPtr, addRecord_params, 3, 0);

		ctx->addToRecord_func = LLVMAddFunction(ctx->module, "Record_Add", addRecord_type);
		return ctx->addToRecord_func;
	}

	if(strcmp(name, "RG_MatrixTupleIter_next") == 0) {
		LLVMTypeRef i64ptr = LLVMPointerType(LLVMInt64Type(), 0);
		LLVMTypeRef boolPtr = LLVMPointerType(LLVMInt8Type(), 0);
		LLVMTypeRef iter_next_params[] = { voidPtr, i64ptr, i64ptr, voidPtr, boolPtr }; 
		LLVMTypeRef iter_next_type = LLVMFunctionType(LLVMInt32Type(), iter_next_params, 5, 0);

		ctx->iter_next_func = LLVMAddFunction(ctx->module, "RG_MatrixTupleIter_next", iter_next_type);
		return ctx->iter_next_func;
	}

	LLVMTypeRef node_x[] = {voidPtr, LLVMInt64Type(), voidPtr, LLVMInt32Type()};
	LLVMTypeRef node_type = LLVMStructType(node_x, 4, 0);

	if(strcmp(name, "Graph_GetNode") == 0) {
		LLVMTypeRef iter_next_params[] = { voidPtr, LLVMInt64Type(), LLVMPointerType(node_type, 0) }; 
		LLVMTypeRef iter_next_type = LLVMFunctionType(LLVMInt32Type(), iter_next_params, 3, 0);

		ctx->getNode_func = LLVMAddFunction(ctx->module, "Graph_GetNode", iter_next_type);
		return ctx->getNode_func;
	}

	if(strcmp(name, "Record_AddNode") == 0) {
		LLVMTypeRef addNode_params[] = { voidPtr, LLVMInt32Type(), LLVMPointerType(node_type, 0) }; 
		LLVMTypeRef addNode_type = LLVMFunctionType(LLVMInt32Type(), addNode_params, 3, 0);

		ctx->addNode_func = LLVMAddFunction(ctx->module, "Record_AddNode", addNode_type);
		return ctx->addNode_func;
	}

	return NULL;
}

static LLVMValueRef GetOrCreateGlobal(const char* name, void* value) {
	EmitCtx *ctx = EmitCtx_Get();
	LLVMValueRef global = LLVMGetNamedGlobal(ctx->module, name);
	if(global) return global;

	LLVMTypeRef voidPtr = LLVMPointerType(LLVMVoidType(), 0);

	global = LLVMAddGlobal(ctx->module, voidPtr, name);

	array_append(ctx->arr, global);
	array_append(ctx->arr, value);

	return global;
}

void JIT_Init(void *g) {
	LLVMTypeRef voidPtr = LLVMPointerType(LLVMVoidType(), 0);

	EmitCtx *ctx = EmitCtx_Get();

	ctx->g = GetOrCreateGlobal(str_g, g);
}

void JIT_CreateRecord(void *opBase) {
	LLVMTypeRef voidPtr = LLVMPointerType(LLVMVoidType(), 0);

	EmitCtx *ctx = EmitCtx_Get();

	LLVMValueRef createRecord_func = GetOrCreateFunction(str_CreateRecord);

	LLVMValueRef global = GetOrCreateGlobal(str_op, opBase);
	LLVMValueRef local = LLVMBuildLoad2(ctx->builder, voidPtr, global, "local");
	LLVMValueRef args[] = {local};
	LLVMValueRef r = LLVMBuildCall2(ctx->builder, LLVMGetReturnType(LLVMTypeOf(createRecord_func)), createRecord_func, args, 1, "call");
	ctx->r = r;
}

void JIT_Result(void *rsVal) {
	LLVMTypeRef voidPtr = LLVMPointerType(LLVMVoidType(), 0);

	EmitCtx *ctx = EmitCtx_Get();

	LLVMValueRef addRecord_func = GetOrCreateFunction(str_AddRecord);
	LLVMValueRef rs = GetOrCreateGlobal(str_rs, rsVal);
	LLVMValueRef local = LLVMBuildLoad2(ctx->builder, voidPtr, rs, "local");


	LLVMValueRef args[] = {local, ctx->r};
	LLVMBuildCall2(ctx->builder, LLVMGetReturnType(LLVMTypeOf(addRecord_func)), addRecord_func, args, 2, "call");

	JIT_CreateRecord(NULL);
}

void JIT_Project(void *opBase, AR_ExpNode **exps, uint exp_count, uint *record_offsets) {
	LLVMTypeRef voidPtr = LLVMPointerType(LLVMVoidType(), 0);

	EmitCtx *ctx = EmitCtx_Get();

	LLVMValueRef addRecord_func = GetOrCreateFunction(str_Record_Add);
	LLVMValueRef AR_EXP_Evaluate_func = GetOrCreateFunction(str_AR_EXP_Evaluate);

	for(uint i = 0; i < exp_count; i++) {
		AR_ExpNode *exp = exps[i];
		char *name;
		asprintf(&name, "project_expr_%d", i);
		LLVMValueRef expr_global = GetOrCreateGlobal(name, exp);
		LLVMValueRef expr_local = LLVMBuildLoad2(ctx->builder, voidPtr, expr_global, name);
		LLVMValueRef eval_args[] = {expr_local, ctx->r};
		LLVMValueRef v = LLVMBuildCall2(ctx->builder, LLVMGetReturnType(LLVMTypeOf(AR_EXP_Evaluate_func)), AR_EXP_Evaluate_func, eval_args, 2, "call");

		int rec_idx = record_offsets[i];

		LLVMValueRef addRec_args[] = {ctx->r, LLVMConstInt(LLVMInt32Type(), rec_idx, 0), v};
		LLVMValueRef r = LLVMBuildCall2(ctx->builder, LLVMGetReturnType(LLVMTypeOf(addRecord_func)), addRecord_func, addRec_args, 3, "call");
	}
}

void JIT_LabelScan(void *iter, int nodeIdx) {
	LLVMTypeRef voidPtr = LLVMPointerType(LLVMVoidType(), 0);
	LLVMTypeRef i64ptr = LLVMPointerType(LLVMInt64Type(), 0);

	EmitCtx *ctx = EmitCtx_Get();

	LLVMValueRef iter_next_func = GetOrCreateFunction(str_RG_MatrixTupleIter_next);
	LLVMValueRef getNode_func = GetOrCreateFunction(str_Graph_GetNode);
	LLVMValueRef addNode_func = GetOrCreateFunction(str_Record_AddNode);

	LLVMValueRef iter_global = GetOrCreateGlobal("iter", iter);
	LLVMValueRef iter_local = LLVMBuildLoad2(ctx->builder, voidPtr, iter_global, "iter");

	LLVMValueRef nodeId = LLVMBuildAlloca(ctx->builder, LLVMInt64Type(), "nodeId");
	LLVMValueRef depleted = LLVMBuildAlloca(ctx->builder, LLVMInt8Type(), "depleted");

	LLVMValueRef iter_next_params[] = {iter_local, LLVMConstPointerNull(i64ptr), nodeId, LLVMConstPointerNull(voidPtr), depleted};
	LLVMBuildCall2(ctx->builder, LLVMGetReturnType(LLVMTypeOf(iter_next_func)), iter_next_func, iter_next_params, 5, "call");

	LLVMTypeRef x[] = {voidPtr, LLVMInt64Type(), voidPtr, LLVMInt32Type()};
	LLVMTypeRef node_type = LLVMStructType(x, 4, 0);

	LLVMValueRef node = LLVMBuildAlloca(ctx->builder, node_type, "node");
	LLVMValueRef g_local = LLVMBuildLoad2(ctx->builder, voidPtr, ctx->g, "g");
	LLVMValueRef nodeId_load = LLVMBuildLoad(ctx->builder, nodeId, "nodeId_load");
	LLVMValueRef getNode_params[] = {g_local, nodeId_load, node};
	LLVMBuildCall2(ctx->builder, LLVMGetReturnType(LLVMTypeOf(getNode_func)), getNode_func, getNode_params, 3, "call");

	LLVMValueRef addNode_params[] = {ctx->r, LLVMConstInt(LLVMInt32Type(), nodeIdx, 0), node};
	LLVMBuildCall2(ctx->builder, LLVMGetReturnType(LLVMTypeOf(addNode_func)), addNode_func, addNode_params, 3, "call");
}