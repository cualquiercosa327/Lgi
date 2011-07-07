/// \file
/// \author Matthew Allen <fret@memecode.com>
#ifndef _GXMLTREE_H_
#define _GXMLTREE_H_

#include "GHashTable.h"
#include "GRefCount.h"
#include "GDom.h"

/// Runtime option: Don't encode entities
#define GXT_NO_ENTITIES						0x0001
/// Runtime option: Output minimal whitespace
#define GXT_NO_PRETTY_WHITESPACE			0x0002
/// Runtime option: Output extra whitespace
#define GXT_PRETTY_WHITESPACE				0x0004
/// Runtime option: Keep input whitespace
#define GXT_KEEP_WHITESPACE					0x0008
/// Runtime option: Don't create DOM heirarchy, just a list of tags
#define GXT_NO_DOM							0x0010

class GXmlTree;
class GXmlTreePrivate;

class LgiClass GXmlAlloc : public GRefCount
{
public:
	virtual ~GXmlAlloc() {}

	virtual void *Alloc(size_t Size) = 0;
	virtual void Free(void *Ptr) = 0;

	char *Alloc(const char *s, int len = -1)
	{
		if (!s) return 0;
		if (len < 0) len = (int)strlen(s);
		char *p = (char*) Alloc(len+1);
		if (!p) return 0;
		memcpy(p, s, len);
		p[len] = 0;
		return p;
	}
};

/// Xml attribute, a named value.
class LgiClass GXmlAttr
{
	friend class GXmlTag;
	friend class GXmlTree;

	/// The name of the attribute
	char *Name;
	/// The value of the attribute
	char *Value;

public:
	GXmlAttr()
	{
		Name = 0;
		Value = 0;
	}
	
	char *GetName() { return Name; }
	char *GetValue() { return Value; }
};

/// An XML element or tag. Contains optionally sub tags and a list of attributes.
/// C++ applications can inherit from this and have native member types mapped to
/// attributes using the SerializeAttr methods. All you have to do is override the
/// virtual member function Serialize and call SerializeAttr on each of your native
/// member variables. When loading and saving the attributes will be mapped to and
/// from your native types.
class LgiClass GXmlTag : virtual public GDom
{
	friend class GXmlTree;

	void ParseAttribute(GXmlTree *Tree, GXmlAlloc *Alloc, char *&t, bool &NoChildren, bool &TypeDef);

protected:
	GAutoRefPtr<GXmlAlloc> Allocator;

	bool Write;
	GXmlAttr *_Attr(const char *Name, bool Write);
	bool GetVariant(const char *Name, GVariant &Value, char *Array);
	bool SetVariant(const char *Name, GVariant &Value, char *Array);

public:
	/// The name of the tag/element. This can be NULL in the case
	/// that the element is purely content.
	char *Tag;
	/// Any content following the tag.
	char *Content;
	/// The parent element/tag.
	GXmlTag *Parent;
	/// A list of attributes that this tag has
	GArray<GXmlAttr> Attr;
	/// A list of child tags. Don't edit this list yourself, use the
	/// InsertTag and RemoveTag methods.
	List<GXmlTag> Children;
	
	/// Construct the object
	GXmlTag
	(
		/// [Optional] Start with this name
		const char *tag = 0,
		/// [Optional] Use this allocator
		GXmlAlloc *alloc = 0
	);	
	/// Construct the object
	GXmlTag(const GXmlTag &t);	
	virtual ~GXmlTag();

	/// For debugging.
	bool Dump(int Depth = 0);
	/// Free any memory owned by this object
	void Empty(bool Deep);
	/// Free all attributes
	void EmptyAttributes();
	/// Frees all child tags
	void EmptyChildren();

	/// \return true if the tag is 's'
	bool IsTag(const char *s) { return Tag && s ? _stricmp(Tag, s) == 0 : false; }

