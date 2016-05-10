#ifndef _GSCRIPTING_PRIV_H_
#define _GSCRIPTING_PRIV_H_

#include <stdio.h>
#include "GScripting.h"
#include "GRefCount.h"

// Instructions
#define _i(name, opcode, desc) \
	name = opcode,

#define AllInstructions \
	_i(INop,				0,					"Nop") \
	_i(IAssign,				OpAssign,			"OpAssign") \
	_i(IPlus,				OpPlus,				"OpPlus") \
	_i(IUnaryPlus,			OpUnaryPlus,		"OpUnaryPlus") \
	_i(IMinus,				OpMinus,			"OpMinus") \
	_i(IUnaryMinus,			OpUnaryMinus,		"OpUnaryMinus") \
	_i(IMul,				OpMul,				"OpMul") \
	_i(IDiv,				OpDiv,				"OpDiv") \
	_i(IMod,				OpMod,				"OpMod") \
	_i(ILessThan,			OpLessThan,			"OpLessThan") \
	_i(ILessThanEqual,		OpLessThanEqual,	"OpLessThanEqual") \
	_i(IGreaterThan,		OpGreaterThan,		"OpGreaterThan") \
	_i(IGreaterThanEqual,	OpGreaterThanEqual,	"OpGreaterThanEqual") \
	_i(IEquals,				OpEquals,			"OpEquals") \
	_i(INotEquals,			OpNotEquals,		"OpNotEquals") \
	_i(IPlusEquals,			OpPlusEquals,		"OpPlusEquals") \
	_i(IMinusEquals,		OpMinusEquals,		"OpMinusEquals") \
	_i(IMulEquals,			OpMulEquals,		"OpMulEquals") \
	_i(IDivEquals,			OpDivEquals,		"OpDivEquals") \
	_i(IPostInc,			OpPostInc,			"OpPostInc") \
	_i(IPostDec,			OpPostDec,			"OpPostDec") \
	_i(IPreInc,				OpPreInc,			"OpPreInc") \
	_i(IPreDec,				OpPreDec,			"OpPreDec") \
	_i(IAnd,				OpAnd,				"OpAnd") \
	_i(IOr,					OpOr,				"OpOr") \
	_i(INot,				OpNot,				"OpNot") \
	\
	/** Calls a another part of the script */ \
	_i(ICallScript,			64,					"CallScript") \
	/** Calls a method defined by the script context */ \
	_i(ICallMethod,			65,					"CallMethod") \
	_i(IDomGet,				67,					"DomGet") \
	_i(IDomSet,				68,					"DomSet") \
	_i(IPush,				69,					"Push") \
	_i(IPop,				70,					"Pop") \
	_i(IJump,				71,					"Jump") \
	_i(IJumpZero,			72,					"JumpZ") \
	_i(IArrayGet,			73,					"ArrayGet") \
	_i(IArraySet,			74,					"ArraySet") \
	_i(IRet,				75,					"Return") \
	_i(IDomCall,			76,					"DomCall") \
	_i(IBreak,				77,					"Break") \

enum GInstruction {
	AllInstructions
};

enum OperatorType
{
	OpPrefix,
	OpInfix,
	OpPostfix,
};

extern char16 sChar[];
extern char16 sInt[];
extern char16 sUInt[];
extern char16 sInt32[];
extern char16 sUInt32[];
extern char16 sInt64[];
extern char16 sHWND[];
extern char16 sDWORD[];
extern char16 sLPTSTR[];
extern char16 sLPCTSTR[];
extern char16 sElse[];
extern char16 sIf[];
extern char16 sFunction[];
extern char16 sExtern[];
extern char16 sFor[];
extern char16 sWhile[];
extern char16 sReturn[];
extern char16 sInclude[];
extern char16 sDefine[];
extern char16 sStruct[];
extern char16 sTrue[];
extern char16 sFalse[];
extern char16 sNull[];
extern char16 sOutParam[];

