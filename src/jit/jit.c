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
		ctx = rm_calloc(1, sizeof(EmitCtx));
		
		LLVMInitializeCore(LLVMGetGlobalPassRegistry());

		ctx->TSCtx = LLVMOrcCreateNewThreadSafeContext();
		ctx->Ctx = LLVMOrcThreadSafeContextGetContext(ctx->TSCtx);
		ctx->module = LLVMModuleCreateWithNameInContext("query", ctx->Ctx);

		LLVMTypeRef p[0];
		LLVMTypeRef func_type = LLVMFunctionType(LLVMVoidTypeInContext(ctx->Ctx), p, 0, false);
		LLVMValueRef query = LLVMAddFunction(ctx->module, "query", func_type);
	
		LLVMBasicBlockRef entry = LLVMAppendBasicBlockInContext(ctx->Ctx, query, "entry");

		ctx->builder = LLVMCreateBuilder();
		LLVMPositionBuilderAtEnd(ctx->builder, entry);

		ctx->voidPtr = LLVMPointerType(LLVMVoidTypeInContext(ctx->Ctx), 0);
		ctx->i32 = LLVMInt32TypeInContext(ctx->Ctx);
		ctx->i64 = LLVMInt64TypeInContext(ctx->Ctx);

		LLVMTypeRef x[] = {ctx->i64, ctx->i32, ctx->i32};
		ctx->si_type = LLVMStructTypeInContext(ctx->Ctx, x, 3, 0);
		LLVMTypeRef node_x[] = {ctx->voidPtr, ctx->i64, ctx->voidPtr, ctx->i32};
		ctx->node_type = LLVMStructTypeInContext(ctx->Ctx, node_x, 4, 0);
		// Set a new thread-local EmitCtx if one has not been created.
		pthread_setspecific(_tlsEmitCtxKey, ctx);
	}
	return ctx;    
}

static LLVMValueRef GetOrCreateFunction(const char* name) {
	EmitCtx *ctx = EmitCtx_Get();

	LLVMValueRef f = LLVMGetNamedFunction(ctx->module, name);
	if(f) return f;

	if(strcmp(name, "ResultSet_AddRecord") == 0) {
		LLVMTypeRef addRecord_params[] = { ctx->voidPtr, ctx->voidPtr }; 
		LLVMTypeRef addRecord_type = LLVMFunctionType(ctx->i32, addRecord_params, 2, 0);

		return LLVMAddFunction(ctx->module, "ResultSet_AddRecord", addRecord_type);
	}

	if(strcmp(name, "OpBase_CreateRecord") == 0) {
		LLVMTypeRef createRecord_params[] = { ctx->voidPtr }; 
		LLVMTypeRef createRecord_type = LLVMFunctionType(ctx->voidPtr, createRecord_params, 1, 0);

		return LLVMAddFunction(ctx->module, "OpBase_CreateRecord", createRecord_type);
	}

	if(strcmp(name, "AR_EXP_Evaluate") == 0) {
		LLVMTypeRef AR_EXP_Evaluate_params[] = { ctx->voidPtr, ctx->voidPtr }; 
		LLVMTypeRef AR_EXP_Evaluate_type = LLVMFunctionType(ctx->si_type, AR_EXP_Evaluate_params, 2, 0);

		return LLVMAddFunction(ctx->module, "AR_EXP_Evaluate", AR_EXP_Evaluate_type);
	}

	if(strcmp(name, "Record_Add") == 0) {
		LLVMTypeRef addRecord_params[] = { ctx->voidPtr, ctx->i32, ctx->si_type }; 
		LLVMTypeRef addRecord_type = LLVMFunctionType(ctx->voidPtr, addRecord_params, 3, 0);

		return LLVMAddFunction(ctx->module, "Record_Add", addRecord_type);
	}

	if(strcmp(name, "RG_MatrixTupleIter_next") == 0) {
		LLVMTypeRef i64ptr = LLVMPointerType(ctx->i64, 0);
		LLVMTypeRef boolPtr = LLVMPointerType(LLVMInt8Type(), 0);
		LLVMTypeRef iter_next_params[] = { ctx->voidPtr, i64ptr, i64ptr, ctx->voidPtr, boolPtr }; 
		LLVMTypeRef iter_next_type = LLVMFunctionType(ctx->i32, iter_next_params, 5, 0);

		return LLVMAddFunction(ctx->module, "RG_MatrixTupleIter_next", iter_next_type);
	}

	if(strcmp(name, "Graph_GetNode") == 0) {
		LLVMTypeRef iter_next_params[] = { ctx->voidPtr, ctx->i64, LLVMPointerType(ctx->node_type, 0) }; 
		LLVMTypeRef iter_next_type = LLVMFunctionType(ctx->i32, iter_next_params, 3, 0);

		return LLVMAddFunction(ctx->module, "Graph_GetNode", iter_next_type);
	}

	if(strcmp(name, "Record_AddNode") == 0) {
		LLVMTypeRef addNode_params[] = { ctx->voidPtr, ctx->i32, LLVMPointerType(ctx->node_type, 0) }; 
		LLVMTypeRef addNode_type = LLVMFunctionType(ctx->i32, addNode_params, 3, 0);

		return LLVMAddFunction(ctx->module, "Record_AddNode", addNode_type);
	}

	return NULL;
}

