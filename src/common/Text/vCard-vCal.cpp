#include <stdlib.h>
#include <stdio.h>
#include <ctype.h>

#include "Lgi.h"
#include "vCard-vCal.h"
#include "GToken.h"

#define Push(s) Write(s, strlen(s))

#define ClearFields() \
	DeleteArray(Field); \
	Types.Empty(); \
	DeleteArray(Data)


#define IsType(str) (Types.Find(str) != 0)

#if 1
bool IsVar(char *field, char *s)
{
	if (!s)
		return false;

	char *dot = strchr(field, '.');
	if (dot)
		return stricmp(dot + 1, s) == 0;

	return stricmp(field, s) == 0;
}
#else
#define IsVar(field, str) (field != 0 AND stricmp(field, str) == 0)
#endif

char *DeEscape(char *s, bool QuotedPrintable)
{
	if (!s)
		return 0;

	char *i = s;
	char *o = s;
	while (*i)
	{
		if (*i == '\\')
		{
			i++;
			switch (*i)
			{
				case 'n':
				case 'N':
					*o++ = '\n';
					break;
				case ',':
				case ';':
				case ':':
					*o++ = *i;
					break;
				default:
					*o++ = '\\';
					i--;
					break;
			}
			
		}
		else if (QuotedPrintable AND *i == '=' AND i[1] AND i[2])
		{
			i++;
			char h[3] = { i[0], i[1], 0};
			*o++ = htoi(h);
			i++;
		}
		else
		{
			*o++ = *i;
		}

		i++;
	}
	*o = 0;
	return s;
}

/////////////////////////////////////////////////////////////
// General IO class
class VIoPriv
{
public:
	GStringPipe Buf;
};

VIo::VIo() : d(new VIoPriv)
{
}

VIo::~VIo()
{
	DeleteObj(d);
}

/*
bool VIo::IsBeginEnd()
{
	if (Var.Length() == 2)
	{
		if (stricmp(Var[0], "begin") == 0)
		{
			Object.Insert(NewStr(Data));
			return true;
		}
		else if (stricmp(Var[0], "end") == 0)
		{
			char *o=Object.Last();
			if (o AND stricmp(o, Data) == 0)
			{
				Object.Delete(o);
				DeleteArray(o);
			}
			return true;
		}
	}

	return false;
}

bool VIo::IsObject(char *Obj)
{
	if (Obj)
	{
		for (char *o=Object.Last(); o; o=Object.Prev())
		{
			if (stricmp(Obj, o) == 0)
			{
				return true;
			}
		}
	}

	return false;
}

bool VIo::IsTyped()
{
	Type.Empty();
	Value.Empty();
	Variables.Empty();

	char *Semi = Var[0] ? strchr(Var[0], ';') : 0;
	if (Semi)
	{
		GToken V(Var[0], ";", false);
		*Semi++ = 0;

		if (strchr(Semi, '='))
		{
			for (int i=0; i<V.Length(); i++)
			{
				if (strnicmp(V[i], "type=", 5) == 0)
				{
					GToken T(V[i]+5, ",");
					Type.AppendTokens(&T);
				}
				else if (strnicmp(V[i], "value=", 5) == 0)
				{
					GToken T(V[i]+6, ",");
					Value.AppendTokens(&T);
				}
				else
				{
					char *v = V[i];
					char *e = strchr(v, '=');
					if (e)
					{
						*e++ = 0;
						if (strchr("\'\"", *e))
						{
							char d = *e++;
							char *End = strchr(e, d);
							if (End) *End = 0;
						}
						Variables[v] = e;
					}
				}
			}
		}
		else
		{
			Type.Parse(Semi, ";", false);
		}

		return true;
	}

	return false;
}

bool VIo::IsEncoded()
{
	if (IsType("quoted-printable"))
	{
		if (Data)
		{
			char *In = Data;
			char *Out = In;
			while (*In)
			{
				if (*In == '=' AND In[1])
				{
					char Hex[3] = {In[1], In[2], 0};
					*Out++ = htoi(Hex);
					In += 3;
				}
				else
				{
					*Out++ = *In++;
				}
			}
			*Out++ = 0;
		}

		return true;
	}

	return false;
}

bool VIo::IsVar(char *a, char *b)
{
	if (Var[0] AND a)
	{
		if (stricmp(a, Var[0]) == 0)
		{
			if (Data AND b)
			{
				return stricmp(Data, b) == 0;
			}
			else
			{
				return true;
			}
		}
	}

	return false;
}

bool VIo::IsType(char *type)
{
	for (int t=0; t<Type.Length(); t++)
	{
		if (type AND stricmp(Type[t], type) == 0)
		{
			return true;
		}
	}

	return false;
}

bool VIo::IsValue(char *value)
{
	for (int i=0; i<Value.Length(); i++)
	{
		if (value AND stricmp(Value[i], value) == 0)
		{
			return true;
		}
	}

	return false;
}
*/