extern char16 sHash[];
extern char16 sPeriod[];
extern char16 sComma[];
extern char16 sSemiColon[];
extern char16 sStartRdBracket[];
extern char16 sEndRdBracket[];
extern char16 sStartSqBracket[];
extern char16 sEndSqBracket[];
extern char16 sStartCurlyBracket[];
extern char16 sEndCurlyBracket[];

extern const char *InstToString(GInstruction i);

/*
	Variable Reference:
		Can either be a:
			- stack variable
			- global variable
			- register (0 .. MAX_REGISTER - 1)

		Thus a variable reference encodes 2 pieces of information, first the scope of
		the reference (as above) and secondly the index into that scope. Scopes are
		stored as arrays of variables.

		The scope is one byte, the index is 3 bytes following, totally 4 bytes per ref.
*/

#define MAX_REGISTER		8
#define SCOPE_REGISTER		0
#define SCOPE_LOCAL			1
#define SCOPE_GLOBAL		2

/// 32bit variable reference, used to track where a variable is during compilation.
struct GVarRef
{
	/// \sa #SCOPE_REGISTER, #SCOPE_LOCAL or #SCOPE_GLOBAL
	unsigned Scope : 8;
	/// Index into scope
	int Index : 24;

	bool Valid()
	{
		return Index >= 0;
	}

	void Empty()
	{
		Scope = 0;
		Index = -1;
	}

	bool IsReg()
	{
		return Scope == SCOPE_REGISTER && Index >= 0 && Index < MAX_REGISTER;
	}

	void SetReg(int i)
	{
		Scope = SCOPE_REGISTER;
		Index = i;
	}

	bool operator ==(GVarRef &r)
	{
		return r.Scope == Scope && r.Index == Index;
	}

	bool operator !=(GVarRef &r)
	{
		return r.Scope != Scope || r.Index != Index;
	}

	const char *GetStr()
	{
		if (Index < 0)
			return "NoRef";

		#define GETSTR_BUF_SIZE 16
		static char Buf[4][GETSTR_BUF_SIZE];
		static int Cur = 0;
		static char Names[] = {'R', 'L', 'G'};
		char *b = Buf[Cur++];
		if (Cur >= 4) Cur = 0;

		LgiAssert(Scope <= SCOPE_GLOBAL);
		sprintf_s(b, GETSTR_BUF_SIZE, "%c%i", Names[Scope], Index);
		return b;		
	}
};

union GPtr
{
	uint8 *u8;
	uint16 *u16;
	uint32 *u32;
	int8 *i8;
	int16 *i16;
	int32 *i32;
	double *dbl;
	float *flt;
	GVarRef *r;
	GFunc **fn;
};

class SystemFunctions;

class GCompileTools
{
protected:
	OperatorType OpType(GOperator o)
	{
		switch (o)
		{
			case OpUnaryPlus:
			case OpUnaryMinus:
			case OpPreInc:
			case OpPreDec:
			case OpNot:
				return OpPrefix;

			case OpPostInc:
			case OpPostDec:
				return OpPostfix;
			
			default:
				return OpInfix;
		}
	}

	int GetPrecedence(GOperator o)
	{
		// Taken from:
		// http://www.cppreference.com/operator_precedence.html
		switch (o)
		{
			case OpAssign:
			case OpMinusEquals:
			case OpPlusEquals:
			case OpMulEquals:
			case OpDivEquals:
				return 16;

			case OpAnd:
				return 13;

			case OpOr:
				return 14;

			case OpEquals:
			case OpNotEquals:
				return 9;

			case OpLessThan:
			case OpLessThanEqual:
			case OpGreaterThan:
			case OpGreaterThanEqual:
				return 8;

			case OpPlus:
			case OpMinus:
				return 6;

			case OpMul:
			case OpDiv:
			case OpMod:
				return 5;

			case OpUnaryPlus:
			case OpUnaryMinus:
			case OpPreInc:
			case OpPreDec:
			case OpNot:
				return 3;
			
			case OpPostInc:
			case OpPostDec:
				return 2;
			
			case OpNull:
				return 0;
			
			default:
				LgiAssert(!"Really?");
				break;
		}

		return -1;
	}

