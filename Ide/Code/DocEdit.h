#ifndef _DOC_EDIT_H_
#define _DOC_EDIT_H_

#include "GTextView3.h"
#include "IdeDoc.h"

#define IDM_FILE_COMMENT			100
#define IDM_FUNC_COMMENT			101

enum SourceType
{
	SrcUnknown,
	SrcPlainText,
	SrcCpp,
	SrcPython,
	SrcXml,
	SrcHtml,
};

class DocEdit : public GTextView3, public GDocumentEnv
{
	IdeDoc *Doc;
	int CurLine;
	GdcPt2 MsClick;
	SourceType FileType;

	struct Keyword
	{
		char16 *Word;
		int Len;
		bool IsType;
		Keyword *Next;

		Keyword(const char *w, bool istype = false)
		{
			Word = Utf8ToWide(w);
			Len = Strlen(Word);
			IsType = istype;
			Next = NULL;
		}

		~Keyword()
		{
			delete Next;
			delete [] Word;
		}
	};

	Keyword *HasKeyword[26];

	// Full refresh triggers
	int RefreshSize;
	const char **RefreshEdges;

	int CountRefreshEdges(size_t At, ssize_t Len);

public:
	static int LeftMarginPx;
	enum HtmlType
	{
		CodeHtml,
		CodePhp,
		CodeCss,
		CodeComment,
		CodePre,
	};

	DocEdit(IdeDoc *d, GFontType *f);
	~DocEdit();

	int GetTopPaddingPx();
	void InvalidateLine(int Idx);
	char *TemplateMerge(const char *Template, char *Name, List<char> *Params);
	bool GetVisible(GStyle &s);
	void StyleString(char16 *&s, char16 *e);
	void StyleCpp(ssize_t Start, ssize_t EditSize);
	void StylePython(ssize_t Start, ssize_t EditSize);
	void StyleDefault(ssize_t Start, ssize_t EditSize);
	void StyleXml(ssize_t Start, ssize_t EditSize);
	GColour ColourFromType(HtmlType t);
	void StyleHtml(ssize_t Start, ssize_t EditSize);
	void AddKeywords(const char **keys, bool IsType);

	// Overrides
	bool AppendItems(GSubMenu *Menu, int Base) override;
	bool DoGoto() override;
	void OnPaintLeftMargin(GSurface *pDC, GRect &r, GColour &colour) override;
	void OnMouseClick(GMouse &m) override;
	bool OnKey(GKey &k) override;	
	bool OnMenu(GDocView *View, int Id, void *Context) override;
	void SetCaret(size_t i, bool Select, bool ForceFullUpdate = false) override;
	void PourStyle(size_t Start, ssize_t EditSize) override;
	bool Pour(GRegion &r) override;
	bool Insert(size_t At, char16 *Data, ssize_t Len) override;
	bool Delete(size_t At, ssize_t Len) override;
};

#endif