static LLVMValueRef GetOrCreateGlobal(const char* name, void* value) {
	EmitCtx *ctx = EmitCtx_Get();
	LLVMValueRef global = LLVMGetNamedGlobal(ctx->module, name);
	if(global) return global;

	global = LLVMAddGlobal(ctx->module, ctx->voidPtr, name);

	LLVMSetGlobalConstant(global, 1);
	LLVMSetInitializer(global, LLVMConstIntToPtr(LLVMConstInt(ctx->i64, value, 0), ctx->voidPtr));

	return global;
}

void JIT_Init(void *g) {
	LLVMTypeRef voidPtr = LLVMPointerType(LLVMVoidType(), 0);

	EmitCtx *ctx = EmitCtx_Get();

	ctx->g = GetOrCreateGlobal(str_g, g);
}

void JIT_End() {
	EmitCtx *ctx = EmitCtx_Get();

	LLVMBuildRetVoid(ctx->builder);

	ctx->TSM = LLVMOrcCreateNewThreadSafeModule(ctx->module, ctx->TSCtx);

    LLVMOrcDisposeThreadSafeContext(ctx->TSCtx);

	char *m = LLVMPrintModuleToString(ctx->module);
	printf("%s\n", m);

	char *error;

	LLVMVerifyModule(ctx->module, LLVMAbortProcessAction, &error);
	LLVMDisposeMessage(error);
}

static void handleError(LLVMErrorRef Err) {
	char *ErrMsg = LLVMGetErrorMessage(Err);
	fprintf(stderr, "Error: %s\n", ErrMsg);
	LLVMDisposeErrorMessage(ErrMsg);
}

static LLVMErrorRef definitionGeneratorFn(LLVMOrcDefinitionGeneratorRef G, void *Ctx,
                        LLVMOrcLookupStateRef *LS, LLVMOrcLookupKind K,
                        LLVMOrcJITDylibRef JD, LLVMOrcJITDylibLookupFlags F,
                        LLVMOrcCLookupSet Names, size_t NamesCount) {
	SymbolResolve fn = Ctx;
    for (size_t I = 0; I < NamesCount; I++) {
      LLVMOrcCLookupSetElement Element = Names[I];
      LLVMOrcJITTargetAddress Addr = (LLVMOrcJITTargetAddress)fn(LLVMOrcSymbolStringPoolEntryStr(Element.Name));
	  if(!Addr) continue;
      LLVMJITSymbolFlags Flags = {LLVMJITSymbolGenericFlagsWeak, 0};
      LLVMJITEvaluatedSymbol Sym = {Addr, Flags};
      LLVMOrcRetainSymbolStringPoolEntry(Element.Name);
      LLVMJITCSymbolMapPair Pair = {Element.Name, Sym};
      LLVMJITCSymbolMapPair Pairs[] = {Pair};
      LLVMOrcMaterializationUnitRef MU = LLVMOrcAbsoluteSymbols(Pairs, 1);
      LLVMErrorRef Err = LLVMOrcJITDylibDefine(JD, MU);
      if (Err)
        return Err;
    }
    return LLVMErrorSuccess;
}

