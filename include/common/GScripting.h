/// \file
#ifndef _LGI_SCRIPTING_H_
#define _LGI_SCRIPTING_H_

#include "GVariant.h"
#include "LList.h"

class GScriptContext;
class GScriptEnginePrivate;
class GVmDebuggerCallback;
class GVirtualMachine;

class LScriptArguments : public GArray<GVariant*>
{
	friend class GScriptEngine;
	friend class GVirtualMachine;

	GVirtualMachine *Vm;
	GStream *Console;
	GVariant _Return;
	GVariant *PtrRet;

public:
	static GStream NullConsole;

	LScriptArguments(GVirtualMachine *vm, GVariant *ret = NULL, GStream *console = NULL)
	{
		Vm = vm;
		if (ret)
			PtrRet = ret;
		else
			PtrRet = &_Return;
		
		if (console)
			Console = console;
		else
			Console = &NullConsole;
	}

	GVariant *GetReturn() { return PtrRet; }
	GStream *GetConsole() { return Console; }
	bool Throw(const char *File, int Line, const char *Msg, ...);
};

typedef bool (GScriptContext::*ScriptCmd)(LScriptArguments &Args);

#define SCOPE_REGISTER		0
#define SCOPE_LOCAL			1
#define SCOPE_GLOBAL		2
#define SCOPE_OBJECT		3
#define SCOPE_RETURN		4
#define SCOPE_MAX			5

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
	GString Method;

	GFunc(const char *m = 0, GFuncType t = NullFunc)
	{
		Type = t;
		Method = m;
	}

	virtual ~GFunc()
	{
	}

	virtual GExecutionStatus Call(GScriptContext *Ctx, LScriptArguments &Args) = 0;
};

struct GHostFunc : public GFunc
{
	GScriptContext *Context;
	GString Args;
	ScriptCmd Func;
	
	GHostFunc(const GHostFunc &f)
	{
		Context = f.Context;
		Args = f.Args;
		Method = f.Method;
		Func = f.Func;
	}
	
	GHostFunc(const char *method, const char *args, ScriptCmd proc) : GFunc(method, HostFunc)
	{
		Args = args;
		Func = proc;
	}

	GExecutionStatus Call(GScriptContext *Ctx, LScriptArguments &Args) override;
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

	GExecutionStatus Call(GScriptContext *Ctx, LScriptArguments &Args) override;
};

class GFunctionInfo : public GRefCount
{
	friend class GVirtualMachinePriv;
	friend class GCompilerPriv;
	
	static int _Infos;

