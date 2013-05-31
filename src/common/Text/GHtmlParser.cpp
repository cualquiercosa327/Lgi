#include "Lgi.h"
#include "GDocView.h"
#include "GToken.h"
#include "GHtmlStatic.h"
#include "GHtmlParser.h"

#define IsBlock(d)		((d) == GCss::DispBlock)

char *GHtmlParser::NextTag(char *s)
{
	while (s && *s)
	{
		char *n = strchr(s, '<');
		if (n)
		{
			if (!n[1])
				return NULL;

			if (IsAlpha(n[1]) || strchr("!/", n[1]) || n[1] == '?')
			{
				return n;
			}

			s = n + 1;
		}
		else break;
	}

	return 0;
}

GHtmlElement *GHtmlParser::GetOpenTag(const char *Tag)
{
	if (Tag)
	{
		for (GHtmlElement *t=OpenTags.Last(); t; t=OpenTags.Prev())
		{
			if (t->Tag)
			{
				if (stricmp(t->Tag, Tag) == 0)
				{
					return t;
				}

				if (stricmp(t->Tag, "table") == 0)
				{
					// stop looking... we don't close tags outside
					// the table from inside.
					break;
				}				
			}
		}
	}

	return 0;
}

void GHtmlParser::SkipNonDisplay(char *&s)
{
	while (*s)
	{
		SkipWhiteSpace(s);

		if (s[0] == '<' &&
			s[1] == '!' &&
			s[2] == '-' &&
			s[3] == '-')
		{
			s += 4;
			char *e = strstr(s, "-->");
			if (e)
				s = e + 3;
			else
			{
				s += strlen(s);
				break;
			}
		}
		else break;
	}
}

char *GHtmlParser::ParsePropValue(char *s, char *&Value)
{
	Value = 0;
	if (s)
	{
		if (strchr("\"\'", *s))
		{
			char Delim = *s++;
			char *Start = s;
			while (*s && *s != Delim) s++;
			Value = NewStr(Start, s - Start);
			s++;
		}
		else
		{
			char *Start = s;
			while (*s && !IsWhiteSpace(*s) && *s != '>') s++;
			Value = NewStr(Start, s - Start);
		}
	}

	return s;
}

char *GHtmlParser::ParseName(char *s, char **Name)
{
	GAutoString a;
	s = ParseName(s, a);
	if (Name)
		*Name = a.Release();
	return s;
}

char *GHtmlParser::ParseName(char *s, GAutoString &Name)
{
	SkipWhiteSpace(s);
	char *Start = s;
	while (*s && (IsAlpha(*s) || strchr("!-:", *s) || IsDigit(*s)))
	{
		s++;
	}

	int Len = s - Start;
	if (Len > 0)
	{
		Name.Reset(NewStr(Start, Len));
	}
	
	return s;
}

char *GHtmlParser::ParsePropList(char *s, GHtmlElement *Obj, bool &Closed)
{
	while (s && *s && *s != '>')
	{
		while (*s && IsWhiteSpace(*s)) s++;
		if (*s == '>') break;

		// get name
		char *Name = 0;
		char *n = ParseName(s, &Name);		
		if (*n == '/')
		{
			Closed = true;
		}		
		if (n == s)
		{
			s = ++n;
		}
		else
		{
			s = n;
		}

		while (*s && IsWhiteSpace(*s)) s++;

		if (*s == '=')
		{
			// get value
			s++;
			while (*s && IsWhiteSpace(*s)) s++;

			char *Value = 0;
			s = ParsePropValue(s, Value);

			if (Value && Name)
			{
				GVariant v;
				v.OwnStr(Value);
				Obj->SetValue(Name,  v);
			}
			else
			{
				DeleteArray(Value);
			}
		}

		DeleteArray(Name);
	}

	if (*s == '>') s++;

	return s;
}

GHtmlElemInfo *GHtmlParser::GetTagInfo(const char *Tag)
{
	static GHtmlElemInfo Unknown = {TAG_UNKNOWN, 0, 0, GHtmlElemInfo::TI_NONE};
	GHtmlElemInfo *i = GHtmlStatic::Inst->TagMap.Find(Tag);
	return i ? i : &Unknown;
}