bool VIo::ParseDate(GDateTime &Out, char *In)
{
	bool Status = false;

	if (In)
	{
		Out.SetTimeZone(0, false);

		GToken v(In, "T");
		if (v.Length() > 0)
		{
			char *d = v[0];
			if (d AND strlen(d) == 8)
			{
				char Year[5] = {d[0], d[1], d[2], d[3], 0};
				char Month[3] = {d[4], d[5], 0};
				char Day[3] = {d[6], d[7], 0};
				Out.Year(atoi(Year));
				Out.Month(atoi(Month));
				Out.Day(atoi(Day));
				Status = true;
			}

			char *t = v[1];
			if (t AND strlen(t) >= 6)
			{
				char Hour[3] = {t[0], t[1], 0};
				char Minute[3] = {t[2], t[3], 0};
				char Second[3] = {t[4], t[5], 0};
				Out.Hours(atoi(Hour));
				Out.Minutes(atoi(Minute));
				Out.Seconds(atoi(Second));
				Status = true;
			}
		}
	}

	return Status;
}

bool VIo::ParseDuration(GDateTime &Out, int &Sign, char *In)
{
	bool Status = false;

	if (In)
	{
		Sign = 1;
		if (*In == '-')
		{
			Sign = -1;
			In++;
		}

		if (toupper(*In++) == 'P' AND
			toupper(*In++) == 'T')
		{
			while (*In)
			{
				int i = atoi(In);
				while (isdigit(*In)) In++;

				switch (toupper(*In++))
				{
					case 'W':
					{
						Out.Day(Out.Day() + (i * 7));
						break;
					}
					case 'D':
					{
						Out.Day(Out.Day() + i);
						break;
					}
					case 'H':
					{
						Out.Hours(Out.Hours() + i);
						break;
					}
					case 'M':
					{
						Out.Minutes(Out.Minutes() + i);
						break;
					}
					case 'S':
					{
						Out.Seconds(Out.Seconds() + i);
						break;
					}
				}
			}

			Status = true;
		}
	}

	return Status;
}

void VIo::Fold(GStreamI &o, char *i, int pre_chars)
{
	int x = pre_chars;
	for (char *s=i; s AND *s;)
	{
		if (x >= 74)
		{
			// wrapping
			o.Write(i, (int)s-(int)i);
			o.Write((char*)"\r\n\t", 3);
			x = 0;
			i = s;
		}
		else if (*s == '=' OR ((((uint8)*s) & 0x80) != 0))
		{
			// quoted printable
			o.Write(i, (int)s-(int)i);
			GStreamPrint(&o, "=%02.2x", (uint8)*s);
			x += 3;
			i = ++s;
		}
		else if (*s == '\n')
		{
			// new line
			o.Write(i, (int)s-(int)i);
			o.Write((char*)"\\n", 2);
			x += 2;
			i = ++s;
		}
		else if (*s == '\r')
		{
			o.Write(i, (int)s-(int)i);
			i = ++s;
		}
		else
		{
			s++;
			x++;
		}
	}
	o.Write(i, strlen(i));
}

char *VIo::Unfold(char *In)
{
	if (In)
	{
		GStringPipe p(256);

		for (char *i=In; i AND *i; i++)
		{
			if (*i == '\n')
			{
				if (i[1] AND strchr(" \t", i[1]))
				{
					i++;
				}
				else
				{
					p.Write(i, 1);
				}
			}
			else
			{
				p.Write(i, 1);
			}
		}

		return p.NewStr();
	}

	return 0;
}

char *VIo::UnMultiLine(char *In)
{
	if (In)
	{
		GStringPipe p;
		char *n;
		for (char *i=In; i AND *i; i=n)
		{
			n = stristr(i, "\\n");
			if (n)
			{
				p.Write(i, (int)n-(int)i);
				p.Push((char*)"\n");
				n += 2;
			}
			else
			{
				p.Push(i);
			}
		}

		return p.NewStr();
	}

	return 0;
}

