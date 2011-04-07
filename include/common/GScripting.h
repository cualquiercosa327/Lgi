/// \file
#ifndef _LGI_SCRIPTING_H_
#define _LGI_SCRIPTING_H_

#include "GVariant.h"

class GScriptContext;
class GScriptEngine;
class GScriptEnginePrivate;
class GCompiledCode;

typedef GArray<GVariant*> ArgumentArray;
typedef bool (GScriptContext::*ScriptCmd)(GVariant *Ret, ArgumentArray &Args);

/// Various type of methods
enum GFuncType
{
	/// No method type.
	NullFunc,
	/// This method is provided by the hosting application.
	HostFunc,
	/// This method is defined in the script itself.
	ScriptFunc,
	/// This method is defined in an external library.
	ExternFunc
};

struct GFunc
{
	GFuncType Type;
	char *Method;

	GFunc(const char *m = 0, GFuncType t = NullFunc)
	{
		Type = t;
		Method = NewStr(m);
	}

	virtual ~GFunc()
	{
		DeleteArray(Method);
	}

	virtual bool Call(GScriptContext *Ctx, GVariant *Ret, ArgumentArray &Args) = 0;
};

struct GHostFunc : public GFunc
{
	GScriptContext *Context;
	char *Args;
	ScriptCmd Func;
	
	GHostFunc(const char *method, const char *args, ScriptCmd proc) : GFunc(method, HostFunc)
	{
		Args = NewStr(args);
		Func = proc;
	}

	~GHostFunc()
	{
		DeleteArray(Args);
	}

	bool Call(GScriptContext *Ctx, GVariant *Ret, ArgumentArray &Args);
};

class GScriptUtils
{
public:
	virtual ~GScriptUtils() {}

	int atoi(char16 *s);
	int64 atoi64(char16 *s);
	double atof(char16 *s);
	int htoi(char16 *s);
};

/// The base context class for scripting engines
class GScriptContext : public GScriptUtils
{
public:
	virtual ~GScriptContext() {}
	
	virtual GHostFunc *GetCommands() = 0;
	virtual void SetEngine(GScriptEngine *Eng) = 0;
	virtual char *GetIncludeFile(char *FileName) = 0;
	virtual GAutoString GetDataFolder() { return GAutoString(); }

	// AddPrimitive: Add your primitive's functions in a derived class using the format:
	//
	//		bool MyPrim(GVariant *Ret, ArgumentArray &Args) { ... }
	//
	// Where you return true if successful or false on error.
};

/// A scripting engine interface
class GScriptEngine
{
	friend class SystemFunctions;

protected:
	virtual GCompiledCode *GetCurrentCode() { return 0; }

public:
	virtual ~GScriptEngine() {}

	#define NotImplmented { LgiAssert(!"Not implemented"); }
	#define NotImplmentedRet0 { LgiAssert(!"Not implemented"); return 0; }

	/// Empty the object of all current script and variables
	virtual void Empty() NotImplmented

	/// Compile a script, required before you can run a script
	virtual bool Compile(char *Script, bool Add = false) NotImplmentedRet0
	
	/// Run a previously compiled script
	virtual bool Run() NotImplmentedRet0

	/// Compile and run a script in one step
	virtual bool RunTemporary(char *Script) NotImplmentedRet0

	/// Evaluate a single expression
	virtual bool EvaluateExpression(GVariant *Result, GDom *VariableSource, char *Expression) NotImplmentedRet0

	/// Create a variable
	virtual GVariant *Var(char16 *name, bool create = true) NotImplmentedRet0

	/// Get the terminal output
	virtual GStringPipe *GetTerm() NotImplmentedRet0

	/// Call a method with the given parameters.
	virtual bool CallMethod(char *Method, GVariant *Ret, ArgumentArray &Args) NotImplmentedRet0

	/// Dump the stack variables to the terminal
	virtual void DumpVariables() NotImplmented
};

/// Old interpreter scripting engine
class GScriptEngine1 : public GScriptEngine
{
	friend class GScriptEval;

	GScriptEnginePrivate *d;

public:
	GScriptEngine1(GViewI *parent, GScriptContext *context);
	~GScriptEngine1();

	void Empty();
	bool Compile(char *Script, bool Add = false);
	bool Run();
	bool RunTemporary(char *Script);
	bool EvaluateExpression(GVariant *Result, GDom *VariableSource, char *Expression);
	GVariant *Var(char16 *name, bool create = true);
	GStringPipe *GetTerm();
	bool CallMethod(char *Method, GVariant *Ret, ArgumentArray &Args);
	void DumpVariables();
};

/// New compiler/byte code/VM scripting engine
class GScriptEngine2 : public GScriptEngine
{
	class GScriptEnginePrivate2 *d;

	GCompiledCode *GetCurrentCode();

public:
	GScriptEngine2(GViewI *parent, GScriptContext *context);
	~GScriptEngine2();

	void Empty();
	bool Compile(char *Script, bool Add = false);
	bool Run();
	bool RunTemporary(char *Script);
	bool EvaluateExpression(GVariant *Result, GDom *VariableSource, char *Expression);
	GVariant *Var(char16 *name, bool create = true);
	GStringPipe *GetTerm();
	bool CallMethod(char *Method, GVariant *Ret, ArgumentArray &Args);
	void DumpVariables();
};

#endif

