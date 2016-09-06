#ifndef _FIND_SYMBOL_H_
#define _FIND_SYMBOL_H_

struct FindSymResult
{
	GString Symbol, File;
	int Line;
	
	FindSymResult(const FindSymResult &c)
	{
		*this = c;
	}
	
	FindSymResult(const char *f = NULL, int line = 0)
	{
		File = f;
		Line = line;
	}
	
	FindSymResult &operator =(const FindSymResult &c)
	{
		Symbol = c.Symbol;
		File = c.File;
		Line = c.Line;
		return *this;
	}
};

struct FindSymRequest
{
	GEventSinkI *Sink;
	GString Str;
	GArray<FindSymResult*> Results;
	
	FindSymRequest(GEventSinkI *sink)
	{
		Sink = sink;
	}
	
	~FindSymRequest()
	{
		Results.DeleteObjects();
	}
};

class FindSymbolSystem
{
	struct FindSymbolSystemPriv *d;

public:
	enum SymAction
	{
		FileAdd,
		FileRemove,
		FileReparse
	};

	FindSymbolSystem(GEventSinkI *app);
	~FindSymbolSystem();
	
	bool SetIncludePaths(GString::Array &Paths);
	bool OnFile(const char *Path, SymAction Action);
	FindSymResult OpenSearchDlg(GViewI *Parent);
	
	/// This function searches the database for symbols and returns
	/// the results as a M_FIND_SYM_RESULT message.
	void Search(GEventSinkI *Results, const char *SearchStr);
};

#endif