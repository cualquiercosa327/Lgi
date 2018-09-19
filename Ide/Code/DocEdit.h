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

class DocEdit;
class DocEditStyling : public LThread, public LMutex
{
public:
	enum DocType
	{
		CodeHtml,
		CodePhp,
		CodeCss,
		CodeComment,
		CodePre,
	};

protected:
	SourceType FileType;
	DocEdit *View;

	enum WordType
	{
		KNone,
		KLang,
		KType
	};

	enum ThreadState
	{
		KWaiting,
		KStyling,
		KCancel,
		KExiting,
	};

	struct Node
	{
		const int Captials = 26;
		const int Numbers = Captials + 26;
		const int Symbols = Numbers + 10;
		Node *Next[26 + 26 + 10 + 1];
		WordType Type;

		int Map(char16 c)
		{
			if (c >= 'a' && c <= 'z')
				return c - 'a';
			if (c >= 'A' && c <= 'Z')
				return c - 'A' + Captials;
			if (IsDigit(c))
				return c - '0' + Numbers;
			if (c == '_')
				return Symbols;
			// LgiAssert(0);
			return -1;
		}

		Node()
		{
			ZeroObj(Next);
			Type = KNone;
		}

		~Node()
		{
			for (int i=0; i<CountOf(Next); i++)
				DeleteObj(Next[i]);
		}
	};

	struct StylingParams
	{
		GTextView3 *View;
		size_t PourStart;
		ssize_t PourSize;
		GArray<char16> Text;
		GString FileName;
		LUnrolledList<GTextView3::GStyle> Styles;
		GTextView3::GStyle Visible;
		GTextView3::GStyle Dirty;

		StylingParams(GTextView3 *view) :
			Dirty(STYLE_NONE)
		{
			View = view;
		}

		void StyleString(char16 *&s, char16 *e, GColour c, LUnrolledList<GTextView3::GStyle> *Out = NULL)
		{
			if (!Out)
				Out = &Styles;
			auto &st = Out->New().Construct(View, STYLE_IDE);
			auto pText = Text.AddressOf();
			st.Start = s - pText;
			st.Font = View->GetFont();

			char16 Delim = *s++;
			while (s < e && *s != Delim)
			{
				if (*s == '\\')
					s++;
				s++;
			}

			st.Len = (s - pText) - st.Start + 1;
			st.Fore = c;
		}
	};

	// Styling data...
	LThreadEvent Event;
	ThreadState ParentState, WorkerState;
	// Lock before access:
		StylingParams Params;
	// EndLock
	// Thread only
		Node Root;
		LUnrolledList<GTextView3::GStyle> PrevStyle;
	// End Thread only

	// Styling functions..
	int Main();
	void StyleCpp(StylingParams &p);
	void StylePython(StylingParams &p);
	void StyleDefault(StylingParams &p);
	void StyleXml(StylingParams &p);
	void StyleHtml(StylingParams &p);
	void AddKeywords(const char **keys, bool IsType);

	// Full refresh triggers
	int RefreshSize;
	const char **RefreshEdges;

public:
	DocEditStyling(DocEdit *view);
	GColour ColourFromType(DocType t);
};

class DocEdit :
	public GTextView3,
	public GDocumentEnv,
	public DocEditStyling
{
	IdeDoc *Doc;
	int CurLine;
	GdcPt2 MsClick;

	void OnApplyStyles();
	int CountRefreshEdges(size_t At, ssize_t Len);

public:
	static int LeftMarginPx;
	DocEdit(IdeDoc *d, GFontType *f);
	~DocEdit();

	char *Name() override { return GTextView3::Name(); }
	bool Name(const char *s) override { return GTextView3::Name(s); }
	bool SetPourEnabled(bool b);
	int GetTopPaddingPx();
	void InvalidateLine(int Idx);
	char *TemplateMerge(const char *Template, char *Name, List<char> *Params);
	bool GetVisible(GStyle &s);
	void OnCreate() override;

	// Overrides
	bool AppendItems(GSubMenu *Menu, int Base) override;
	bool DoGoto() override;
	void OnPaintLeftMargin(GSurface *pDC, GRect &r, GColour &colour) override;
	void OnMouseClick(GMouse &m) override;
	bool OnKey(GKey &k) override;	
	bool OnMenu(GDocView *View, int Id, void *Context) override;
	GMessage::Result OnEvent(GMessage *m) override;
	void SetCaret(size_t i, bool Select, bool ForceFullUpdate = false) override;
	void PourStyle(size_t Start, ssize_t EditSize) override;
	bool Pour(GRegion &r) override;
	bool Insert(size_t At, char16 *Data, ssize_t Len) override;
	bool Delete(size_t At, ssize_t Len) override;
};

#endif