char *GHtmlParser::ParseHtml(GHtmlElement *Elem, char *Doc, int Depth, bool InPreTag, bool *BackOut)
{
	#if CRASH_TRACE
	LgiTrace("::ParseHtml Doc='%.10s'\n", Doc);
	#endif

	if (Depth >= 1024)
	{
		// Bail
		return Doc + strlen(Doc);
	}

	bool IsFirst = true;
	for (char *s=Doc; s && *s; )
	{
		char *StartTag = s;

		if (*s == '<')
		{
			if (s[1] == '?')
			{
				// Dynamic content
				s += 2;
				while (*s && IsWhiteSpace(*s)) s++;

				if (strnicmp(s, "xml:namespace", 13) == 0)
				{
					// Ignore Outlook generated HTML tag
					char *e = strchr(s, '/');
					while (e)
					{
						if (e[1] == '>'
							||
							(e[1] == '?' && e[2] == '>'))
						{
							if (e[1] == '?')
								s = e + 3;
							else
								s = e + 2;
							break;
						}

						e = strchr(e + 1, '/');
					}

					if (!e)
						LgiAssert(0);
				}
				else
				{
					char *Start = s;
					while (*s && (!(s[0] == '?' && s[1] == '>')))
					{
						if (strchr("\'\"", *s))
						{
							char d = *s++;
							s = strchr(s, d);
							if (s) s++;
							else break;
						}
						else s++;
					}

					if (s)
					{
						if (s[0] == '?' &&
							s[1] == '>' &&
							View && View->GetEnv())
						{
							char *e = s - 1;
							while (e > Start && IsWhiteSpace(*e)) e--;
							e++;
							char *Code = NewStr(Start, e - Start);
							if (Code)
							{
								char *Result = View->GetEnv()->OnDynamicContent(Code);
								if (Result)
								{
									char *p = Result;
									do
									{
										GHtmlElement *c = CreateElement(Elem);
										if (c)
										{
											p = ParseHtml(c, p, Depth + 1, InPreTag);
										}
										else break;
									}
									while (ValidStr(p));

									DeleteArray(Result);
								}

								DeleteArray(Code);
							}

							s += 2;
						}
					}
				}
			}
			else if (s[1] == '!' &&
					s[2] == '-' &&
					s[3] == '-')
			{
				// Comment
				s = strstr(s, "-->");
				if (s) s += 3;
			}
			else if (s[1] == '!' &&
					s[2] == '[')
			{
				// Parse conditional...
				char *StartTag = s;
				s += 3;
				char *Cond = 0;
				s = ParseName(s, &Cond);
				if (!Cond)
				{
					while (*s && *s != ']')
						s++;
					if (*s == ']') s++;
					if (*s == '>') s++;
					return s;
				}

				bool IsEndIf = false;
				if (!stricmp(Cond, "if"))
				{
					if (!IsFirst)
					{
						DeleteArray(Cond);
						s = StartTag;
						goto DoChildTag;
					}

					Elem->TagId = CONDITIONAL;
					SkipWhiteSpace(s);
					char *Start = s;
					while (*s && *s != ']')
						s++;
					Elem->Condition.Reset(NewStr(Start, s-Start));
					Elem->Tag.Reset(NewStr("[if]"));
					Elem->Info = GetTagInfo(Elem->Tag);					
					Elem->Display(GCss::DispNone);
				}
				else if (!stricmp(Cond, "endif"))
				{
					IsEndIf = true;
				}
				DeleteArray(Cond);
				if (*s == ']') s++;
				if (*s == '>') s++;
				if (IsEndIf)
					return s;
			}
			else if (s[1] == '!')
			{
				s += 2;
				s = strchr(s, '>');
				if (s)
					s++;
				else
					return NULL;
			}
			else if (IsAlpha(s[1]))
			{
				// Start tag
				if (Elem->Parent && IsFirst)
				{
					// My tag
					s = ParseName(++s, Elem->Tag);
					if (!Elem->Tag)
					{
					    if (BackOut)
						    *BackOut = true;
						return s;
					}

					bool TagClosed = false;
					s = ParsePropList(s, Elem, TagClosed);

					if (stricmp("th", Elem->Tag) == 0)
					{
						Elem->Tag.Reset(NewStr("td"));
					}
					
					Elem->Info = GetTagInfo(Elem->Tag);
					if (Elem->Info)
					{
						Elem->TagId = Elem->Info->Id;
						Elem->Display
						(
							TestFlag
							(
								Elem->Info->Flags,
								GHtmlElemInfo::TI_BLOCK
							)
							||
							(
								Elem->Tag
								&&
								Elem->Tag[0] == '!'
							)
							?
							GCss::DispBlock
							:
							GCss::DispInline
						);
						if (Elem->TagId == TAG_PRE)
						{
							InPreTag = true;
						}
					}

					if (IsBlock(Elem->Display()) || Elem->TagId == TAG_BR)
					{
						SkipNonDisplay(s);
					}

					switch (Elem->TagId)
					{
						default: break;
						case TAG_SCRIPT:
						{
							char *End = stristr(s, "</script>");
							if (End)
							{
								if (View && View->GetEnv())
								{
									*End = 0;
									GVariant Lang, Type;
									Elem->GetValue("language", Lang);
									Elem->GetValue("type", Type);
									View->GetEnv()->OnCompileScript(s, Lang.Str(), Type.Str());
									*End = '<';
								}

								s = End;
							}
							break;
						}
						case TAG_TABLE:
						{
							if (Elem->Parent->TagId == TAG_TABLE)
							{
								// Um no...
								if (BackOut)
								{
									GHtmlElement *l = OpenTags.Last();
									if (l && l->TagId == TAG_TABLE)
									{
										CloseTag(l);
									}

									*BackOut = true;
									return StartTag;
								}
							}
							break;
						}
						case TAG_IFRAME:
						{
							GVariant Src;
							if (Elem->GetValue("src", Src) && View && View->GetEnv())
							{
								GDocumentEnv::LoadJob *j = View->GetEnv()->NewJob();
								if (j)
								{
									j->Uri.Reset(Src.ReleaseStr());
									j->View = View;
									j->UserData = this;
									// j->UserUid = Html->d->DocumentUid;

									GDocumentEnv::LoadType Result = View->GetEnv()->GetContent(j);
									if (Result == GDocumentEnv::LoadImmediate)
									{
										if (j->Stream)
										{
											uint64 Len = j->Stream->GetSize();
											if (Len > 0)
											{
												GAutoString a(new char[Len+1]);
												int r = j->Stream->Read(a, Len);
												a[r] = 0;
												
												GHtmlElement *Child = CreateElement(Elem);
												if (Child)
												{
													bool BackOut = false;
													ParseHtml(Child, a, Depth + 1, false, &BackOut);
												}
											}
										}
									}
									DeleteObj(j);
								}
							}
							break;
						}
					}
					
					Elem->SetStyle();

					if (Elem->TagId == TAG_STYLE)
					{
						char *End = stristr(s, "</style>");
						if (End)
						{
							if (View)
							{
								char *Css = NewStr(s, End - s);
								if (Css)
								{
									View->OnAddStyle("text/css", Css);
								}
							}

							s = End;
						}
						else
						{
							// wtf?
						}
					}

					/* FIXME???
					if (TagId == TAG_P)
					{
						GHtmlElement *p;
						if (p = Html->GetOpenTag("p"))
						{
							return s;
						}
					}
					*/

					if (TagClosed || Elem->Info->NeverCloses())
					{
						return s;
					}

					if (Elem->Info->ReattachTo)
					{
						GToken T(Elem->Info->ReattachTo, ",");
						const char *Reattach = Elem->Info->ReattachTo;
						for (int i=0; i<T.Length(); i++)
						{
							if (Elem->Parent->Tag &&
								stricmp(Elem->Parent->Tag, T[i]) == 0)
							{
								Reattach = 0;
								break;
							}
						}

						if (Reattach)
						{
							if (Elem->TagId == TAG_HEAD)
							{
								// Ignore it..
								return s;
							}
							else
							{
								GHtmlElement *Last = 0;
								for (GHtmlElement *t=OpenTags.Last(); t; t=OpenTags.Prev())
								{
									if (t->Tag)
									{
										if (stricmp(t->Tag, Elem->Tag) == 0)
										{
											CloseTag(t);
											t = OpenTags.Current();
											if (!t) t = OpenTags.Last();
										}

										if (t && t->Tag && stricmp(t->Tag, Reattach) == 0)
										{
											Last = t;
											break;
										}
									}

									if (!t || t->TagId == TAG_TABLE)
										break;
								}

								if (Last)
								{
									Last->Attach(Elem);
								}
							}
						}
					}

					OpenTags.Insert(Elem);
				}
				else
				{
					// Child tag
					DoChildTag:
					GHtmlElement *c = CreateElement(Elem);
					if (c)
					{
						bool BackOut = false;

						s = ParseHtml(c, s, Depth + 1, InPreTag, &BackOut);
						if (BackOut)
						{
							c->Detach();
							DeleteObj(c);
							return s;
						}
						else if (IsBlock(c->Display()))
						{
							GHtmlElement *Last;
							while (c->Children.Length())
							{
								Last = c->Children[c->Children.Length()-1];

								if (Last->TagId == CONTENT &&
									!ValidStrW(Last->Text))
								{
									Last->Detach();
									DeleteObj(Last);
								}
								else break;
							}
						}
					}
				}
			}
			else if (s[1] == '/')
			{
				// End tag
				char *PreTag = s;
				s += 2;

				// This code segment detects out of order HTML tags
				// and skips them. If we didn't do this then the parser
				// would get stuck on a Tag which has already been closed
				// and would return to the very top of the recursion.
				//
				// e.g.
				//		<font>
				//			<b>
				//			</font>
				//		</b>
				char *EndBracket = strchr(s, '>');
				if (EndBracket)
				{
					char *e = EndBracket;
					while (e > s && strchr(WhiteSpace, e[-1]))
						e--;
					char Prev = *e;
					*e = 0;
					GHtmlElement *Open = GetOpenTag(s);
					*e = Prev;
					
					if (Open)
					{
						Open->WasClosed = true;
					}
					else
					{
						s = EndBracket + 1;
						continue;
					}
				}
				else
				{
					s += strlen(s);
					continue;
				}

				if (Elem->Tag)
				{
					// Compare against our tag
					char *t = Elem->Tag;
					while (*s && *t && toupper(*s) == toupper(*t))
					{
						s++;
						t++;
					}

					SkipWhiteSpace(s);

					if (*s == '>')
					{
						GHtmlElement *t;
						while ((t = OpenTags.Last()))
						{
							CloseTag(t);
							if (t == Elem)
							{
								break;
							}
						}
						s++;

						if (IsBlock(Elem->Display()) || Elem->TagId == TAG_BR)
						{
							SkipNonDisplay(s);
						}

						if (Elem->Parent)
						{
							return s;
						}
					}
				}
				else
					LgiAssert(!"This should not happen?");

				if (Elem->Parent)
				{
					return PreTag;
				}
			}
			else
			{
				goto PlainText;
			}
		}
		else if (*s)
		{
			// Text child
			PlainText:
			char *n = NextTag(s);
			int Len = n ? n - s : strlen(s);
			GAutoWString WStr(CleanText(s, Len, true, InPreTag));
			if (WStr && *WStr)
			{
				// This loop processes the text into lengths that need different treatment
				enum TxtClass
				{
					TxtNone,
					TxtEmoji,
					TxtEol,
					TxtNull,
				};

				char16 *Start = WStr;
				GHtmlElement *Child = NULL;
				for (char16 *c = WStr; true; c++)
				{
					TxtClass Cls = TxtNone;
					
					/*
					if (Html->d->DecodeEmoji && *c >= EMOJI_START && *c <= EMOJI_END)
						Cls = TxtEmoji;
					else
					*/
					if (InPreTag && *c == '\n')
						Cls = TxtEol;
					else if (!*c)
						Cls = TxtNull;

					if (Cls)
					{
						if (c > Start)
						{
							// Emit the text before the point of interest...
							GAutoWString Cur;
							if (Start == WStr && !*c)
							{
								// Whole string
								Cur = WStr;
							}
							else
							{
								// Sub-string
								Cur.Reset(NewStrW(Start, c - Start));
							}

							if (Elem->Children.Length() == 0 &&
								(!Elem->Info || !Elem->Info->NoText()) &&
								!Elem->Text)
							{
								Elem->Text = Cur;
							}
							else if ((Child = CreateElement(Elem)))
							{
								Child->Text = Cur;
							}
						}

						// Now process the text of interest...
						/*
						if (Cls == TxtEmoji)
						{
							// Emit the emoji image
							GHtmlElement *img = CreateElement(Elem);
							if (img)
							{
								img->Tag.Reset(NewStr("img"));
								if ((img->Info = GetTagInfo(img->Tag)))
									img->TagId = img->Info->Id;

								GRect rc;
								EMOJI_CH2LOC(*c, rc);

								img->Set("src", Html->d->EmojiImg);

								char css[256];
								sprintf(css, "x-rect: rect(%i,%i,%i,%i);", rc.y1, rc.x2, rc.y2, rc.x1);
								img->Set("style", css);
								img->SetStyle();
							}
							Start = c + 1;
						}
						else
						*/
						if (Cls == TxtEol)
						{
							// Emit the <br> tag
							GHtmlElement *br = CreateElement(Elem);
							if (br)
							{
								br->Tag.Reset(NewStr("br"));
								if ((br->Info = GetTagInfo(br->Tag)))
									br->TagId = br->Info->Id;
							}
							Start = c + 1;
						}
					}
					
					// Check for the end of string...
					if (!*c)
						break;
				}

			}

			s = n;
		}

		IsFirst = false;
	}

	#if CRASH_TRACE
	LgiTrace("::ParseHtml end\n");
	#endif

	return 0;
}

