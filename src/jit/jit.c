#include "jit.h"
#include "../arithmetic/arithmetic_expression.h"
#include "../util/rmalloc.h"
#include "../util/arr.h"

pthread_key_t _tlsEmitCtxKey;  // Thread local storage query context key.

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

	if(name == "ResultSet_AddRecord") {
		if(ctx->addRecord_func) return ctx->addRecord_func;

		LLVMTypeRef addRecord_params[] = { voidPtr, voidPtr }; 
		LLVMTypeRef addRecord_type = LLVMFunctionType(LLVMInt32Type(), addRecord_params, 2, 0);

		ctx->addRecord_func = LLVMAddFunction(ctx->module, "ResultSet_AddRecord", addRecord_type);
		return ctx->addRecord_func;
	}

	if(name == "OpBase_CreateRecord") {
		if(ctx->createRecord_func) return ctx->createRecord_func;

		LLVMTypeRef createRecord_params[] = { voidPtr }; 
		LLVMTypeRef createRecord_type = LLVMFunctionType(voidPtr, createRecord_params, 1, 0);

		ctx->createRecord_func = LLVMAddFunction(ctx->module, "OpBase_CreateRecord", createRecord_type);
		return ctx->createRecord_func;
	}

	LLVMTypeRef x[] = {LLVMInt64Type(), LLVMInt32Type(), LLVMInt32Type()};
	LLVMTypeRef si_type = LLVMStructType(x, 3, 0);

	if(name == "AR_EXP_Evaluate") {
		if(ctx->AR_EXP_Evaluate_func) return ctx->AR_EXP_Evaluate_func;

		LLVMTypeRef AR_EXP_Evaluate_params[] = { voidPtr, voidPtr }; 
		LLVMTypeRef AR_EXP_Evaluate_type = LLVMFunctionType(si_type, AR_EXP_Evaluate_params, 2, 0);

		ctx->AR_EXP_Evaluate_func = LLVMAddFunction(ctx->module, "AR_EXP_Evaluate", AR_EXP_Evaluate_type);
		return ctx->AR_EXP_Evaluate_func;
	}

	if(name == "Record_Add") {
		if(ctx->addToRecord_func) return ctx->addToRecord_func;

		LLVMTypeRef addRecord_params[] = { voidPtr, LLVMInt32Type(), si_type }; 
		LLVMTypeRef addRecord_type = LLVMFunctionType(voidPtr, addRecord_params, 3, 0);

		ctx->addToRecord_func = LLVMAddFunction(ctx->module, "Record_Add", addRecord_type);
		return ctx->addToRecord_func;
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

void JIT_Result(void *rsVal) {
	LLVMTypeRef voidPtr = LLVMPointerType(LLVMVoidType(), 0);

	EmitCtx *ctx = EmitCtx_Get();

	LLVMValueRef addRecord_func = GetOrCreateFunction("ResultSet_AddRecord");
	LLVMValueRef rs = GetOrCreateGlobal("rs", rsVal);
	LLVMValueRef local = LLVMBuildLoad2(ctx->builder, voidPtr, rs, "local");


	LLVMValueRef args[] = {local, ctx->r};
	LLVMBuildCall2(ctx->builder, LLVMGetReturnType(LLVMTypeOf(addRecord_func)), addRecord_func, args, 2, "call");
}

void JIT_Project(void *opBase, AR_ExpNode **exps, uint exp_count, uint *record_offsets) {
	LLVMTypeRef voidPtr = LLVMPointerType(LLVMVoidType(), 0);

	EmitCtx *ctx = EmitCtx_Get();

	LLVMValueRef createRecord_func = GetOrCreateFunction("OpBase_CreateRecord");
	LLVMValueRef addRecord_func = GetOrCreateFunction("Record_Add");
	LLVMValueRef AR_EXP_Evaluate_func = GetOrCreateFunction("AR_EXP_Evaluate");

	LLVMValueRef global = GetOrCreateGlobal("op", opBase);
	LLVMValueRef local = LLVMBuildLoad2(ctx->builder, voidPtr, global, "local");
	LLVMValueRef args[] = {local};
	LLVMValueRef r = LLVMBuildCall2(ctx->builder, LLVMGetReturnType(LLVMTypeOf(createRecord_func)), createRecord_func, args, 1, "call");
	ctx->r = r;

	for(uint i = 0; i < exp_count; i++) {
		AR_ExpNode *exp = exps[i];
		char *name;
		asprintf(&name, "project_expr_%d", i);
		LLVMValueRef expr_global = GetOrCreateGlobal(name, exp);
		LLVMValueRef expr_local = LLVMBuildLoad2(ctx->builder, voidPtr, expr_global, name);
		LLVMValueRef eval_args[] = {expr_local, r};
		LLVMValueRef v = LLVMBuildCall2(ctx->builder, LLVMGetReturnType(LLVMTypeOf(AR_EXP_Evaluate_func)), AR_EXP_Evaluate_func, eval_args, 2, "call");

		int rec_idx = record_offsets[i];

		LLVMValueRef addRec_args[] = {r, LLVMConstInt(LLVMInt32Type(), rec_idx, 0), v};
		LLVMValueRef r = LLVMBuildCall2(ctx->builder, LLVMGetReturnType(LLVMTypeOf(addRecord_func)), addRecord_func, addRec_args, 3, "call");
	}
}
