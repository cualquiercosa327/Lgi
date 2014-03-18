#ifndef _GHTMLPARSER_H_
#define _GHTMLPARSER_H_

class GHtmlParser
{
	GStringPipe SourceData;
	const char *CurrentSrc;

protected:
	GDocView *View;
	GAutoString Source;
	List<GHtmlElement> OpenTags;
	GAutoString DocCharSet;
	bool DocAndCsTheSame;

	void CloseTag(GHtmlElement *t)
	{
		if (!t)
			return;

		OpenTags.Delete(t);
	}

	GHtmlElement *GetOpenTag(const char *Tag);
	void _TraceOpenTags();
	char *ParseHtml(GHtmlElement *Elem, char *Doc, int Depth, bool InPreTag = false, bool *BackOut = NULL);	
	char16 *DecodeEntities(const char *s, int len);

public:
	GHtmlParser(GDocView *view)
	{
		View = view;
	}

	// Main entry point
	bool Parse(GHtmlElement *Root, const char *Doc);
	
	// Tool methods
	GHtmlElemInfo *GetTagInfo(const char *Tag);
	static bool ParseColour(const char *s, GCss::ColorDef &c);
	static bool Is8Bit(char *s);
	char *ParsePropValue(char *s, char16 *&Value);
	char *ParseName(char *s, GAutoString &Name);
	char *ParseName(char *s, char **Name);
	char *ParsePropList(char *s, GHtmlElement *Obj, bool &Closed);
	void SkipNonDisplay(char *&s);
	char *NextTag(char *s);
	char16 *CleanText(const char *s, int Len, bool ConversionAllowed, bool KeepWhiteSpace);
	
	// Virtual callbacks
	virtual GHtmlElement *CreateElement(GHtmlElement *Parent) = 0;
};

#endif // _GHTMLPARSER_H_