/////////////////////////////////////////////////////////////
// VCard class
bool VCard::Import(GDataPropI *c, GStreamI *s)
{
	bool Status = false;

	if (!c OR !s)
		return false;

	char *Field = 0;
	TypesList Types;
	char *Data = 0;

	int PrefEmail = -1;
	GArray<char*> Emails;

	while (ReadField(*s, &Field, &Types, &Data))
	{
		if (stricmp(Field, "begin") == 0 AND
			stricmp(Data, "vcard") == 0)
		{
			while (ReadField(*s, &Field, &Types, &Data))
			{
				if (stricmp(Field, "end") == 0 AND
					stricmp(Data, "vcard") == 0)
					goto ExitLoop;

				if (IsVar(Field, "n"))
				{
					if (stristr(Data, "Ollis"))
					{
						int asd=0;
					}

					GToken Name(Data, ";", false);
					char *First = Name[1];
					char *Last = Name[0];
					char *Title = Name[3];

					if (First)
					{
						c->SetStr(FIELD_FIRST_NAME, First);
						Status = true;
					}

					if (Last)
					{
						c->SetStr(FIELD_LAST_NAME, Last);
						Status = true;
					}

					if (Title)
					{
						c->SetStr(FIELD_TITLE, Title);
						Status = true;
					}
				}
				else if (IsVar(Field, "nickname"))
				{
					c->SetStr(FIELD_NICK, Data);
				}
				else if (IsVar(Field, "tel"))
				{
					GToken Phone(Data, ";", false);
					for (int p=0; p<Phone.Length(); p++)
					{
						if (IsType("cell"))
						{
							if (IsType("Work"))
							{
								c->SetStr(FIELD_WORK_MOBILE, Phone[p]);
							}
							else
							{
								c->SetStr(FIELD_HOME_MOBILE, Phone[p]);
							}
						}
						else if (IsType("fax"))
						{
							if (IsType("Work"))
							{
								c->SetStr(FIELD_WORK_FAX, Phone[p]);
							}
							else
							{
								c->SetStr(FIELD_HOME_FAX, Phone[p]);
							}
						}
						else
						{
							if (IsType("Work"))
							{
								c->SetStr(FIELD_WORK_PHONE, Phone[p]);
							}
							else
							{
								c->SetStr(FIELD_HOME_PHONE, Phone[p]);
							}
						}
					}
				}
				else if (IsVar(Field, "email"))
				{
					if (IsType("pref"))
					{
						PrefEmail = Emails.Length();
					}
					Emails.Add(NewStr(Data));
				}
				else if (IsVar(Field, "org"))
				{
					GToken Org(Data, ";", false);
					if (Org[0]) c->SetStr(FIELD_COMPANY, Org[0]);
				}
				else if (IsVar(Field, "adr"))
				{
					bool IsWork = IsType("work");
					bool IsHome = IsType("home");
					GToken Addr(Data, ";", false);
					if (Addr[2])
					{
						GToken A(Addr[2], "\r\n");
						if (A.Length() > 1)
						{
							c->SetStr(IsWork ? FIELD_WORK_STREET : FIELD_HOME_STREET, A[0]);
							if (A[1]) c->SetStr(IsWork ? FIELD_WORK_SUBURB : FIELD_HOME_SUBURB, A[1]);
							if (A[2]) c->SetStr(IsWork ? FIELD_WORK_COUNTRY : FIELD_HOME_COUNTRY, A[2]);
						}
						else
						{
							c->SetStr(IsWork ? FIELD_WORK_STREET : FIELD_HOME_STREET, Addr[2]);
						}
					}

					if (Addr[3]) c->SetStr(IsWork ? FIELD_WORK_SUBURB : FIELD_HOME_SUBURB, Addr[3]);
					if (Addr[4]) c->SetStr(IsWork ? FIELD_WORK_STATE : FIELD_HOME_STATE, Addr[4]);
					if (Addr[5]) c->SetStr(IsWork ? FIELD_WORK_POSTCODE : FIELD_HOME_POSTCODE, Addr[5]);
					if (Addr[6]) c->SetStr(IsWork ? FIELD_WORK_COUNTRY : FIELD_HOME_COUNTRY, Addr[6]);
				}
				else if (IsVar(Field, "note"))
				{
					c->SetStr(FIELD_NOTE, Data);
				}
				else if (IsVar(Field, "uid"))
				{
					GToken n(Data, ";", false);
					c->SetStr(FIELD_UID, n[0]);
				}
				else if (IsVar(Field, "x-perm"))
				{
					int Perms = atoi(Data);
					c->SetInt(FIELD_PERMISSIONS, Perms);
				}
				else if (IsVar(Field, "url"))
				{
					bool IsWork = IsType("work");
					bool IsHome = IsType("home");
					if (IsWork)
					{
						c->SetStr(FIELD_HOME_WEBPAGE, Data);
					}
					else if (IsHome)
					{
						c->SetStr(FIELD_WORK_WEBPAGE, Data);
					}
				}
				else if (IsVar(Field, "nickname"))
				{
					c->SetStr(FIELD_NICK, Data);
				}
			}			
		}
	}
	ExitLoop:

	if (Emails.Length())
	{
		if (PrefEmail < 0)
			PrefEmail = 0;

		c->SetStr(FIELD_EMAIL, Emails[PrefEmail]);
		Emails.DeleteAt(PrefEmail);

		if (Emails.Length())
		{
			GStringPipe p;
			for (int i=0; i<Emails.Length(); i++)
			{
				if (i) p.Print(",%s", Emails[i]);
				else p.Print("%s", Emails[i]);
			}

			char *v = p.NewStr();
			if (v)
			{
				c->SetStr(FIELD_ALT_EMAIL, v);
				DeleteArray(v);
			}
		}
	}

	ClearFields();

	return Status;
}

