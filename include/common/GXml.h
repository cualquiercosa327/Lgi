/// \file
/// \author Matthew Allen
#ifndef __XML_H
#define __XML_H

#include "GFile.h"
#include "GContainers.h"

//////////////////////////////////////////////////////////////////////////////////////
class Xml;

/// Xml attribute
class XmlValue
{
public:
	char *Name;
	char *Value;
	int OwnName:1;
	int OwnValue:1;

	XmlValue(Xml *x);
	XmlValue(Xml *x, char *&s);
	~XmlValue();
};

/// Xml element container
class XmlTag
{
	Xml *X;

public:
	/// The name of the element
	char *Name;

	/// The contained attributes
	List<XmlValue> Values;

	XmlTag();
	XmlTag(Xml *x, char *&s);
	~XmlTag();

	/// Get an attribute
	bool Get(char *Name, char *&Value);
	/// Set an attribute
	bool Set(char *Name, char *Value);
};

/// Xml document class
class Xml
{
	friend class XmlTag;
	friend class XmlValue;

protected:
	int Length;
	char *Data;
	char *End;
	char *GetStr(char *Start, int Len);

	bool FileToData(GFile &f);
	virtual bool ProcessTag(GStringPipe &p, XmlTag *Tag) { return false; }

public:
	/// All the tags
	List<XmlTag> Values;
	int CursorX, CursorY;

	Xml();
	virtual ~Xml();

	void Empty();

	/// Parse a XML file
	int ParseXmlFile(char *FileName, int Flags = 0);
	/// Parse a XML stream
	int ParseXmlFile(GFile &File, int Flags = 0);
	/// Parse a XML from memory
	int ParseXml(char *Data, int Flags = 0);
	char *NewStrFromXmlFile(GFile &File);
	char *NewStrFromXmlFile(char *FileName);
	virtual bool ExpandXml(GStringPipe &p, char *s);
};

#endif

