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
/// Don't print <?xml ... ?> header
#define GXT_NO_HEADER						0x0020

class GXmlTree;
class GXmlTreePrivate;

class LgiClass GXmlAlloc : public GRefCount
{
public:
	virtual ~GXmlAlloc() {}

	virtual void *Alloc(size_t Size) = 0;
	virtual void Free(void *Ptr) = 0;
	
	// Helper allocator for strings
	char *Alloc(const char *s, ssize_t len = -1);
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
	/// This allocator is responsible for all the string memory used by the Attributes and Tag
	GAutoRefPtr<GXmlAlloc> Allocator;

	bool Write;
	GXmlAttr *_Attr(const char *Name, bool Write);
	bool GetVariant(const char *Name, GVariant &Value, char *Array);
	bool SetVariant(const char *Name, GVariant &Value, char *Array);

	/// The name of the tag/element. This can be NULL in the case
	/// that the element is purely content. The memory is managed by 
	/// 'Allocator' not the general heap. Do not "NewStr" something
	/// and assign it to 'Tag'.
	char *Tag;

	/// Any content following the tag. Memory is owned by the allocator.
	char *Content;

public:
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

	/// Get the content
	char *GetContent() { return Content; };
	/// Convert the content to an integer
	int GetContentAsInt(int Default = 0);
	/// Set the content
	bool SetContent(const char *s, int len = -1);

	/// Get the string value of a named attribute
	char *GetAttr(const char *Name);
	/// Get the value of a named attribute as an int
	int GetAsInt(const char *Name);
	/// Set the value of a named attribute to a string
	bool SetAttr(const char *Name, const char *Value);
	/// Set the value of a named attribute to an int
	bool SetAttr(const char *Name, int Value);
	/// Set the value of a named attribute to an int64
	bool SetAttr(const char *Name, int64 Value);
	/// Deletes an attribute
	bool DelAttr(const char *Name);

	/// Replaces the content with an integer
	bool SetContent(int i);

	/// Read/write a native C integer into an attribute
	bool SerializeAttr(const char *Attr, int &Int);	
	/// Read/write a native C dynamically allocated string into an attribute
	bool SerializeAttr(const char *Attr, char *&Str);
	/// Read/write a native C double into an attribute
	bool SerializeAttr(const char *Attr, double &Dbl);
	
	/// Gets the current tag
	const char *GetTag();
	/// Sets or removes the tag using the allocator
	void SetTag(const char *Str = NULL);

	/// Read/write all your native types in here
	virtual bool Serialize(bool Write) { return false; }
	/// Returns a pointer to a child tag if present, or NULL if not.
	GXmlTag *GetChildTag(const char *Name, bool Create = false, const char *TagSeparator = ".");
	/// Creates a sub tag if it doesn't already exist.
	GXmlTag *CreateTag(const char *Name, char *Content = 0);
	/// Inserts a child tag.
	virtual void InsertTag(GXmlTag *t);
	/// Removes this tag from the DOM hierarchy.
	virtual void RemoveTag();
	/// Counts all this and all child tags
	int64 CountTags();

	/// Copy operator, doesn't effect children.
	GXmlTag &operator =(GXmlTag &t);
	/// Copy method, deep option copies all child elements as well.
	bool Copy(GXmlTag &t, bool Deep = false);
	
	/// Retrieve elements using XPath notation.
	bool XPath(GArray<GXmlTag*> &Results, const char *Path);
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
	virtual void OnParseComment(GXmlTag *Ref, const char *Comment, ssize_t Bytes) {}

	void Output(GXmlTag *t, int Depth);

public:
	/// Constructor
	GXmlTree
	(
		/// \sa #GXT_NO_ENTITIES, #GXT_NO_PRETTY_WHITESPACE, #GXT_PRETTY_WHITESPACE, #GXT_KEEP_WHITESPACE and #GXT_NO_DOM
		int Flags = 0
	);
	virtual ~GXmlTree();
	
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
		GStreamI *File,
		/// [Optional] Progress reporting.
		Progress *Prog = NULL
	);
	/// Gets the last error message.
	char *GetErrorMsg();
	/// A hash of tags that can't have children.
	GHashTbl<const char*,bool> *NoChildTags();
	/// Gets the associated style file
	char *GetStyleFile(char **StyleType = 0);
	/// Sets the associated css file
	void SetStyleFile(char *stylefile, const char *styletype = "text/css");

	/// Add entities
	GHashTbl<const char*,char16> *GetEntityTable();
	/// Decode a string with entities
	char *DecodeEntities(GXmlAlloc *Alloc, char *s, ssize_t len = -1);
	/// Encode a string to use entities
	char *EncodeEntities(char *s, ssize_t len = -1, const char *extra_characters = 0);
	/// Encode a string to use entities
	bool EncodeEntities(GStreamI *out, char *s, ssize_t len, const char *extra_characters = 0);
};

#endif