bool VIo::ReadField(GStreamI &s, char **Name, TypesList *Type, char **Data)
{
	bool Status = false;

	DeleteArray(*Name);
	if (Type) Type->Empty();
	DeleteArray(*Data);

	if (Name AND Data)
	{
		char Temp[256];
		GArray<char> p;
		bool Done = false;

		while (!Done)
		{
			int r = d->Buf.GetSize();
			if (r == 0)
			{
				r = s.Read(Temp, sizeof(Temp));
				if (r > 0)
				{
					d->Buf.Write(Temp, r);
				}
				else break;
			}

			bool EatNext = false;
			ReadNextLine:
			r = d->Buf.Pop(Temp, sizeof(Temp));
			while (r > 0 AND !ValidStr(Temp))
			{
				r = d->Buf.Pop(Temp, sizeof(Temp));
			}

			if (r > 0)
			{
				// Unfold
				for (char *c = Temp; *c; c++)
				{
					if (*c == '\r')
					{
						// do nothing
					}
					else if (*c == '\n')
					{
						TryPeekNext:
						char Next;
						r = d->Buf.Peek((uchar*) &Next, 1);
						if (r == 1)
						{
							if (Next == ' ' OR Next == '\t')
							{
								// Wrapped, do nothing
								EatNext = true;
								goto ReadNextLine;
							}
							else
							{
								Done = true;
								break;
							}
						}
						else
						{
							r = s.Read(Temp, sizeof(Temp));
							if (r > 0)
							{
								d->Buf.Write(Temp, r);
								goto TryPeekNext;
							}
							else break;
						}
					}
					else if (EatNext)
					{
						EatNext = false;
					}
					else
					{
						p.Add(*c);
					}
				}
			}
			else
			{
				r = s.Read(Temp, sizeof(Temp));
				if (r > 0)
				{
					d->Buf.Write(Temp, r);
				}
				else break;
			}
		}


		p.Add(0);
		char *f = p.Length() > 1 ? &p[0] : 0;
		if (f)
		{
			GHashTable Mod(0, false);
			char *e = strchr(f, ':');
			if (e)
			{
				*e++ = 0;
				GToken t(f, ";");
				if (t.Length() > 0)
				{
					*Name = NewStr(t[0]);
					for (int i=1; i<t.Length(); i++)
					{
						char *var = t[i];
						char *val = strchr(var, '=');
						if (val)
						{
							*val++ = 0;

							if (Type && !stricmp(var, "type"))
								Type->New().Reset(NewStr(val));
							else
								Mod.Add(var, NewStr(val));
						}
					}
				}
			}

			bool QuotedPrintable = false;
			char *Enc = (char*)Mod.Find("encoding");
			if (Enc)
			{
				if (stricmp(Enc, "quoted-printable") == 0)
				{
					QuotedPrintable = true;
				}
				else LgiTrace("%s:%i - Unknown encoding '%s'\n", __FILE__, __LINE__, Enc);
			}

			DeEscape(e, QuotedPrintable);

			char *Charset = (char*)Mod.Find("charset");
			if (Charset)
			{
				*Data = (char*)LgiNewConvertCp("utf-8", e, Charset);
			}
			else
			{
				*Data = NewStr(e);
			}

			Status = *Data != 0;

			for (char *m = (char*)Mod.First(); m; m = (char*)Mod.Next())
			{
				DeleteArray(m);
			}
		}
	}

	return Status;
}

void VIo::WriteField(GStreamI &s, char *Name, TypesList *Type, char *Data)
{
	if (Name AND Data)
	{
		int64 Size = s.GetSize();

		GStreamPrint(&s, "%s", Name);
		if (Type)
		{
			for (int i=0; i<Type->Length(); i++)
				GStreamPrint(&s, "%stype=%s", i?"":";", (*Type)[i]);
		}
		
		bool Is8Bit = false;
		bool HasEq = false;
		for (uint8 *c = (uint8*)Data; *c; c++)
		{
			if ((*c & 0x80) != 0)
			{
				Is8Bit = true;
			}
			else if (*c == '=')
			{
				HasEq = true;
			}
		}
		if (Is8Bit OR HasEq)
		{
			if (Is8Bit) s.Write((char*)";charset=utf-8", 14);
			s.Write((char*)";encoding=quoted-printable", 26);
		}

		s.Write((char*)":", 1);
		Fold(s, Data, s.GetSize() - Size);
		s.Write((char*)"\r\n", 2);
	}	
}