	/// Get the string value of a named attribute
	char *GetAttr(const char *Name);
	/// Get the value of a named attribute as an int
	int GetAsInt(const char *Name);
	/// Set the value of a named attribute to a string
	bool SetAttr(const char *Name, const char *Value);
	/// Set the value of a named attribute to an int
	bool SetAttr(const char *Name, int Value);
	/// Deletes an attribute
	bool DelAttr(const char *Name);

	/// Read/write a native C integer into an attribute
	bool SerializeAttr(const char *Attr, int &Int);	
	/// Read/write a native C dynamically allocated string into an attribute
	bool SerializeAttr(const char *Attr, char *&Str);
	/// Read/write a native C double into an attribute
	bool SerializeAttr(const char *Attr, double &Dbl);
		
	/// Read/write all your native types in here
	virtual bool Serialize() { return false; }
	/// Returns a pointer to a child tag if present, or NULL if not.
	GXmlTag *GetTag(const char *Name, bool Create = false);
	/// Creates a sub tag if it doesn't already exist.
	GXmlTag *CreateTag(const char *Name, char *Content = 0);
	/// Inserts a child tag.
	virtual void InsertTag(GXmlTag *t);
	/// Removes this tag from the DOM heirarchy.
	virtual void RemoveTag();

	/// Copy operator, doesn't effect children.
	GXmlTag &operator =(GXmlTag &t);
	/// Copy method, deep option copies all child elements as well.
	bool Copy(GXmlTag &t, bool Deep = false);
};

/// In the case your inheriting objects from GXmlTag you need to instantiate your
/// types as the XML is loaded and elements are encountered. Pass this class to the
/// GXmlTree::Read to get create events during the read process, create an object of
/// the right type and pass it back.
class GXmlFactory
{
public:
	/// Creates an object of type 'Tag' and returns it.
	virtual GXmlTag *Create(char *Tag) = 0;
};

/// The reader/writer for a tree of GXmlTag objects.
class LgiClass GXmlTree
{
	friend class GXmlTag;
	GXmlTreePrivate *d;

protected:
	GXmlTag *Parse(GXmlTag *Tag, GXmlAlloc *Alloc, char *&t, bool &NoChildren, bool InTypeDef);
	void Output(GXmlTag *t, int Depth);

public:
	/// Constructor
	GXmlTree
	(
		/// \sa #GXT_NO_ENTITIES, #GXT_NO_PRETTY_WHITESPACE, #GXT_PRETTY_WHITESPACE, #GXT_KEEP_WHITESPACE and #GXT_NO_DOM
		int Flags = 0
	);
	~GXmlTree();
	
	/// Read an XML file into a DOM tree of GXmlTag objects from a stream.
	bool Read
	(
		/// The root tag to create children from.
		GXmlTag *Root,
		/// The stream to read from.
		GStreamI *File,
		/// [Optional] The factory to create GXmlTag type objects. If not specified
		/// vanilla GXmlTag objects will be created.
		GXmlFactory *Factory = 0
	);
	/// Write an XML file from a DOM tree of GXmlTag objects into a stream.
	bool Write
	(
		/// The DOM tree.
		GXmlTag *Root,
		/// The output stream.
		GStreamI *File
	);
	/// Gets the last error message.
	char *GetErrorMsg();
	/// A hash of tags that can't have children.
	GHashTable *NoChildTags();
	/// Gets the associated style file
	char *GetStyleFile(char **StyleType = 0);
	/// Sets the associated css file
	void SetStyleFile(char *stylefile, char *styletype = "text/css");

	/// Add entities
	GHashTbl<const char*,char16> *GetEntityTable();
	/// Decode a string with entities
	char *DecodeEntities(char *s, int len = -1);
	/// Encode a string to use entities
	char *EncodeEntities(char *s, int len = -1, const char *extra_characters = 0);
	/// Encode a string to use entities
	bool EncodeEntities(GStreamI *out, char *s, int len, const char *extra_characters = 0);
};

#endif