void JIT_Run(SymbolResolve fn) {
	EmitCtx *ctx = EmitCtx_Get();

	LLVMInitializeNativeTarget();
	LLVMInitializeNativeAsmPrinter();

	LLVMOrcLLJITRef J;
	{
		LLVMErrorRef Err;
		if ((Err = LLVMOrcCreateLLJIT(&J, 0))) {
			handleError(Err);
			goto llvm_shutdown;
		}
	}

	LLVMOrcJITDylibRef MainJD = LLVMOrcLLJITGetMainJITDylib(J);
	LLVMErrorRef Err;
	if ((Err = LLVMOrcLLJITAddLLVMIRModule(J, MainJD, ctx->TSM))) {
		LLVMOrcDisposeThreadSafeModule(ctx->TSM);
		handleError(Err);
		goto jit_cleanup;
	}

	// LLVMOrcDefinitionGeneratorRef ProcessSymbolsGenerator = 0;
	// if ((Err = LLVMOrcCreateDynamicLibrarySearchGeneratorForProcess(
	// 		 &ProcessSymbolsGenerator, LLVMOrcLLJITGetGlobalPrefix(J),
	// 		 NULL, NULL))) {
	//   handleError(Err);
	//   goto jit_cleanup;
	// }

	// LLVMOrcJITDylibAddGenerator(LLVMOrcLLJITGetMainJITDylib(J), ProcessSymbolsGenerator);

	LLVMOrcDefinitionGeneratorRef Gen = LLVMOrcCreateCustomCAPIDefinitionGenerator(definitionGeneratorFn, fn);
	LLVMOrcJITDylibAddGenerator(LLVMOrcLLJITGetMainJITDylib(J), Gen);

	LLVMOrcJITTargetAddress QueryAddr;
	{
		LLVMErrorRef Err;
		if ((Err = LLVMOrcLLJITLookup(J, &QueryAddr, "query"))) {
			handleError(Err);
			goto jit_cleanup;
		}
	}

	int a = 1;

	void (*func)() = (void (*)(void))QueryAddr;
	func();

	LLVMDisposeBuilder(ctx->builder);

jit_cleanup:
	if ((Err = LLVMOrcDisposeLLJIT(J))) {
		handleError(Err);
	}

llvm_shutdown:
	LLVMShutdown();
}

void JIT_CreateRecord(void *opBase) {
	EmitCtx *ctx = EmitCtx_Get();

	LLVMValueRef createRecord_func = GetOrCreateFunction(str_CreateRecord);

	LLVMValueRef global = GetOrCreateGlobal(str_op, opBase);
	LLVMValueRef local = LLVMBuildLoad2(ctx->builder, ctx->voidPtr, global, "local");
	LLVMValueRef args[] = {local};
	LLVMValueRef r = LLVMBuildCall2(ctx->builder, LLVMGetReturnType(LLVMTypeOf(createRecord_func)), createRecord_func, args, 1, "call");
	ctx->r = r;
}

void JIT_Result(void *rsVal) {
	EmitCtx *ctx = EmitCtx_Get();

	LLVMValueRef addRecord_func = GetOrCreateFunction(str_AddRecord);
	LLVMValueRef rs = GetOrCreateGlobal(str_rs, rsVal);
	LLVMValueRef local = LLVMBuildLoad2(ctx->builder, ctx->voidPtr, rs, "local");


	LLVMValueRef args[] = {local, ctx->r};
	LLVMBuildCall2(ctx->builder, LLVMGetReturnType(LLVMTypeOf(addRecord_func)), addRecord_func, args, 2, "call");

	JIT_CreateRecord(NULL);
}

