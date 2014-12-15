/// \file
#ifndef _LGI_SCRIPTING_H_
#define _LGI_SCRIPTING_H_

#include "GVariant.h"

class GScriptContext;
class GScriptEngine;
class GScriptEnginePrivate;

typedef GArray<GVariant*> ArgumentArray;
typedef bool (GScriptContext::*ScriptCmd)(GVariant *Ret, ArgumentArray &Args);

/// Execution status
enum GExecutionStatus
{
	ScriptError,
	ScriptWarning,
	ScriptSuccess,
};

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

	virtual GExecutionStatus Call(GScriptContext *Ctx, GVariant *Ret, ArgumentArray &Args) = 0;
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

	GExecutionStatus Call(GScriptContext *Ctx, GVariant *Ret, ArgumentArray &Args);
};

class GFunctionInfo : public GRefCount
{
	friend class GVirtualMachinePriv;
	friend class GCompilerPriv;
	
	static int _Infos;

	int StartAddr;
	int FrameSize;
	GVariant Name;
	GArray<GVariant> Params;

public:
	GFunctionInfo(const char *name)
	{
		StartAddr = 0;
		FrameSize = 0;
		if (name)
			Name = name;
	}

	~GFunctionInfo()
	{
	}
	
	char *GetName()
	{
		return Name.Str();
	}

	GFunctionInfo &operator =(GFunctionInfo &f)
	{
		StartAddr = f.StartAddr;
		FrameSize = f.FrameSize;
		Name = f.Name;
		for (unsigned i=0; i<f.Params.Length(); i++)
		{
			Params[i] = f.Params[i];
		}
		return *this;
	}
};

class GScriptObj
{
public:
	virtual ~GScriptObj() {}

	virtual uint32 Length() = 0;
	virtual GVariant *Set(const char *Name, GVariant &v) = 0;
	virtual const char *GetFileName() = 0;
	virtual GFunctionInfo *GetMethod(const char *Name, bool Create = false) = 0;
	virtual class GTypeDef *GetType(char16 *Name) = 0;
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
	virtual char *GetIncludeFile(char *FileName) = 0;
	virtual GAutoString GetDataFolder() { return GAutoString(); }
	virtual GStream *GetLog() { return NULL; }
	virtual bool SetLog(GStream *Log) { return false; }

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
	virtual GScriptObj *GetCurrentCode() { return NULL; }

public:
	virtual ~GScriptEngine() {}

	#define NotImplmented { LgiAssert(!"Not implemented"); }
	#define NotImplmentedRet(c) { LgiAssert(!"Not implemented"); return c; }

	/// Get the terminal output
	virtual GStream *GetConsole() NotImplmentedRet(NULL)
	virtual bool SetConsole(GStream *s) NotImplmentedRet(false)

	/// Empty the object of all current script and variables
	virtual void Empty() NotImplmented

	/// Compile a script, required before you can run a script
	virtual bool Compile(char *Script, const char *FileName, bool Add = false) NotImplmentedRet(false)
	
	/// Run a previously compiled script
	virtual GExecutionStatus Run() NotImplmentedRet(ScriptError)

	/// Compile and run a script in one step
	virtual GExecutionStatus RunTemporary(char *Script) NotImplmentedRet(ScriptError)

	/// Evaluate a single expression
	virtual bool EvaluateExpression(GVariant *Result, GDom *VariableSource, char *Expression) NotImplmentedRet(false)

	/// Create a variable
	virtual GVariant *Var(char16 *name, bool create = true) NotImplmentedRet(NULL)

	/// Call a method with the given parameters.
	virtual bool CallMethod(const char *Method, GVariant *Ret, ArgumentArray &Args) NotImplmentedRet(false)

	/// Dump the stack variables to the terminal
	virtual void DumpVariables() NotImplmented
};

/// New compiler/byte code/VM scripting engine
class GScriptEngine2
{
	class GScriptEnginePrivate2 *d;

public:
	GScriptEngine2(GViewI *parent, GScriptContext *UserContext);
	~GScriptEngine2();

	GStream *GetConsole();
	bool SetConsole(GStream *t);

	GScriptObj *CreateObj();
	bool Compile(GAutoPtr<GScriptObj> &Obj, GScriptContext *UserContext, char *Script, const char *FileName = NULL);
	GExecutionStatus Run(GScriptObj *Obj);
	GExecutionStatus RunTemporary(GScriptObj *Obj, char *Script);
	bool EvaluateExpression(GVariant *Result, GDom *VariableSource, char *Expression);
	bool CallMethod(GScriptObj *Obj, const char *Method, GVariant *Ret, ArgumentArray &Args);
	GScriptContext *GetSystemContext();
};

class GVirtualMachine;

class GVmDebugger : public GDom
{
public:
	/// Set the VM ownership flag.
	virtual void OwnVm(bool Own) = 0;
	/// Set the source and asm
	virtual void SetSource(const char *Mixed) = 0;
	
	// Events
		/// Called to update the debugger UI to the new current execution position
		virtual void OnAddress(int Addr) = 0;
		/// Called when an error occurs executing the script
		virtual void OnError(const char *Msg) = 0;
		/// Called when execution starts or ends
		virtual void OnRun(bool Running) = 0;
};

class GVmDebuggerCallback : public GDom
{
public:
	/// Start a debugger instance to handle the execution in 'Vm'
	virtual GVmDebugger *AttachVm(GVirtualMachine *Vm, const char *Script, const char *Assembly) = 0;
	/// Compile a new script
	virtual bool CompileScript(GAutoPtr<GScriptObj> &Output, const char *FileName, const char *Source) = 0;
};

/// Debugger for vm script
class GVmDebuggerWnd : public GWindow, public GVmDebugger
{
	struct GScriptVmDebuggerPriv *d;

public:
	GVmDebuggerWnd(GView *Parent, GVmDebuggerCallback *Callback, GVirtualMachine *Vm, const char *Script, const char *Assembly);
	~GVmDebuggerWnd();

	void OwnVm(bool Own);
	void OnAddress(int Addr);
	void OnError(const char *Msg);
	void OnRun(bool Running);
	void SetSource(const char *Mixed);
	int OnNotify(GViewI *Ctrl, int Flags);
	GMessage::Param OnEvent(GMessage *Msg);
	void LoadFile(const char *File);
};

#endif

