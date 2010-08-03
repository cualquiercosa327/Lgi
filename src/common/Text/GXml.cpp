/*
**	FILE:			Xml.cpp
**	AUTHOR:			Matthew Allen
**	DATE:			26/6/99
**	DESCRIPTION:	Scribe Xml methods
**
**	Copyright (C) 1999, Matthew Allen
**		fret@memecode.com
*/

#include <stdio.h>
#include <time.h>
#include <stdarg.h>

#include "GFile.h"
#include "GXml.h"
#include "Gdc2.h"
#include "GString.h"
#include "GUtf8.h"
#include "GUtf8.h"

//////////////////////////////////////////////////////////////////////////////////
XmlValue::XmlValue(Xml *x)
{
	Name = Value = 0;
	OwnName = 0;
	OwnValue = 0;
}

XmlValue::XmlValue(Xml *x, char *&s)
{
	// Init
	Name = Value = 0;
	OwnName = 0;
	OwnValue = 0;

	// skip whitespace
	while (strchr(" \r\t\n", *s)) s++;
	
	if (!strchr("<>/", *s))
	{
		// Find the equals
		char *Start = s;
		while (*s AND *s != '=')
		{
			s++;
		}
		if (*s == '=')
		{
			// Got the name
			Name = x->GetStr(Start, (int)s-(int)Start);
			s++;
			
			bool Quoted = IsQuote(*s);
			char Quote = *s;
			if (Quoted)
			{
				s++;
				Start = s;
				while (	*s AND
						*s != Quote)
				{
					s++;
				}
			}
			else
			{
				Start = s;
				while (	(*s) AND
						(!strchr(" \r\n\t", *s)) AND
						(*s != '>'))
				{
					s++;
				}
			}

			// Got the value
			Value = x->GetStr(Start, (int)s-(int)Start);
			if (Quoted) s++;
		}
	}
}

XmlValue::~XmlValue()
{
	if (OwnName)
	{
		DeleteArray(Name);
	}

	if (OwnValue)
	{
		DeleteArray(Value);
	}
}

////////////////////////////////////////////////////////////////////////////////////////
XmlTag::XmlTag()
{
	X = 0;
}

XmlTag::XmlTag(Xml *x, char *&s)
{
	char *Whitespace = " \r\n\t";
	Name = 0;
	X = x;

	// skip leading characters
	for (; *s AND *s != '<'; s++);

	if (*s == '<')
	{
		// read name
		char *Start = ++s;
		for (;	(*s) AND
				(*s != '>') AND
				(!strchr(Whitespace, *s)); s++)
		{
			if (*s == '\"' OR *s == '\'')
			{
				char d = *s++;
				while (*s AND *s != d)
				{
					s++;
				}
			}
		}

		Name = x->GetStr(Start, (int)s-(int)Start);

		while (*s AND strchr(Whitespace, *s)) s++;

		// read any values
		while (*s AND *s != '>')
		{
			char *b = s;
			XmlValue *Val = new XmlValue(x, s);
			if (Val)
			{
				if (Val->Value)
				{
					Values.Insert(Val);
				}
				else
				{
					DeleteObj(Val);
				}
			}

			while (s AND *s AND strchr(Whitespace, *s)) s++;
			if (*s == '/') s++;
			if (s == b OR *s == '>')
			{
				s++;
				break;
			}
		}
	}
}

XmlTag::~XmlTag()
{
	Values.DeleteObjects();
}

bool XmlTag::Get(char *Attr, char *&Value)
{
	if (Attr)
	{
		for (XmlValue *v = Values.First(); v; v = Values.Next())
		{
			if (stricmp(Attr, v->Name) == 0)
			{
				Value = v->Value;
				return true;
			}
		}
	}

	return false;
}

bool XmlTag::Set(char *Attr, char *Value)
{
	if (Attr)
	{
		for (XmlValue *v = Values.First(); v; v = Values.Next())
		{
			if (stricmp(Attr, v->Name) == 0)
			{
				// value already exists
				if (v->OwnValue)
				{
					DeleteArray(v->Value);
				}

				v->Value = NewStr(Value);
				v->OwnValue = true;
				return true;
			}
		}

		// ok doesn't exist, create it
		XmlValue *Val = new XmlValue(X);
		if (Val)
		{
			Val->Name = NewStr(Attr);
			Val->OwnName = true;

			Val->Value = NewStr(Value);
			Val->OwnValue = true;

			Values.Insert(Val);
		}
	}

	return false;
}