void JIT_Project(void *opBase, AR_ExpNode **exps, uint exp_count, uint *record_offsets) {
	EmitCtx *ctx = EmitCtx_Get();

	LLVMValueRef addRecord_func = GetOrCreateFunction(str_Record_Add);
	LLVMValueRef AR_EXP_Evaluate_func = GetOrCreateFunction(str_AR_EXP_Evaluate);

	for(uint i = 0; i < exp_count; i++) {
		AR_ExpNode *exp = exps[i];
		char *name;
		asprintf(&name, "project_expr_%d", i);
		LLVMValueRef expr_global = GetOrCreateGlobal(name, exp);
		LLVMValueRef expr_local = LLVMBuildLoad2(ctx->builder, ctx->voidPtr, expr_global, name);
		LLVMValueRef eval_args[] = {expr_local, ctx->r};
		LLVMValueRef v = LLVMBuildCall2(ctx->builder, LLVMGetReturnType(LLVMTypeOf(AR_EXP_Evaluate_func)), AR_EXP_Evaluate_func, eval_args, 2, "call");

		int rec_idx = record_offsets[i];

		LLVMValueRef addRec_args[] = {ctx->r, LLVMConstInt(ctx->i32, rec_idx, 0), v};
		LLVMValueRef r = LLVMBuildCall2(ctx->builder, LLVMGetReturnType(LLVMTypeOf(addRecord_func)), addRecord_func, addRec_args, 3, "call");
	}
}

void JIT_LabelScan(void *iter, int nodeIdx) {
	EmitCtx *ctx = EmitCtx_Get();

	LLVMTypeRef i64ptr = LLVMPointerType(ctx->i64, 0);

	LLVMValueRef iter_next_func = GetOrCreateFunction(str_RG_MatrixTupleIter_next);
	LLVMValueRef getNode_func = GetOrCreateFunction(str_Graph_GetNode);
	LLVMValueRef addNode_func = GetOrCreateFunction(str_Record_AddNode);

	LLVMValueRef iter_global = GetOrCreateGlobal("iter", iter);
	LLVMValueRef iter_local = LLVMBuildLoad2(ctx->builder, ctx->voidPtr, iter_global, "iter");

	LLVMValueRef nodeId = LLVMBuildAlloca(ctx->builder, ctx->i64, "nodeId");
	LLVMValueRef depleted = LLVMBuildAlloca(ctx->builder, LLVMInt8Type(), "depleted");

	LLVMValueRef iter_next_params[] = {iter_local, LLVMConstPointerNull(i64ptr), nodeId, LLVMConstPointerNull(ctx->voidPtr), depleted};
	LLVMBuildCall2(ctx->builder, LLVMGetReturnType(LLVMTypeOf(iter_next_func)), iter_next_func, iter_next_params, 5, "call");

	LLVMValueRef node = LLVMBuildAlloca(ctx->builder, ctx->node_type, "node");
	LLVMValueRef g_local = LLVMBuildLoad2(ctx->builder, ctx->voidPtr, ctx->g, "g");
	LLVMValueRef nodeId_load = LLVMBuildLoad(ctx->builder, nodeId, "nodeId_load");
	LLVMValueRef getNode_params[] = {g_local, nodeId_load, node};
	LLVMBuildCall2(ctx->builder, LLVMGetReturnType(LLVMTypeOf(getNode_func)), getNode_func, getNode_params, 3, "call");

	LLVMValueRef addNode_params[] = {ctx->r, LLVMConstInt(ctx->i32, nodeIdx, 0), node};
	LLVMBuildCall2(ctx->builder, LLVMGetReturnType(LLVMTypeOf(addNode_func)), addNode_func, addNode_params, 3, "call");
}