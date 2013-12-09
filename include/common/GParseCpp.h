#ifndef _GPARSECPP_H_
#define _GPARSECPP_H_

/// C++ symbol types
enum GCppSymType
{
	SymNone,
	SymFunction,
	SymVariable,
	SymMacro,
};

/// A reference to a matched symbol
struct GCppSymbolResult
{
	/// The type of the symbol
	GCppSymType Type;
	/// Which file the symbol is in
	const char *File;
	/// The line number in that file
	int Line;
	/// The quality of the match (higher is better).
	int MatchQuality;
};

/// A parser library for C++ code.
class GCppParser
{
	struct GCppParserPriv *d;

public:
	typedef void (*SearchResultsCb)(void *CallbackData, GArray<GCppSymbolResult*> &Syms);
	
	struct ValuePair
	{
		GAutoString Var, Val;
		
		ValuePair(const char *var = NULL, const char *val = NULL)
		{
			Var.Reset(NewStr(var));
			Val.Reset(NewStr(val));
		}
		
		ValuePair(const ValuePair &p)
		{
			Var.Reset(NewStr(p.Var));
			Val.Reset(NewStr(p.Val));
		}
	};
	
	GCppParser();
	virtual ~GCppParser();
	
	/// Returns true if the worker thread is actively doing something.
	bool IsActive();

	/// Returns true if the worker thread is available to do searches.
	bool IsReady();
	
	/// Parse a group of source code files. This will be done in a worker thread and you 
	/// won't get an immediate result.
	void ParseCode
	(
		/// All the include paths for this group of source code files
		GArray<const char*> &IncludePaths,
		/// Any pre-defined values
		GArray<ValuePair*> &PreDefines,
		/// All the source code files to parse
		GArray<const char*> &Source
	);
	
	/// Do a search for a symbol
	void Search
	(
		/// The search string, can be multiple parts broken by spaces. Sub-strings will
		/// match longer idenifiers and searches are case sensitive
		const char *Str,
		/// A callback to receive results. This method will be called from the worker thread,
		/// if you need to display the result in a GUI you will have to do synchronization on
		/// the receiving end.
		SearchResultsCb Callback,
		/// A user defined value to pass back to the Callback.
		void *CallbackData
	);
};

#endif