bool VCard::Export(GDataPropI *c, GStreamI *o)
{
	if (!c OR !o)
		return false;

	bool Status = true;
	char s[512];
	char *Empty = "";

	o->Push((char*)"begin:vcard\r\n");
	o->Push((char*)"version:3.0\r\n");

	char *First = 0, *Last = 0, *Title = 0;
	First = c->GetStr(FIELD_FIRST_NAME);
	Last = c->GetStr(FIELD_LAST_NAME);
	Title = c->GetStr(FIELD_TITLE);
	if (First || Last)
	{
		sprintf(s,
				"%s;%s;%s",
				Last?Last:Empty,
				First?First:Empty,
				Title?Title:Empty);
		WriteField(*o, "n", 0, s);
	}

	#define OutputTypedField(Field, Name, Type)		\
		{ char *str = 0;								\
		if (str = c->GetStr(Name))					\
		{											\
			WriteField(*o, Field, Type, str);			\
		} }

	#define OutputField(Field, Name)				\
		{ char *str = 0;								\
		if (str = c->GetStr(Name))					\
		{											\
			WriteField(*o, Field, 0, str);				\
		} }

	OutputTypedField("tel", FIELD_WORK_PHONE, &TypesList("Work"));
	OutputTypedField("tel", FIELD_WORK_MOBILE, &TypesList("Work,Cell"));
	OutputTypedField("tel", FIELD_WORK_FAX, &TypesList("Work,Fax"));
	OutputTypedField("tel", FIELD_HOME_PHONE, &TypesList("Home"));
	OutputTypedField("tel", FIELD_HOME_MOBILE, &TypesList("Home,Cell"));
	OutputTypedField("tel", FIELD_HOME_FAX, &TypesList("Home,Fax"));
	OutputField("org", FIELD_COMPANY);
	OutputTypedField("email", FIELD_EMAIL, &TypesList("internet,pref"));
	char *Alt;
	if (Alt = c->GetStr(FIELD_ALT_EMAIL))
	{
		GToken t(Alt, ",");
		for (int i=0; i<t.Length(); i++)
		{
			WriteField(*o, "email", &TypesList("internet"), t[i]);
		}
	}

	char * Uid;
	if (Uid = c->GetStr(FIELD_UID))
	{
		GStreamPrint(o, "UID:%s\r\n", Uid);
	}

	char *Street, *Suburb, *PostCode, *State, *Country;
	Street = Suburb = PostCode = State = Country = 0;
	Street = c->GetStr(FIELD_HOME_STREET);
	Suburb = c->GetStr(FIELD_HOME_SUBURB);
	PostCode = c->GetStr(FIELD_HOME_POSTCODE);
	State = c->GetStr(FIELD_HOME_STATE);
	Country = c->GetStr(FIELD_HOME_COUNTRY);
	if (Street OR Suburb OR PostCode OR State OR Country)
	{
		snprintf(s, sizeof(s)-1,
				";;%s;%s;%s;%s;%s",
				Street?Street:Empty,
				Suburb?Suburb:Empty,
				State?State:Empty,
				PostCode?PostCode:Empty,
				Country?Country:Empty);
		WriteField(*o, "adr", &TypesList("home"), s);
	}

	Street = Suburb = PostCode = State = Country = 0;
	Street = c->GetStr(FIELD_WORK_STREET);
	Suburb = c->GetStr(FIELD_WORK_SUBURB);
	PostCode = c->GetStr(FIELD_WORK_POSTCODE);
	State = c->GetStr(FIELD_WORK_STATE);
	Country = c->GetStr(FIELD_WORK_COUNTRY);
	if (Street OR Suburb OR PostCode OR State OR Country)
	{
		snprintf(s, sizeof(s)-1,
				";;%s;%s;%s;%s;%s",
				Street?Street:Empty,
				Suburb?Suburb:Empty,
				State?State:Empty,
				PostCode?PostCode:Empty,
				Country?Country:Empty);
		WriteField(*o, "adr", &TypesList("work"), s);
	}

	// OutputField("X-Perm", FIELD_PERMISSIONS);

	char *Url;
	if (Url = c->GetStr(FIELD_HOME_WEBPAGE))
	{
		WriteField(*o, "url", &TypesList("home"), Url);
	}
	if (Url = c->GetStr(FIELD_WORK_WEBPAGE))
	{
		WriteField(*o, "url", &TypesList("work"), Url);
	}
	char *Nick;
	if (Nick = c->GetStr(FIELD_NICK))
	{
		WriteField(*o, "nickname", 0, Nick);
	}
	char *Note;
	if (Note = c->GetStr(FIELD_NOTE))
	{
		WriteField(*o, "note", 0, Note);
	}

	o->Push((char*)"end:vcard\r\n");

	return Status;
}

