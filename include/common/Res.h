
#ifndef __RES_H
#define __RES_H

// Includes
#include "GXmlTree.h"

/////////////////////////////////////////////////////////////////////

/// LgiRes file format types
enum ResFileFormat
{
	/// The original format based on different codepages for each translations.
	CodepageFile,
	/// The second format using utf-8 throughout instead.
	Lr8File,
	/// Straight XML.
	XmlFile,
};

/// All languages are defined by a short string using this type.
///
/// A list of all language supported is provide in ::LgiLanguageTable.
/// 
/// \ingroup Resources
typedef const char *GLanguageId;

/// Infomation pertaining to one language.
///
/// A table of these is in ::LgiLanguageTable.
/// 
/// \ingroup Resources
struct GLanguage
{
	/// The full english name of the language
	const char *Name;
	
	/// The short language code, e.g. 'en'
	GLanguageId Id;
	
	/// Any windows language ID mapping
	int Win32Id;
	
	/// The old ID in previous versions
	int OldId;
	
	/// The default charset to use with this language
	const char *Charset;

	/// \return true if this language is 'en'
	bool IsEnglish()
	{
		return	Id[0] == 'e' &&
				Id[1] == 'n' &&
				!Id[2];
	}

	/// \returns true if the language is the same as 'i'
	bool operator ==(GLanguageId i)
	{
		return i && _stricmp(Id, i) == 0;
	}

	/// \returns true if the language is NOT the same as 'i'
	bool operator !=(GLanguageId i)
	{
		return !(*this == i);
	}
};

/// Find a GLanguage object by it's language code.
///
/// This function is very fast, it's not a linear search through the
/// list of all languages. So you can call it often without worrying
/// about speed.
LgiExtern GLanguage *GFindLang
(
	/// [Optional] The language code to search for.
	/// If not provided the start of the table is returned.
	GLanguageId Id,
	/// [Optional] The textual name of the language to search for.
	const char *Name = NULL
);

/// Finds a language by it's OldId.
/// \deprecated Use ::GFindLang in any new code.
LgiExtern GLanguage *GFindOldLang(int OldId);

////////////////////////////////////////////////////////////////////
// All the names of the controls as strings
LgiExtern char Res_Dialog[];
LgiExtern char Res_Table[];
LgiExtern char Res_ControlTree[];
LgiExtern char Res_StaticText[];
LgiExtern char Res_EditBox[];
LgiExtern char Res_CheckBox[];
LgiExtern char Res_Button[];
LgiExtern char Res_Group[];
LgiExtern char Res_RadioBox[];
LgiExtern char Res_Tab[];
LgiExtern char Res_TabView[];
LgiExtern char Res_ListView[];
LgiExtern char Res_Column[];
LgiExtern char Res_TreeView[];
LgiExtern char Res_Bitmap[];
LgiExtern char Res_Progress[];
LgiExtern char Res_Slider[];
LgiExtern char Res_ComboBox[];
LgiExtern char Res_ScrollBar[];
LgiExtern char Res_Custom[];

// Classes
class ResObjectImpl;
class ResFactory;

/// This interface is used by the resource sub-system to check 
/// whether a UI element should be loaded and displayed.
struct LgiClass ResReadCtx
{
	/// Check the tags for matches...
	/// \returns true when the element should be displayed
	virtual bool Check(const char *tags) { return true; }
	/// Check the tags for matches...
	/// \returns true when the element should be displayed
	virtual bool Check(GXmlTag *t) { return true; }
};

class LgiClass ResObject
{
private:
	ResObjectImpl *_ObjImpl;

protected:
	char *_ObjName;
	void SetObjectName(char *on) { _ObjName = on; }

public:
	ResObject(char *Name);
	virtual ~ResObject();

	char *GetObjectName() { return _ObjName; }
	virtual ResObjectImpl *GetObjectImpl(ResFactory *f);
};

// Inherit from and implement to create objects for your system
class LgiClass ResFactory
{
public:
	// Use to read and write the objects
	bool Res_Read(ResObject *Obj, GXmlTag *Tag, ResReadCtx &Ctx);
	bool Res_Write(ResObject *Obj, GXmlTag *Tag);

	// Overide
	virtual char *StringFromRef(int Ref) = 0;
	virtual ResObject *CreateObject(GXmlTag *Tag, ResObject *Parent) = 0;

	// Overide these to operate on the objects
	virtual void		Res_SetPos		(ResObject *Obj, int x1, int y1, int x2, int y2) = 0;
	virtual void		Res_SetPos		(ResObject *Obj, char *s) = 0;
	virtual GRect		Res_GetPos		(ResObject *Obj) = 0;
	virtual int			Res_GetStrRef	(ResObject *Obj) = 0;
	virtual bool		Res_SetStrRef	(ResObject *Obj, int Ref, ResReadCtx *Ctx) = 0;
	virtual void		Res_Attach		(ResObject *Obj, ResObject *Parent) = 0;
	virtual bool		Res_GetChildren	(ResObject *Obj, List<ResObject> *l, bool Deep) = 0;
	virtual void		Res_Append		(ResObject *Obj, ResObject *Parent) = 0;
	virtual bool		Res_GetItems	(ResObject *Obj, List<ResObject> *l) = 0;
	virtual GDom*		Res_GetDom		(ResObject *Obj) = 0;
	
	// Property access
	virtual bool		Res_GetProperties(ResObject *Obj, GDom *Props) = 0;
	virtual bool		Res_SetProperties(ResObject *Obj, GDom *Props) = 0;
};

#endif