char16 *GHtmlParser::CleanText(const char *s, int Len, bool ConversionAllowed, bool KeepWhiteSpace)
{
	static const char *DefaultCs = "iso-8859-1";
	char16 *t = 0;

	if (s && Len > 0)
	{
		bool Has8 = false;
		if (Len >= 0)
		{
			for (int n = 0; n < Len; n++)
			{
				if (s[n] & 0x80)
				{
					Has8 = true;
					break;
				}
			}
		}
		else
		{
			for (int n = 0; s[n]; n++)
			{
				if (s[n] & 0x80)
				{
					Has8 = true;
					break;
				}
			}
		}
		
		bool DocAndCsTheSame = false;
		if (DocCharSet && View->GetCharset())
		{
			DocAndCsTheSame = stricmp(DocCharSet, View->GetCharset()) == 0;
		}

		if (DocCharSet &&
			View->GetCharset() &&
			!View->GetOverideDocCharset())
		{
			char *DocText = (char*)LgiNewConvertCp(DocCharSet, s, View->GetCharset(), Len);
			t = (char16*) LgiNewConvertCp(LGI_WideCharset, DocText, DocCharSet, -1);
			DeleteArray(DocText);
		}
		else if (DocCharSet)
		{
			t = (char16*) LgiNewConvertCp(LGI_WideCharset, s, DocCharSet, Len);
		}
		else
		{
			t = (char16*) LgiNewConvertCp(LGI_WideCharset, s, View->GetCharset() ? View->GetCharset() : DefaultCs, Len);
		}

		if (t && ConversionAllowed)
		{
			char16 *o = t;
			for (char16 *i=t; *i; )
			{
				switch (*i)
				{
					case '&':
					{
						i++;
						
						if (*i == '#')
						{
							// Unicode Number
							char n[32] = "", *p = n;
							
							i++;
							
							if (*i == 'x' || *i == 'X')
							{
								// Hex number
								i++;
								while (	*i &&
										(
											IsDigit(*i) ||
											(*i >= 'A' && *i <= 'F') ||
											(*i >= 'a' && *i <= 'f')
										) &&
										(p - n) < 31)
								{
									*p++ = *i++;
								}
							}
							else
							{
								// Decimal number
								while (*i && IsDigit(*i) && (p - n) < 31)
								{
									*p++ = *i++;
								}
							}
							*p++ = 0;
							
							char16 Ch = atoi(n);
							if (Ch)
							{
								*o++ = Ch;
							}
							
							if (*i && *i != ';')
								i--;
						}
						else
						{
							// Named Char
							char16 *e = i;
							while (*e && IsAlpha(*e) && *e != ';')
							{
								e++;
							}
							
							GAutoWString Var(NewStrW(i, e-i));							
							char16 Char = GHtmlStatic::Inst->VarMap.Find(Var);
							if (Char)
							{
								*o++ = Char;
								i = e;
							}
							else
							{
								i--;
								*o++ = *i;
							}
						}
						break;
					}
					case '\r':
					{
						break;
					}
					case ' ':
					case '\t':
					case '\n':
					{
						if (KeepWhiteSpace)
						{
							*o++ = *i;
						}
						else
						{
							*o++ = ' ';

							// Skip furthur whitespace
							while (i[1] && IsWhiteSpace(i[1]))
							{
								i++;
							}
						}
						break;
					}
					default:
					{
						// Normal char
						*o++ = *i;
						break;
					}
				}

				if (*i) i++;
				else break;
			}
			*o++ = 0;
		}
	}

	if (t && !*t)
	{
		DeleteArray(t);
	}

	return t;
}

void GHtmlParser::_TraceOpenTags()
{
	GStringPipe p;
	for (GHtmlElement *t=OpenTags.First(); t; t=OpenTags.Next())
	{
		p.Print(", %s", t->Tag);
		
		GVariant Id;
		if (t->GetValue("id", Id))
		{
			p.Print("#%s", Id.Str());
		}
	}
	char *s = p.NewStr();
	if (s)
	{
		LgiTrace("Open tags = '%s'\n", s + 2);
		DeleteArray(s);
	}
}
