#include <stdlib.h>
#include <stdio.h>
#include <ctype.h>

#include "Lgi.h"
#include "vCard-vCal.h"
#include "GToken.h"
#include "ScribeDefs.h"

#define Push(s) Write(s, strlen(s))

#define ClearFields() \
	DeleteArray(Field); \
	Types.Empty(); \
	DeleteArray(Data)


#define IsType(str) (Types.Find(str) != 0)

#if 1
bool IsVar(char *field, const char *s)
{
	if (!s)
		return false;

	char *dot = strchr(field, '.');
	if (dot)
		return _stricmp(dot + 1, s) == 0;

	return _stricmp(field, s) == 0;
}
#else
#define IsVar(field, str) (field != 0 && _stricmp(field, str) == 0)
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
		else if (QuotedPrintable && *i == '=' && i[1] && i[2])
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
			if (d && strlen(d) == 8)
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
			if (t && strlen(t) >= 6)
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

		if (toupper(*In++) == 'P' &&
			toupper(*In++) == 'T')
		{
			while (*In)
			{
				int i = atoi(In);
				while (IsDigit(*In)) In++;

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
	for (char *s=i; s && *s;)
	{
		if (x >= 74)
		{
			// wrapping
			o.Write(i, (int)(s-i));
			o.Write((char*)"\r\n\t", 3);
			x = 0;
			i = s;
		}
		else if (*s == '=' || ((((uint8)*s) & 0x80) != 0))
		{
			// quoted printable
			o.Write(i, (int)(s-i));
			GStreamPrint(&o, "=%02.2x", (uint8)*s);
			x += 3;
			i = ++s;
		}
		else if (*s == '\n')
		{
			// new line
			o.Write(i, (int)(s-i));
			o.Write((char*)"\\n", 2);
			x += 2;
			i = ++s;
		}
		else if (*s == '\r')
		{
			o.Write(i, (int)(s-i));
			i = ++s;
		}
		else
		{
			s++;
			x++;
		}
	}
	o.Write(i, (int)strlen(i));
}

char *VIo::Unfold(char *In)
{
	if (In)
	{
		GStringPipe p(256);

		for (char *i=In; i && *i; i++)
		{
			if (*i == '\n')
			{
				if (i[1] && strchr(" \t", i[1]))
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
		for (char *i=In; i && *i; i=n)
		{
			n = stristr(i, "\\n");
			if (n)
			{
				p.Write(i, (int)(n-i));
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

	if (!c || !s)
		return false;

	char *Field = 0;
	TypesList Types;
	char *Data = 0;

	int PrefEmail = -1;
	GArray<char*> Emails;

	while (ReadField(*s, &Field, &Types, &Data))
	{
		if (_stricmp(Field, "begin") == 0 &&
			_stricmp(Data, "vcard") == 0)
		{
			while (ReadField(*s, &Field, &Types, &Data))
			{
				if (_stricmp(Field, "end") == 0 &&
					_stricmp(Data, "vcard") == 0)
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
					for (uint32 p=0; p<Phone.Length(); p++)
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
			for (uint32 i=0; i<Emails.Length(); i++)
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

	if (Name && Data)
	{
		char Temp[256];
		GArray<char> p;
		bool Done = false;

		while (!Done)
		{
			int64 r = d->Buf.GetSize();
			if (r == 0)
			{
				r = s.Read(Temp, sizeof(Temp));
				if (r > 0)
				{
					d->Buf.Write(Temp, (int)r);
				}
				else break;
			}

			bool EatNext = false;
			ReadNextLine:
			r = d->Buf.Pop(Temp, sizeof(Temp));
			while (r > 0 && !ValidStr(Temp))
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
							if (Next == ' ' || Next == '\t')
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
								d->Buf.Write(Temp, (int)r);
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
					d->Buf.Write(Temp, (int)r);
				}
				else break;
			}
		}


		p.Add(0);
		char *f = p.Length() > 1 ? &p[0] : 0;
		if (f)
		{
			GHashTbl<const char*, char*> Mod(0, false);
			char *e = strchr(f, ':');
			if (e)
			{
				*e++ = 0;
				GToken t(f, ";");
				if (t.Length() > 0)
				{
					*Name = NewStr(t[0]);
					for (uint32 i=1; i<t.Length(); i++)
					{
						char *var = t[i];
						char *val = strchr(var, '=');
						if (val)
						{
							*val++ = 0;

							if (Type && !_stricmp(var, "type"))
								Type->New().Reset(NewStr(val));
							else
								Mod.Add(var, NewStr(val));
						}
					}
				}
			}

			bool QuotedPrintable = false;
			char *Enc = Mod.Find("encoding");
			if (Enc)
			{
				if (_stricmp(Enc, "quoted-printable") == 0)
				{
					QuotedPrintable = true;
				}
				else LgiTrace("%s:%i - Unknown encoding '%s'\n", __FILE__, __LINE__, Enc);
			}

			DeEscape(e, QuotedPrintable);

			char *Charset = Mod.Find("charset");
			if (Charset)
			{
				*Data = (char*)LgiNewConvertCp("utf-8", e, Charset);
			}
			else
			{
				*Data = NewStr(e);
			}

			Status = *Data != 0;

			for (char *m = Mod.First(); m; m = Mod.Next())
			{
				DeleteArray(m);
			}
		}
	}

	return Status;
}

void VIo::WriteField(GStreamI &s, const char *Name, TypesList *Type, char *Data)
{
	if (Name && Data)
	{
		int64 Size = s.GetSize();

		GStreamPrint(&s, "%s", Name);
		if (Type)
		{
			for (uint32 i=0; i<Type->Length(); i++)
				GStreamPrint(&s, "%stype=%s", i?"":";", (*Type)[i].Get());
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
		if (Is8Bit || HasEq)
		{
			if (Is8Bit) s.Write((char*)";charset=utf-8", 14);
			s.Write((char*)";encoding=quoted-printable", 26);
		}

		s.Write((char*)":", 1);
		Fold(s, Data, (int) (s.GetSize() - Size));
		s.Write((char*)"\r\n", 2);
	}	
}

bool VCard::Export(GDataPropI *c, GStreamI *o)
{
	if (!c || !o)
		return false;

	bool Status = true;
	char s[512];
	const char *Empty = "";

	o->Push("begin:vcard\r\n");
	o->Push("version:3.0\r\n");

	char *First = 0, *Last = 0, *Title = 0;
	First = c->GetStr(FIELD_FIRST_NAME);
	Last = c->GetStr(FIELD_LAST_NAME);
	Title = c->GetStr(FIELD_TITLE);
	if (First || Last)
	{
		sprintf_s(s, sizeof(s),
				"%s;%s;%s",
				Last?Last:Empty,
				First?First:Empty,
				Title?Title:Empty);
		WriteField(*o, "n", 0, s);
	}

	#define OutputTypedField(Field, Name, Type)		\
		{ char *str = 0;								\
		if ((str = c->GetStr(Name)))					\
		{											\
			WriteField(*o, Field, Type, str);			\
		} }

	#define OutputField(Field, Name)				\
		{ char *str = 0;								\
		if ((str = c->GetStr(Name)))					\
		{											\
			WriteField(*o, Field, 0, str);				\
		} }

	TypesList Work("Work"), WorkCell("Work,Cell"), WorkFax("Work,Fax");
	TypesList Home("Home"), HomeCell("Home,Cell"), HomeFax("Home,Fax");
	TypesList InetPref("internet,pref"), Inet("internet");
	OutputTypedField("tel", FIELD_WORK_PHONE, &Work);
	OutputTypedField("tel", FIELD_WORK_MOBILE, &WorkCell);
	OutputTypedField("tel", FIELD_WORK_FAX, &WorkFax);
	OutputTypedField("tel", FIELD_HOME_PHONE, &Home);
	OutputTypedField("tel", FIELD_HOME_MOBILE, &HomeCell);
	OutputTypedField("tel", FIELD_HOME_FAX, &HomeFax);
	OutputField("org", FIELD_COMPANY);
	OutputTypedField("email", FIELD_EMAIL, &InetPref);
	char *Alt;
	if ((Alt = c->GetStr(FIELD_ALT_EMAIL)))
	{
		GToken t(Alt, ",");
		for (unsigned i=0; i<t.Length(); i++)
		{
			WriteField(*o, "email", &Inet, t[i]);
		}
	}

	char * Uid;
	if ((Uid = c->GetStr(FIELD_UID)))
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

	if (Street || Suburb || PostCode || State || Country)
	{
		sprintf_s(s, sizeof(s),
				";;%s;%s;%s;%s;%s",
				Street?Street:Empty,
				Suburb?Suburb:Empty,
				State?State:Empty,
				PostCode?PostCode:Empty,
				Country?Country:Empty);
		
		WriteField(*o, "adr", &Home, s);
	}

	Street = Suburb = PostCode = State = Country = 0;
	Street = c->GetStr(FIELD_WORK_STREET);
	Suburb = c->GetStr(FIELD_WORK_SUBURB);
	PostCode = c->GetStr(FIELD_WORK_POSTCODE);
	State = c->GetStr(FIELD_WORK_STATE);
	Country = c->GetStr(FIELD_WORK_COUNTRY);
	if (Street || Suburb || PostCode || State || Country)
	{
		sprintf_s(s, sizeof(s),
				";;%s;%s;%s;%s;%s",
				Street?Street:Empty,
				Suburb?Suburb:Empty,
				State?State:Empty,
				PostCode?PostCode:Empty,
				Country?Country:Empty);
		
		WriteField(*o, "adr", &Work, s);
	}

	// OutputField("X-Perm", FIELD_PERMISSIONS);

	char *Url;
	if ((Url = c->GetStr(FIELD_HOME_WEBPAGE)))
	{
		WriteField(*o, "url", &Home, Url);
	}
	if ((Url = c->GetStr(FIELD_WORK_WEBPAGE)))
	{
		WriteField(*o, "url", &Work, Url);
	}
	char *Nick;
	if ((Nick = c->GetStr(FIELD_NICK)))
	{
		WriteField(*o, "nickname", 0, Nick);
	}
	char *Note;
	if ((Note = c->GetStr(FIELD_NOTE)))
	{
		WriteField(*o, "note", 0, Note);
	}

	o->Push("end:vcard\r\n");

	return Status;
}

/////////////////////////////////////////////////////////////
// VCal class
bool VCal::Import(GDataPropI *c, GStreamI *In)
{
	bool Status = false;

	if (!c || !In)
		return false;

	char *Field = 0;
	TypesList Types;
	char *Data = 0;
	bool SetType = false;
	bool IsEvent = false;
	GAutoString SectionType;

	while (ReadField(*In, &Field, &Types, &Data))
	{
		if (IsCal)
		{
		    if (_stricmp(Field, "end") == 0 &&
			    _stricmp(Data, "vcalendar") == 0)
		    {
		        IsCal = false;
		        break;
		    }
		    
		    if (IsEvent)
		    {
		        if (_stricmp(Field, "end") == 0 &&
			        _stricmp(Data, SectionType) == 0)
		        {
		            Status = true;
			        break;
		        }
				
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
				    GAutoString Sum(UnMultiLine(Data));
				    if (Sum)
						c->SetStr(FIELD_CAL_NOTES, Sum);
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

					if (_stricmp(n, "TENTATIVE") == 0)
					{
						c->SetInt(FIELD_CAL_SHOW_TIME_AS, 1);
					}
					else if (_stricmp(n, "BUSY") == 0)
					{
						c->SetInt(FIELD_CAL_SHOW_TIME_AS, 2);
					}
					else if (_stricmp(n, "OUT") == 0)
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
							if (_stricmp(Role, "CHAIR") == 0)
							{
								a->SetStr(FIELD_ATTENDEE_ATTENDENCE, 1);
							}
							else if (_stricmp(Role, "REQ-PARTICIPANT") == 0)
							{
								a->SetStr(FIELD_ATTENDEE_ATTENDENCE, 2);
							}
							else if (_stricmp(Role, "OPT-PARTICIPANT") == 0)
							{
								a->SetStr(FIELD_ATTENDEE_ATTENDENCE, 3);
							}
						}
						*/
					}
				}
			}
		    else if
		    (
		        _stricmp(Field, "begin") == 0
			    &&
			    (
				    _stricmp(Data, "vevent") == 0
				    ||
				    _stricmp(Data, "vtodo") == 0
			    )
		    )
		    {
		        IsEvent = true;
		        SectionType.Reset(NewStr(Data));

				int Type = _stricmp(Data, "vtodo") == 0 ? 1 : 0;
				c->SetInt(FIELD_CAL_TYPE, Type);
		    }
		}
		else if (_stricmp(Field, "begin") == 0 &&
			_stricmp(Data, "vcalendar") == 0)
		{
		    IsCal = true;
		}
	}

	ClearFields();

	return Status;
}

bool VCal::Export(GDataPropI *c, GStreamI *o)
{
	if (!c || !o)
		return false;

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
			if ((_s = c->GetStr(Field)))					\
			{												\
				WriteField(*o, Name, 0, _s);				\
			}												\
		}

		OutputStr(FIELD_CAL_SUBJECT, "SUMMARY");
		OutputStr(FIELD_CAL_LOCATION, "LOCATION");
		OutputStr(FIELD_CAL_NOTES, "DESCRIPTION");

		int ShowAs;
		if ((ShowAs = c->GetInt(FIELD_CAL_SHOW_TIME_AS)))
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
		if ((Uid = c->GetStr(FIELD_UID)))
		{
			GStreamPrint(o, "UID:%s\r\n", Uid);
		}

		GDateTime *Dt;
		if ((Dt = c->GetDate(FIELD_CAL_START_UTC)))
		{
			Dt->ToUtc();
			GStreamPrint(o, "DTSTART:%04.4i%02.2i%02.2iT%02.2i%02.2i%02.2i\r\n",
					Dt->Year(), Dt->Month(), Dt->Day(),
					Dt->Hours(), Dt->Minutes(), Dt->Seconds());
		}
		if ((Dt = c->GetDate(FIELD_CAL_END_UTC)))
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
						int len = strlen(s);
						sprintf_s(s+len, sizeof(s)-len, ";CN=\"%s\"", Name);
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
						int len = strlen(s)
						sprintf_s(s+len, sizeof(s)-len, ":MAILTO=%s", Email);
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
