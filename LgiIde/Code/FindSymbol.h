#ifndef _FIND_SYMBOL_H_
#define _FIND_SYMBOL_H_

#include "GEventTargetThread.h"

struct FindSymResult
{
	GString Symbol, File;
	int Line;
	int Score;
	
	FindSymResult(const FindSymResult &c)
	{
		*this = c;
	}
	
	FindSymResult(const char *f = NULL, int line = 0)
	{
		Score = 0;
		File = f;
		Line = line;
	}
	
	FindSymResult &operator =(const FindSymResult &c)
	{
		Score = c.Score;
		Symbol = c.Symbol;
		File = c.File;
		Line = c.Line;
		return *this;
	}

	int Class()
	{
		if (Score > 0)
			return 0;

		char *e = LgiGetExtension(File);
		if (!e)
			return 3;
		
		if (!_stricmp(e, "h") ||
			!_stricmp(e, "hpp") ||
			!_stricmp(e, "hxx"))
			return 1;
		
		return 2;
	}

	// Sort symbol results by:
	int Compare(FindSymResult *r)
	{
		// Score first...
		if (Score != r->Score)
			return r->Score - Score;

		// Then headers...
		int c1 = Class();
		int c2 = r->Class();
		if (c1 != c2)
			return c1 - c2;

		// Then by file name...
		int c = _stricmp(File, r->File);
		if (c)
			return c;

		// Then by line number...
		return Line - r->Line;
	}
};

struct FindSymRequest
{
	int SinkHnd;
	GString Str;
	GArray<FindSymResult*> Results;
	
	FindSymRequest(int sinkhnd)
	{
		SinkHnd = sinkhnd;
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

	struct SymFileParams
	{
		SymAction Action;
		GString File;
		int Platforms;
	};

	FindSymbolSystem(int AppHnd);
	~FindSymbolSystem();
	
	bool SetIncludePaths(GString::Array &Paths);
	bool OnFile(const char *Path, SymAction Action, int Platforms);
	FindSymResult OpenSearchDlg(GViewI *Parent);
	
	/// This function searches the database for symbols and returns
	/// the results as a M_FIND_SYM_REQUEST message.
	void Search(int ResultsSinkHnd, const char *SearchStr, bool AllPlat);
};

#endif