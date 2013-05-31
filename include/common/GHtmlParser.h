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
	GHtmlElement *GetOpenTag(const char *Tag);
	char *NextTag(char *s);
	char16 *CleanText(const char *s, int Len, bool ConversionAllowed, bool KeepWhiteSpace);

public:
	GHtmlParser(GDocView *view)
	{
		View = view;
	}

	char *ParseHtml(GHtmlElement *Elem, char *Doc, int Depth, bool InPreTag, bool *BackOut = NULL);	
	GHtmlElemInfo *GetTagInfo(const char *Tag);
};

#endif // _GHTMLPARSER_H_