	int StartAddr;
	int FrameSize;
	GString Name;
	GArray<GString> Params;

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
		return Name;
	}
	
	size_t GetParams()
	{
		return Params.Length();
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
	GCustomType *Obj;
	
	GVariables(int scope)
	{
		Scope = scope;
		NullIndex = -1;
		Obj = NULL;
	}
	
	GVariables(GCustomType *obj)
	{
		Scope = SCOPE_OBJECT;
		NullIndex = -1;
		Obj = obj;
	}

	int Var(const char *n, bool create = false)
	{
		if (Obj)
		{
			return Obj->IndexOf(n);
		}
		else
		{
			int p = Lut.Find(n);
			if (p)
			{
				return p - 1;
			}

			if (create)
			{
				int Len = (int)Length();

				Lut.Add(n, Len + 1);
				Length(Len + 1);

				return Len;
			}
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
	friend class GVmDebuggerWnd;

	/// The global variables
	GVariables Globals;
	
	/// The byte code of all the instructions
	GArray<uint8> ByteCode;
	
	/// All the methods defined in the byte code and their arguments.
	GArray< GAutoRefPtr<GFunctionInfo> > Methods;
	
	/// All the externs defined in the code.
	GArray<GExternFunc*> Externs;
	
	/// All the user types defined
	GHashTbl<char16*, class GCustomType*> Types;
	
	/// The original script details
	GString FileName;
	GString Source;
	
	/// The system context (all the system functions)
	GScriptContext *SysContext;
	
	/// Any user context (application functions)
	GScriptContext *UserContext;

	/// Debug info to map instruction address back to source line numbers
	GHashTbl<NativeInt, int> Debug;

public:
	GCompiledCode();
	GCompiledCode(GCompiledCode &copy);
	~GCompiledCode();

	/// Size of the byte code
	size_t Length() { return ByteCode.Length(); }
	/// Assignment operator
	GCompiledCode &operator =(GCompiledCode &c);
	/// Gets a method defined in the code
	GFunctionInfo *GetMethod(const char *Name, bool Create = false);
	/// Sets a global variable
	GVariant *Set(const char *Name, GVariant &v);
	/// Gets the definition of a struct or custom type
	GCustomType *GetType(char16 *Name) { return Types.Find(Name); }
	/// Gets the file name this code was compiled from
	const char *GetFileName() { return FileName; }
	/// Gets the source
	const char *GetSource() { return Source; }
	/// Gets the source line number associated with an address
	int ObjectToSourceAddress(size_t ObjAddr);
	/// Turns an object address into a FileName:LineNumber string.
	const char *AddrToSourceRef(size_t ObjAddr);

	/// Sets the file name this code was compiled from
	void SetSource(const char *file, const char *src)
	{
		if (file != FileName.Get())
			FileName = file;
		Source = src;
	}
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
	GExecutionStatus Run(GCompiledCode *Obj, GVariant *Ret = NULL, const char *TempPath = NULL);
	GExecutionStatus RunTemporary(GCompiledCode *Obj, char *Script, GVariant *Ret = NULL);
	bool EvaluateExpression(GVariant *Result, GDom *VariableSource, char *Expression);
	bool CallMethod(GCompiledCode *Obj, const char *Method, LScriptArguments &Args);
	GScriptContext *GetSystemContext();
};

class GVirtualMachine;

class GVmDebugger : public GDom
{
public:
	/// Set the VM ownership flag.
	virtual void OwnVm(bool Own) = 0;
	/// Makes the debugger the owner of the compiled code
	virtual void OwnCompiledCode(GAutoPtr<GCompiledCode> Cc) = 0;
	/// Gets the code owned by the debugger
	virtual GCompiledCode *GetCode() = 0;
	/// Set the source and asm
	virtual void SetSource(const char *Mixed) = 0;
	/// Show UI and wait for user response
	virtual void Run() = 0;
	
	// Events
		/// Called to update the debugger UI to the new current execution position
		virtual void OnAddress(size_t Addr) = 0;
		/// Called when an error occurs executing the script
		virtual void OnError(const char *Msg) = 0;
		/// Called when execution starts or ends
		virtual void OnRun(bool Running) = 0;
};

class GVmDebuggerCallback : public GDom
{
public:
	/// Start a debugger instance to handle the execution in 'Vm'
	virtual GVmDebugger *AttachVm(GVirtualMachine *Vm, GCompiledCode *Code, const char *Assembly) = 0;
	/// Compile a new script
	virtual bool CompileScript(GAutoPtr<GCompiledCode> &Output, const char *FileName, const char *Source) = 0;
};

/// Debugger for vm script
class GVmDebuggerWnd : public GWindow, public GVmDebugger
{
	struct GScriptVmDebuggerPriv *d;

	void UpdateVariables(LList *Lst, GVariant *Arr, ssize_t Len, char Prefix);

public:
	GVmDebuggerWnd(GView *Parent, GVmDebuggerCallback *Callback, GVirtualMachine *Vm, GCompiledCode *Code, const char *Assembly);
	~GVmDebuggerWnd();

	void OwnVm(bool Own);
	void OnAddress(size_t Addr);
	void OnError(const char *Msg);
	void OnRun(bool Running);
	void SetSource(const char *Mixed);
	int OnNotify(GViewI *Ctrl, int Flags);
	int OnCommand(int Cmd, int Event, OsView Wnd);
	bool OnRequestClose(bool OsShuttingDown);
	GMessage::Param OnEvent(GMessage *Msg);
	void LoadFile(const char *File);
	GStream *GetLog();
	void OwnCompiledCode(GAutoPtr<GCompiledCode> Cc);
	GCompiledCode *GetCode();
	
	void Run();
};

#endif