	GOperator IsOp(char16 *s, int PrevIsOp)
	{
		if (!s) return OpNull;

		if (s[0] != 0 && !s[1])
		{
			// One character operator
			switch (*s)
			{
				case '=': return OpAssign;
				case '*': return OpMul;
				case '/': return OpDiv;
				case '<': return OpLessThan;
				case '>': return OpGreaterThan;
				case '%': return OpMod;
				case '!': return OpNot;
				case '+':
				{
					if (PrevIsOp == 0)
						return OpPlus;
					
					return OpUnaryPlus;
				}
				case '-':
				{
					if (PrevIsOp == 0)
						return OpMinus;

					return OpUnaryMinus;
				}
			}
		}
		else if (s[0] != 0 && s[1] == '=' && !s[2])
		{
			// 2 chars, "something" equals operator
			switch (*s)
			{
				case '!': return OpNotEquals;
				case '=': return OpEquals;
				case '<': return OpLessThanEqual;
				case '>': return OpGreaterThanEqual;

				case '+': return OpPlusEquals;
				case '-': return OpMinusEquals;
				case '*': return OpMulEquals;
				case '/': return OpDivEquals;
			}
		}
		else if (s[0] == '+' && s[1] == '+' && !s[2])
		{
			if (PrevIsOp == 0)
				return OpPostInc;

			return OpPreInc;
		}
		else if (s[0] == '-' && s[1] == '-' && !s[2])
		{
			if (PrevIsOp == 0)
				return OpPostDec;

			return OpPreDec;
		}
		else if (s[0] == '&' && s[1] == '&' && !s[2])
		{
			return OpAnd;
		}
		else if (s[0] == '|' && s[1] == '|' && !s[2])
		{
			return OpOr;
		}

		return OpNull;
	}

};

/// This class compiles the source down to byte code
class GCompiler : public GScriptUtils
{
	class GCompilerPriv *d;

public:
	/// Constructor
	GCompiler();
	~GCompiler();

	/// Compile the source into byte code.
	bool Compile
	(
		GAutoPtr<GCompiledCode> &Code,
		GScriptContext *SysContext,
		GScriptContext *UserContext,
		const char *FileName,
		const char *Script,
		GDom *Args
	);
};

/// This class is the VM for the byte language
class GVirtualMachine : public GScriptUtils
{
	friend class GVmDebuggerWnd;
	class GVirtualMachinePriv *d;

public:
	GVirtualMachine(GVmDebuggerCallback *callback = NULL);
	~GVirtualMachine();
	
	/// Executes the whole script starting at the top
	GExecutionStatus Execute
	(
		/// [In] The code to execute
		GCompiledCode *Code,
		/// [In] The instruction to start at... [defaults to the start of script)
		uint32 StartOffset = 0,
		/// [Optional] Log file for execution
		GStream *Log = NULL,
		/// Start the script execution straight away?
		bool StartImmediately = true,
		/// Optional return value
		GVariant *Return = NULL
	);

	/// Execute just one method and return
	GExecutionStatus ExecuteFunction
	(
		/// [In] The code to execute
		GCompiledCode *Code,
		/// [In] The function to execute
		GFunctionInfo *Func,
		/// [In] The function's arguments
		ArgumentArray &Args,
		/// [Out] The return value of the function
		GVariant *Ret,
		/// [Optional] Log file for execution
		GStream *Log = NULL
	);

	// Debugging commands
	GVmDebugger *OpenDebugger(const char *Script = NULL);
	bool StepInto();
	bool StepOver();
	bool StepOut();
	bool BreakExecution();
	bool Continue();
	bool Stop();
	bool BreakPoint(const char *File, int Line, bool Add);
};