/////////////////////////////////////////////////////////////
// VCal class
bool VCal::Import(GDataPropI *c, GStreamI *In)
{
	bool Status = false;

	if (!c OR !In)
		return false;

	#if 1

	char *Field = 0;
	TypesList Types;
	char *Data = 0;
	bool SetType = false;

	while (ReadField(*In, &Field, &Types, &Data))
	{
		if (stricmp(Field, "begin") == 0 AND
			stricmp(Data, "vcalendar") == 0)
		{
			while (ReadField(*In, &Field, &Types, &Data))
			{
				if (stricmp(Field, "end") == 0 AND
					stricmp(Data, "vcalendar") == 0)
					goto ExitLoop;
				

				if
				(
					stricmp(Field, "begin") == 0
					AND
					(
						stricmp(Data, "vevent") == 0
						OR
						stricmp(Data, "vtodo") == 0
					)
				)
				{
					char *SectionType = NewStr(Data);
					if (!SetType)
					{
						SetType = true;
						Status = true;

						int Type = stricmp(Data, "vtodo") == 0;
						c->SetInt(FIELD_CAL_TYPE, Type);
					}

					while (ReadField(*In, &Field, &Types, &Data))
					{
						if (stricmp(Field, "end") == 0 AND
							stricmp(Data, SectionType) == 0)
							break;

						if (IsVar(Field, "dtstart"))
						{
							GDateTime d;
							if (ParseDate(d, Data))
							{
								c->SetDate(FIELD_CAL_START_UTC, &d);
							}
						}
						else if (IsVar(Field, "dtend"))
						{
							GDateTime d;
							if (ParseDate(d, Data))
							{
								c->SetDate(FIELD_CAL_END_UTC, &d);
							}
						}
						else if (IsVar(Field, "summary"))
						{
							c->SetStr(FIELD_CAL_SUBJECT, Data);
						}
						else if (IsVar(Field, "description"))
						{
							char *Sum = UnMultiLine(Data);
							if (Sum)
							{
								c->SetStr(FIELD_CAL_NOTES, Sum);
								DeleteArray(Sum);
							}
						}
						else if (IsVar(Field, "location"))
						{
							c->SetStr(FIELD_CAL_LOCATION, Data);
						}
						else if (IsVar(Field, "uid"))
						{
							char *Uid = Data;
							c->SetStr(FIELD_UID, Uid);
						}
						else if (IsVar(Field, "x-showas"))
						{
							char *n = Data;

							if (stricmp(n, "TENTATIVE") == 0)
							{
								c->SetInt(FIELD_CAL_SHOW_TIME_AS, 1);
							}
							else if (stricmp(n, "BUSY") == 0)
							{
								c->SetInt(FIELD_CAL_SHOW_TIME_AS, 2);
							}
							else if (stricmp(n, "OUT") == 0)
							{
								c->SetInt(FIELD_CAL_SHOW_TIME_AS, 3);
							}
							else
							{
								c->SetInt(FIELD_CAL_SHOW_TIME_AS, 0);
							}
						}
						else if (IsVar(Field, "attendee"))
						{
							char *e = stristr(Data, "mailto=");
							if (e)
							{
								e += 7;

								/*
								char *Name = Variables["CN"];
								char *Role = Variables["Role"];

								GDataPropI *a = c->CreateAttendee();
								if (a)
								{
									a->SetStr(FIELD_ATTENDEE_NAME, Name);
									a->SetStr(FIELD_ATTENDEE_EMAIL, e);
									if (stricmp(Role, "CHAIR") == 0)
									{
										a->SetStr(FIELD_ATTENDEE_ATTENDENCE, 1);
									}
									else if (stricmp(Role, "REQ-PARTICIPANT") == 0)
									{
										a->SetStr(FIELD_ATTENDEE_ATTENDENCE, 2);
									}
									else if (stricmp(Role, "OPT-PARTICIPANT") == 0)
									{
										a->SetStr(FIELD_ATTENDEE_ATTENDENCE, 3);
									}
								}
								*/
							}
						}
					}

					DeleteArray(SectionType);
				}
			}
		}
	}
	ExitLoop:
	ClearFields();

	#else

	bool SetType = false;
	char *Unfolded = 0; // fixme Unfold(Text);
	GToken L(Unfolded, "\r\n");
	if (L.Length() > 0)
	{
		for (int i=0; i<L.Length(); i++)
		{
			char *Line = L[i];
			if (!Line) break;
			char *Eol = Line+strlen(Line)-1;
			while (Eol > Line AND strchr(" \t", *Eol)) *Eol-- = 0;

			Var.Empty();

			char *Colon = strchr(L[i], ':');
			if (Colon)
			{
				*Colon++ = 0;
				GToken Temp(L[i], 0);
				Var.AppendTokens(&Temp);
				Temp.Parse(Colon, 0);
				Var.AppendTokens(&Temp);

				if (!IsBeginEnd() AND
					IsObject("vcalendar"))
				{
					Status = true;
					IsTyped();
					IsEncoded();

					bool IsAnEvent = IsObject("vevent");
					bool IsAToDo = IsObject("vtodo");
					if (IsAnEvent OR IsAToDo)
					{
						if (!SetType)
						{
							SetType = true;

							int Type = IsAToDo ? 1 : 0;
							c->SetStr(FIELD_CAL_TYPE, Type);
						}

						if (IsObject("valarm"))
						{
							if (IsVar("trigger"))
							{
								if (IsValue("date-time"))
								{
									// don't really support this...
								}
								else
								{
									c->SetStr(FIELD_CAL_REMINDER_ACTION, true);
									c->SetStr(FIELD_CAL_REMINDER_TIME, 0); // reasonable default

									GDateTime Dt;
									int Sign = 0;
									if (ParseDuration(Dt, Sign, Data))
									{
										int d = Dt.Day() * 24 * 60;
										int h = Dt.Hours() * 60;
										int m = Dt.Minutes();
										c->SetStr(FIELD_CAL_REMINDER_TIME, -(d + h + m)); 
									}
								}
							}
						}
						else
						{
							if (IsVar("dtstart"))
							{
								GDateTime d;
								if (ParseDate(d, Data))
								{
									c->SetStr(FIELD_CAL_START_UTC, d);
								}
							}
							else if (IsVar("dtend"))
							{
								GDateTime d;
								if (ParseDate(d, Data))
								{
									c->SetStr(FIELD_CAL_END_UTC, d);
								}
							}
							else if (IsVar("summary"))
							{
								c->SetStr(FIELD_CAL_SUBJECT, Data);
							}
							else if (IsVar("description"))
							{
								char *Sum = UnMultiLine(Data);
								if (Sum)
								{
									c->SetStr(FIELD_CAL_NOTES, Sum);
									DeleteArray(Sum);
								}
							}
							else if (IsVar("location"))
							{
								c->SetStr(FIELD_CAL_LOCATION, Data);
							}
							else if (IsVar("uid"))
							{
								char *Uid = Data;
								if (isdigit(*Uid))
								{
									c->SetStr(FIELD_UID, atoi(Uid));
								}
								else
								{
									c->SetStr(FIELD_UID, Uid);
								}
							}
							else if (IsVar("x-showas"))
							{
								if (stricmp(Data, "TENTATIVE") == 0)
								{
									c->SetStr(FIELD_CAL_SHOW_TIME_AS, 1);
								}
								else if (stricmp(Data, "BUSY") == 0)
								{
									c->SetStr(FIELD_CAL_SHOW_TIME_AS, 2);
								}
								else if (stricmp(Data, "OUT") == 0)
								{
									c->SetStr(FIELD_CAL_SHOW_TIME_AS, 3);
								}
								else
								{
									c->SetStr(FIELD_CAL_SHOW_TIME_AS, 0);
								}
							}
							else if (IsVar("attendee"))
							{
								char *e = stristr(Data, "mailto=");
								if (e)
								{
									e += 7;

									char *Name = Variables["CN"];
									char *Role = Variables["Role"];

									GDataPropI *a = c->CreateAttendee();
									if (a)
									{
										a->SetStr(FIELD_ATTENDEE_NAME, Name);
										a->SetStr(FIELD_ATTENDEE_EMAIL, e);
										if (stricmp(Role, "CHAIR") == 0)
										{
											a->SetStr(FIELD_ATTENDEE_ATTENDENCE, 1);
										}
										else if (stricmp(Role, "REQ-PARTICIPANT") == 0)
										{
											a->SetStr(FIELD_ATTENDEE_ATTENDENCE, 2);
										}
										else if (stricmp(Role, "OPT-PARTICIPANT") == 0)
										{
											a->SetStr(FIELD_ATTENDEE_ATTENDENCE, 3);
										}
									}
								}
							}
						}
					}
				}
			}
		}
	}

	DeleteArray(Unfolded);

	#endif

	return Status;
}

