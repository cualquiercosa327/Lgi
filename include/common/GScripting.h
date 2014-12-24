/// \file
#ifndef _LGI_SCRIPTING_H_
#define _LGI_SCRIPTING_H_

#include "GVariant.h"

class GScriptContext;
class GScriptEnginePrivate;
class GVmDebuggerCallback;

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
	GAutoString Method;

	GFunc(const char *m = 0, GFuncType t = NullFunc)
	{
		Type = t;
		Method.Reset(NewStr(m));
	}

	virtual ~GFunc()
	{
	}

	virtual GExecutionStatus Call(GScriptContext *Ctx, GVariant *Ret, ArgumentArray &Args) = 0;
};

struct GHostFunc : public GFunc
{
	GScriptContext *Context;
	GAutoString Args;
	ScriptCmd Func;
	
	GHostFunc(const GHostFunc &f)
	{
		Context = f.Context;
		Args.Reset(NewStr(f.Args));
		Method.Reset(NewStr(f.Method));
		Func = f.Func;
	}
	
	GHostFunc(const char *method, const char *args, ScriptCmd proc) : GFunc(method, HostFunc)
	{
		Args.Reset(NewStr(args));
		Func = proc;
	}

	GExecutionStatus Call(GScriptContext *Ctx, GVariant *Ret, ArgumentArray &Args);
};

struct GExternFunc : public GFunc
{
	struct ExternType
	{
		int Ptr;
		bool Unsigned;
		bool Out;
		int ArrayLen;
		GVariantType Base;
	};

	GAutoString Lib;
	ExternType ReturnType;
	GArray<ExternType> ArgType;

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

class GVariables : public GArray<GVariant>
{
	friend class GVirtualMachinePriv;

	GHashTbl<const char*,int> Lut;

public:
	int Scope;
	int NullIndex;

	GVariables(int scope)
	{
		Scope = scope;
		NullIndex = -1;
	}

	int Var(const char *n, bool create = false)
	{
		int p = Lut.Find(n);
		if (p)
		{
			return p - 1;
		}

		if (create)
		{
			int Len = Length();

			Lut.Add(n, Len + 1);
			Length(Len + 1);

			return Len;
		}

		return -1;
	}
};

/// A block of compile byte code
class GCompiledCode
{
	friend class GCompilerPriv;
	friend class GVirtualMachinePriv;
	friend class GCompiler;

	/// The global variables
	GVariables Globals;
	
	/// The byte code of all the instructions
	GArray<uint8> ByteCode;
	
	/// All the methods defined in the byte code and their arguments.
	GArray< GAutoRefPtr<GFunctionInfo> > Methods;
	
	/// All the externs defined in the code.
	GArray<GExternFunc*> Externs;
	
	/// All the user types defined
	GHashTbl<char16*, class GTypeDef*> Types;
	
	/// The original script filename
	GAutoString FileName;
	
	/// The system context (all the system functions)
	GScriptContext *SysContext;
	
	/// Any user context (application functions)
	GScriptContext *UserContext;

	/// Debug info to map instruction address back to source line numbers
	GHashTbl<int, int> Debug;

public:
	GCompiledCode();
	GCompiledCode(GCompiledCode &copy);
	~GCompiledCode();

	/// Size of the byte code
	uint32 Length() { return ByteCode.Length(); }
	/// Assignment operator
	GCompiledCode &operator =(GCompiledCode &c);
	/// Gets a method defined in the code
	GFunctionInfo *GetMethod(const char *Name, bool Create = false);
	/// Sets a global variable
	GVariant *Set(const char *Name, GVariant &v);
	/// Gets the definition of a struct or custom type
	GTypeDef *GetType(char16 *Name) { return Types.Find(Name); }
	/// Sets the file name this code was compiled from
	void SetFileName(const char *f) { if (f != FileName) FileName.Reset(NewStr(f)); }
	/// Gets the file name this code was compiled from
	const char *GetFileName() { return FileName; }
	/// Gets the source line number associated with an address
	int ObjectToSourceAddress(int ObjAddr);
	/// Turns an object address into a FileName:LineNumber string.
	const char *AddrToSourceRef(int ObjAddr);
};

/// New compiler/byte code/VM scripting engine
class GScriptEngine
{
	class GScriptEnginePrivate *d;

public:
	GScriptEngine(GViewI *parent, GScriptContext *UserContext, GVmDebuggerCallback *Callback);
	~GScriptEngine();

	GStream *GetConsole();
	bool SetConsole(GStream *t);

	GCompiledCode *GetCurrentCode();
	bool Compile(	GAutoPtr<GCompiledCode> &Obj,
					GScriptContext *UserContext,
					char *Script,
					const char *FileName = NULL,
					GDom *Args = NULL);
	GExecutionStatus Run(GCompiledCode *Obj);
	GExecutionStatus RunTemporary(GCompiledCode *Obj, char *Script);
	bool EvaluateExpression(GVariant *Result, GDom *VariableSource, char *Expression);
	bool CallMethod(GCompiledCode *Obj, const char *Method, GVariant *Ret, ArgumentArray &Args);
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
	virtual bool CompileScript(GAutoPtr<GCompiledCode> &Output, const char *FileName, const char *Source) = 0;
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
	GStream *GetLog();
};

#endif

