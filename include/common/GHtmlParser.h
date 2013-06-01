#ifndef _GHTMLPARSER_H_
#define _GHTMLPARSER_H_

class GHtmlParser
{
protected:
	GDocView *View;
	List<GHtmlElement> OpenTags;
	GAutoString DocCharSet;
	bool DocAndCsTheSame;

	void CloseTag(GHtmlElement *t)
	{
		if (!t)
			return;

		OpenTags.Delete(t);
	}

	char *ParsePropValue(char *s, char *&Value);
	char *ParseName(char *s, GAutoString &Name);
	char *ParseName(char *s, char **Name);
	char *ParsePropList(char *s, GHtmlElement *Obj, bool &Closed);
	void SkipNonDisplay(char *&s);
	char *NextTag(char *s);
	char16 *CleanText(const char *s, int Len, bool ConversionAllowed, bool KeepWhiteSpace);
	GHtmlElement *GetOpenTag(const char *Tag);
	void _TraceOpenTags();

public:
	GHtmlParser(GDocView *view)
	{
		View = view;
	}

	char *ParseHtml(GHtmlElement *Elem, char *Doc, int Depth, bool InPreTag = false, bool *BackOut = NULL);	
	GHtmlElemInfo *GetTagInfo(const char *Tag);
	
	// Tool methods
	static bool ParseColour(const char *s, GCss::ColorDef &c);
	static bool Is8Bit(char *s);
	
	// Virtual callbacks
	virtual GHtmlElement *CreateElement(GHtmlElement *Parent) = 0;
};

#endif // _GHTMLPARSER_H_