/// Scripting engine system functions
class SystemFunctions : public GScriptContext
{
	GScriptEngine *Engine;
	GStream *Log;
	#ifdef WINNATIVE
	HANDLE Brk;
	#endif

	GView *CastGView(GVariant &v);

public:
	SystemFunctions();
	~SystemFunctions();

	GStream *GetLog();
	bool SetLog(GStream *log);
	void SetEngine(GScriptEngine *Eng);	
	
	char *GetIncludeFile(char *FileName)
	{
		return NULL;
	}
	
	GHostFunc *GetCommands();

	// String
		bool LoadString(GVariant *Ret, ArgumentArray &Args);
		/// Formats a string
		bool Sprintf(GVariant *Ret, ArgumentArray &Args);
		/// Formats a file size
		bool FormatSize(GVariant *Ret, ArgumentArray &Args);
		/// Prints items to the console
		bool Print(GVariant *Ret, ArgumentArray &Args);

	// Object creation/deletion
		bool New(GVariant *Ret, ArgumentArray &Args);
		bool Delete(GVariant *Ret, ArgumentArray &Args);

	// File
		/// Reads a text file into a variable
		bool ReadTextFile(GVariant *Ret, ArgumentArray &Args);
		/// Writes a text file from a variable
		bool WriteTextFile(GVariant *Ret, ArgumentArray &Args);
		/// \brief Opens a file open dialog to select files.
		///
		/// Args: GView *Parent, char *Patterns, 
		///		  char *InitFolder, bool Multiselect
		bool SelectFiles(GVariant *Ret, ArgumentArray &Args);
		/// Lists file in folder
		///
		/// Args; char *Path, [optional] char *Pattern
		/// Returns: List of DOM objects with the following fields:
		///		Name - File/dir name
		///		Size - Size of entry
		///		Folder - bool, true if folder
		///		Modified - GDateTime, modified time
		bool ListFiles(GVariant *Ret, ArgumentArray &Args);
		/// Deletes a file
		bool DeleteFile(GVariant *Ret, ArgumentArray &Args);
		/// Gets the current script path.
		bool CurrentScript(GVariant *Ret, ArgumentArray &Args);
		/// Finds out if a path exists.
		bool PathExists(GVariant *Ret, ArgumentArray &Args);
		/// Joins path segments together.
		bool PathJoin(GVariant *Ret, ArgumentArray &Args);
		/// Returns the current OS path separator.
		bool PathSep(GVariant *Ret, ArgumentArray &Args);

	// Time
		/// Sleeps a number of milliseconds
		bool Sleep(GVariant *Ret, ArgumentArray &Args);
		/// Get the current tick count
		bool ClockTick(GVariant *Ret, ArgumentArray &Args);
		/// Get the date time
		bool Now(GVariant *Ret, ArgumentArray &Args);

	// Bitmaps
		/// Creates a memory context
		bool CreateSurface(GVariant *Ret, ArgumentArray &Args);

	// User interface
		/// Gets an input string from the user
		/// String GetInputDlg(Window Parent, String InitialValue, String Question, String Title[, bool IsPassword]);
		bool GetInputDlg(GVariant *Ret, ArgumentArray &Args);
		/// Gets a view by id
		bool GetViewById(GVariant *Ret, ArgumentArray &Args);

	// System
		/// Executes a command, waits for it to finish, then returns it's output:
		/// String Execute(String Application, String CmdLine);
		bool Execute(GVariant *Ret, ArgumentArray &Args);
		/// Executes a command and doesn't wait for it to return:
		/// Bool System(String Application, String CmdLine);
		bool System(GVariant *Ret, ArgumentArray &Args);
		/// Gets the operating system name.
		bool OsName(GVariant *Ret, ArgumentArray &Args);
		/// Gets the operating system version.
		bool OsVersion(GVariant *Ret, ArgumentArray &Args);
};

#endif