bool VCal::Export(GDataPropI *c, GStreamI *o)
{
	if (!c OR !o)
		return false;

	char s[512];
	int Type = c->GetInt(FIELD_CAL_TYPE);
	char *TypeStr = Type == 1 ? (char*)"VTODO" : (char*)"VEVENT";

	o->Push((char*)"BEGIN:VCALENDAR\r\n");
	o->Push((char*)"VERSION:1.0\r\n");

	if (c)
	{
		GStreamPrint(o, "BEGIN:%s\r\n", TypeStr);

		#define OutputStr(Field, Name)						\
		{													\
			char *_s = 0;									\
			if (_s = c->GetStr(Field))						\
			{												\
				WriteField(*o, Name, 0, _s);				\
			}												\
		}

		OutputStr(FIELD_CAL_SUBJECT, "SUMMARY");
		OutputStr(FIELD_CAL_LOCATION, "LOCATION");
		OutputStr(FIELD_CAL_NOTES, "DESCRIPTION");

		int ShowAs;
		if (ShowAs = c->GetInt(FIELD_CAL_SHOW_TIME_AS))
		{
			switch (ShowAs)
			{
				default:
				case 0:
				{
					o->Push((char*)"X-SHOWAS:FREE\r\n");
					break;
				}
				case 1:
				{
					o->Push((char*)"X-SHOWAS:TENTATIVE\r\n");
					break;
				}
				case 2:
				{
					o->Push((char*)"X-SHOWAS:BUSY\r\n");
					break;
				}
				case 3:
				{
					o->Push((char*)"X-SHOWAS:OUT\r\n");
					break;
				}
			}
		}

		char *Uid;
		if (Uid = c->GetStr(FIELD_UID))
		{
			GStreamPrint(o, "UID:%s\r\n", Uid);
		}

		GDateTime *Dt;
		if (Dt = c->GetDate(FIELD_CAL_START_UTC))
		{
			Dt->ToUtc();
			GStreamPrint(o, "DTSTART:%04.4i%02.2i%02.2iT%02.2i%02.2i%02.2i\r\n",
					Dt->Year(), Dt->Month(), Dt->Day(),
					Dt->Hours(), Dt->Minutes(), Dt->Seconds());
		}
		if (Dt = c->GetDate(FIELD_CAL_END_UTC))
		{
			Dt->ToUtc();
			GStreamPrint(o, "DTEND:%04.4i%02.2i%02.2iT%02.2i%02.2i%02.2i\r\n",
					Dt->Year(), Dt->Month(), Dt->Day(),
					Dt->Hours(), Dt->Minutes(), Dt->Seconds());
		}

		int AlarmAction;
		int AlarmTime;
		if ((AlarmAction = c->GetInt(FIELD_CAL_REMINDER_ACTION)) &&
			AlarmAction &&
			(AlarmTime = c->GetInt(FIELD_CAL_REMINDER_TIME)))
		{
			o->Push((char*)"BEGIN:VALARM\r\n");
			GStreamPrint(o, "TRIGGER:%sPT%iM\r\n", (AlarmTime<0) ? "-" : "", abs(AlarmTime));
			o->Push((char*)"END:VALARM\r\n");
		}

		/*
		List<GDataPropI> Attendees;
		if (c->GetAttendees(Attendees))
		{
			for (GDataPropI *a=Attendees.First(); a; a=Attendees.Next())
			{
				// ATTENDEE;CN="Matthew Allen";ROLE=REQ-PARTICIPANT;RSVP=TRUE:MAILTO:matthew@cisra.canon.com.au

				int Attend = 2;
				char *Name = 0;
				char *Email = 0;
				int Response = 0;

				a->GetStr(FIELD_ATTENDEE_ATTENDENCE, Attend);
				a->GetStr(FIELD_ATTENDEE_NAME, Name);
				a->GetStr(FIELD_ATTENDEE_EMAIL, Email);
				a->GetStr(FIELD_ATTENDEE_RESPONSE, Response);

				if (Email)
				{
					strcpy(s, "ATTENDEE");
					if (Name)
					{
						sprintf(s+strlen(s), ";CN=\"%s\"", Name);
					}
					switch (Attend)
					{
						case 1:
						{
							strcat(s, ";ROLE=CHAIR");
							break;
						}
						default:
						case 2:
						{
							strcat(s, ";ROLE=REQ-PARTICIPANT");
							break;
						}
						case 3:
						{
							strcat(s, ";ROLE=OPT-PARTICIPANT");
							break;
						}
					}
					if (Email)
					{
						sprintf(s+strlen(s), ":MAILTO=%s", Email);
					}
					strcat(s, "\r\n");
					o->Push(s);
				}				
			}
		}
		*/

		GStreamPrint(o, "END:%s\r\n", TypeStr);
	}

	o->Push((char*)"END:VCALENDAR\r\n");

	return true;
}