////////////////////////////////////////////////////////////////////////////////////////
Xml::Xml()
{
	Data = 0;
	End = 0;
	Length = 0;
}

Xml::~Xml()
{
	Empty();
}

char *Xml::GetStr(char *Start, int Len)
{
	char *Status = 0;

	if (Data AND Start >= Data AND Start < Data + Length)
	{
		if (!End) End = Data;

		LgiAssert(End < Start + Len);

		Status = End;
		memmove(End, Start, Len);
		End += Len;
		*End++ = 0;
	}

	return Status;
}

bool Xml::ExpandXml(GStringPipe &p, char *s)
{
	bool Status = false;
	char WhiteSpace[] = " \t\r\n";

	if (!Data)
	{
		Data = NewStr(s);
	}
	if (Data)
	{
		s = Data;
		int Len = Length = strlen(Data);

		Status = true;

		char16 Ch;
		while (*s)
		{
			// seek to next tag
			char *Start = s;
			while (Ch = LgiUtf8To32((uint8*&)s, Len))
			{
				if (Ch == 0xfeff) Start += 3;
				if (Ch == '<')
				{
					s--;
					Len++;
					break;
				}
			}
			int t = (int) s - (int) Start;
			if (t > 0)
			{
				p.Push(Start, t);
			}
			else if (!Ch)
			{
				break;
			}

			if (*s == '<')
			{
				bool TagOk = false;
				Start = s;

				XmlTag *Tag = new XmlTag(this, s);
				if (Tag AND Tag->Name)
				{
					// Add to list of tags
					Values.Insert(Tag);

					// process tag
					TagOk = ProcessTag(p, Tag);
					Status |= TagOk;
				}
				else
				{
					DeleteObj(Tag);
				}

				// skip end delimiter
				if (*s == '>') s++;
				int TagLen = s - Start;
				Len -= TagLen;

				if (!TagOk)
				{
					// emit unrecognised tag as plain text
					// p.Push(Start);
				}
			}
		}
	}

	return Status;
}

int Xml::ParseXmlFile(char *FileName, int Flags)
{
	GFile f;
	if (f.Open(FileName, O_READ))
	{
		return ParseXmlFile(f);
	}

	return 0;
}

void Xml::Empty()
{
	Values.DeleteObjects();
	DeleteArray(Data);
}

bool Xml::FileToData(GFile &File)
{
	int Status = 0;

	Empty();

	Length = File.GetSize();
	Data = new char[Length+1];
	End = 0;
	if (Data)
	{
		int r = File.Read(Data, Length);
		if (r > 0)
		{
			Data[max(r, 0)] = 0;
			Status = true;
		}
	}

	return Status;
}

int Xml::ParseXmlFile(GFile &File, int Flags)
{
	if (FileToData(File))
	{
		return ParseXml(Data);
	}
	return 0;
}

int Xml::ParseXml(char *s, int Flags)
{
	if (s)
	{
		if (!Data)
		{
			DeleteArray(Data);
			Data = NewStr(s);
		}

		if (Data)
		{
			Length = strlen(Data);

			for (char *p = Data; *p; )
			{
				for (; *p AND *p != '<'; p++);
				if (*p == '<')
				{
					XmlTag *Tag = new XmlTag(this, p);
					if (Tag AND Tag->Name)
					{
						Values.Insert(Tag);
					}
					else
					{
						DeleteObj(Tag);
					}
				}
			}
		}
	}

	return Values.Length();
}

char *Xml::NewStrFromXmlFile(char *FileName)
{
	GFile f;
	if (f.Open(FileName, O_READ))
	{
		return NewStrFromXmlFile(f);
	}

	return NULL;
}

char *Xml::NewStrFromXmlFile(GFile &File)
{
	GStringPipe Str;
	if (FileToData(File))
	{
		ExpandXml(Str, Data);
	}

	return Str.NewStr();
}
