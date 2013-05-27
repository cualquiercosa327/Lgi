#ifndef _FIND_SYMBOL_H_
#define _FIND_SYMBOL_H_

struct FindSymResult
{
	GAutoString Symbol, File;
	int Line;
	
	FindSymResult(const char *f = NULL, int line = 0)
	{
		File.Reset(NewStr(f));
		Line = line;
	}
	
	FindSymResult &operator =(const FindSymResult &c)
	{
		Symbol.Reset(NewStr(c.Symbol));
		File.Reset(NewStr(c.File));
		Line = c.Line;
		return *this;
	}
};

class FindSymbolSystem
{
	struct FindSymbolSystemPriv *d;

public:
	FindSymbolSystem(class AppWnd *app);
	~FindSymbolSystem();
	
	void OnProject();
	FindSymResult OpenSearchDlg(GViewI *Parent);
	void Search(const char *SearchStr, GArray<FindSymResult> &Results);
};

#endif