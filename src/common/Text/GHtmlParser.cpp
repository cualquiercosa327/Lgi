#include "Lgi.h"
#include "GDocView.h"
#include "GToken.h"
#include "GHtmlCommon.h"
#include "GHtmlParser.h"
#include "GUtf8.h"

#define FEATURE_REATTACH_ELEMENTS		1
#define IsBlock(d)						((d) == GCss::DispBlock)

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
				if (_stricmp(t->Tag, Tag) == 0)
				{
					return t;
				}

				if (_stricmp(t->Tag, "table") == 0)
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

char16 *GHtmlParser::DecodeEntities(const char *s, int len)
{
	char16 buf[256];
	char16 *o = buf;
	const char *end = s + len;
	GStringPipe p(256);

	for (const char *i = s; i < end; )
	{
		if (o - buf > CountOf(buf) - 32)
		{
			// We are getting near the end of the buffer...
			// push existing data into the GStringPipe and
			// reset the output ptr.
			p.Write(buf, (o - buf) * sizeof(*o) );
			o = buf;
		}
		
		switch (*i)
		{
			case '&':
			{
				i++;
				
				if (*i == '#')
				{
					// Unicode Number
					char n[32] = "", *p = n;
					char16 Ch;
					
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
						*p++ = 0;
						Ch = htoi(n);
					}
					else
					{
						// Decimal number
						while (*i && IsDigit(*i) && (p - n) < 31)
						{
							*p++ = *i++;
						}
						*p++ = 0;
						Ch = atoi(n);
					}
					
					if (Ch)
						*o++ = Ch;
					
					if (*i && *i != ';')
						i--;
				}
				else
				{
					// Named Char
					const char *e = i;
					while (*e && IsAlpha(*e) && *e != ';')
					{
						e++;
					}
					
					GAutoWString Var(LgiNewUtf8To16(i, e-i));
					uint32 Char = GHtmlStatic::Inst->VarMap.Find(Var);
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
				*o++ = *i;
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
	*o = 0;
	
	if (p.GetSize() > 0)
	{
		// Long string mode... use the GStringPipe
		p.Write(buf, (o - buf) * sizeof(*o));
		return p.NewStrW();
	}
	
	return NewStrW(buf, o - buf);
}

char *GHtmlParser::ParsePropValue(char *s, char16 *&Value)
{
	Value = 0;
	if (s)
	{
		if (strchr("\"\'", *s))
		{
			char Delim = *s++;
			char *Start = s;
			while (*s && *s != Delim) s++;
			Value = DecodeEntities(Start, s - Start);
			s++;
		}
		else
		{
			char *Start = s;
			while (*s && !IsWhiteSpace(*s) && *s != '>') s++;
			Value = DecodeEntities(Start, s - Start);
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

			char16 *Value = 0;
			s = ParsePropValue(s, Value);

			if (Name && Value)
			{
				#if defined(_DEBUG) && 0
				if (!_stricmp(Name, "debug"))
				{
					int asd=0;
				}
				#endif
				Obj->Set(Name, Value);
			}

			DeleteArray(Value);
		}

		DeleteArray(Name);
	}

	if (*s == '>') s++;

	return s;
}

GHtmlElemInfo *GHtmlParser::GetTagInfo(const char *Tag)
{
	LgiAssert(GHtmlStatic::Inst != NULL);
	return GHtmlStatic::Inst->GetTagInfo(Tag);
}

void DumpDomTree(GHtmlElement *e, int Depth = 0)
{
	char Sp[256];
	int d = Depth << 1;
	memset(Sp, ' ', d);
	Sp[d] = 0;
	LgiTrace("%s%s (%p,%p)\n", Sp, e->Tag.Get(), e, e->Parent);
	for (unsigned i=0; i<e->Children.Length(); i++)
	{
		DumpDomTree(e->Children[i], Depth+1);
	}
}

bool GHtmlParser::Parse(GHtmlElement *Root, const char *Doc)
{
	SourceData.Empty();
	CurrentSrc = Doc;
	ParseHtml(Root, (char*)Doc, 0);
	
	// DumpDomTree(Root);
	
	if (CurrentSrc)
		SourceData.Write(CurrentSrc, strlen(CurrentSrc));	
	Source.Reset(SourceData.NewStr());
	return true;
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
				
				// Write out the document before the dynamic section
				if (s > CurrentSrc)
				{
					SourceData.Write(CurrentSrc, s - CurrentSrc);
				}
				
				// Process dynamic section
				s += 2;
				while (*s && IsWhiteSpace(*s)) s++;

				if (_strnicmp(s, "xml:namespace", 13) == 0)
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
							GAutoString Code(NewStr(Start, e - Start));
							if (Code)
							{
								GAutoString Result(View->GetEnv()->OnDynamicContent(View, Code));
								if (Result)
								{
									// Save the dynamic code to the source pipe
									SourceData.Write(Result, strlen(Result));
									
									// Create some new elements based on the dynamically generated string
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
								}
							}

							s += 2;
						}
					}
				}
				
				// Move current position to after the dynamic section
				CurrentSrc = s;
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
				if (!_stricmp(Cond, "if"))
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
					
					if (!EvaluateCondition(Elem->Condition))
						Elem->Display(GCss::DispNone);
					else
						Elem->Display(GCss::DispInline);
					
					OpenTags.Add(Elem);
				}
				else if (!_stricmp(Cond, "endif"))
				{
					GHtmlElement *MatchingIf = NULL;
					List<GHtmlElement>::I it = OpenTags.End();
					for (GHtmlElement *e = *it; e; e = *--it)
					{
						if (e->TagId == CONDITIONAL &&
							e->Tag &&
							!_stricmp(e->Tag, "[if]"))
						{
							MatchingIf = e;
							break;
						}
					}
					if (MatchingIf)
					{
						MatchingIf->WasClosed = true;
						IsEndIf = true;
						OpenTags.Delete(MatchingIf);
					}
				}
				DeleteArray(Cond);
				while (*s && *s != '>')
					s++;
				if (*s == '>')
					s++;
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
					
					#if 0 // _DEBUG
					int Depth = 0;
					for (GHtmlElement *ep = Elem; ep; ep=ep->Parent)
					{
						Depth++;
					}
					char Sp[256];
					Depth<<=1;
					memset(Sp, ' ', Depth);
					Sp[Depth] = 0;
					LgiTrace("%s%s (this=%p, parent=%p)\n", Sp, Elem->Tag, Elem, Elem->Parent);
					#endif
					
								
					bool AlreadyOpen = false;
					Elem->Info = GetTagInfo(Elem->Tag);
					if (Elem->Info)
					{
						if (Elem->Info->Flags & GHtmlElemInfo::TI_SINGLETON)
						{
							// Do singleton check... we don't want nested BODY or HEAD tags...
							List<GHtmlElement>::I it = OpenTags.End();
							for (GHtmlElement *e = *it; e; e = *--it)
							{
								if (e->TagId == TAG_IFRAME)
								{
									// In the case of IFRAMEs... don't consider the parent document.
									break;
								}

								if (e->Tag &&
									!_stricmp(e->Tag, Elem->Tag))
								{
									AlreadyOpen = true;
									
									#if 0 // This dumps the tags in the list
									it = OpenTags.Start();
									LgiTrace("Open tags:\n");
									for (GHtmlElement *e = *it; e; e = *++it)
									{
										GAutoString a = e->DescribeElement();
										LgiTrace("\t%s\n", a.Get());
									}
									#endif
									break;
								}
							}
						}
					
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
						if (Elem->TagId == TAG_META)
						{
							GAutoString Cs;

							const char *s;
							if (Elem->Get("http-equiv", s) &&
								_stricmp(s, "Content-Type") == 0)
							{
								const char *ContentType;
								if (Elem->Get("content", ContentType))
								{
									char *CharSet = stristr(ContentType, "charset=");
									if (CharSet)
									{
										char16 *cs = NULL;
										ParsePropValue(CharSet + 8, cs);
										Cs.Reset(LgiNewUtf16To8(cs));
										DeleteArray(cs);
									}
								}
							}

							if (Elem->Get("name", s) && _stricmp(s, "charset") == 0 && Elem->Get("content", s))
							{
								Cs.Reset(NewStr(s));
							}
							else if (Elem->Get("charset", s))
							{
								Cs.Reset(NewStr(s));
							}

							if (Cs)
							{
								if (Cs &&
									_stricmp(Cs, "utf-16") != 0 &&
									_stricmp(Cs, "utf-32") != 0 &&
									LgiGetCsInfo(Cs))
								{
									DocCharSet = Cs;
								}
							}
						}
					}

					if (IsBlock(Elem->Display()) || Elem->TagId == TAG_BR)
					{
						SkipNonDisplay(s);
					}

					Elem->SetStyle();

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
									View->GetEnv()->OnCompileScript(View, s, Lang.Str(), Type.Str());
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
						case TAG_STYLE:
						{
							char *End = stristr(s, "</style>");
							if (End)
							{
								if (View)
								{
									GAutoString Css(NewStr(s, End - s));
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
								return s + strlen(s);
							}
							break;
						}
					}
					
					if (AlreadyOpen ||
						TagClosed ||
						Elem->Info->NeverCloses())
					{
						return s;
					}

					#if FEATURE_REATTACH_ELEMENTS
					if (Elem->Info->Reattach)
					{
						GArray<int> ParentTags;
						switch (Elem->TagId)
						{
							case TAG_LI:
							{
								ParentTags.Add(TAG_OL);
								ParentTags.Add(TAG_UL);
								break;
							}
							case TAG_HEAD:
							{
								ParentTags.Add(TAG_HTML);
								break;
							}
							case TAG_TBODY:
							{
								ParentTags.Add(TAG_TABLE);
								break;
							}
							case TAG_TR:
							{
								ParentTags.Add(TAG_TBODY);
								ParentTags.Add(TAG_TABLE);
								break;
							}
							case TAG_TD:
							case TAG_TH:
							{
								ParentTags.Add(TAG_TR);
								break;
							}
							default:
								break;
						}

						for (GHtmlElement *p=OpenTags.Last(); p && p->TagId != TAG_TABLE; p=OpenTags.Prev())
						{
							if (p->TagId == Elem->TagId)
							{
								CloseTag(p);
								break;
							}
						}
												
						bool Reattach = !ParentTags.HasItem(Elem->Parent->TagId);						
						if (Reattach)
						{
							if (Elem->TagId == TAG_HEAD)
							{
								// Ignore it..
								return s;
							}
							else
							{
								GHtmlElement *Parent = NULL;
								for (GHtmlElement *t=OpenTags.Last(); t; t=OpenTags.Prev())
								{
									if (t->TagId && ParentTags.HasItem(t->TagId))
									{
										Parent = t;
										break;
									}

									if (t->TagId == TAG_TABLE)
										break;
								}

								if (Parent)
								{
									// Reattach to the right parent.
									// LgiTrace("Reattaching '%s'(%p) to '%s'(%p) count=%i\n", Elem->Tag, Elem, Parent->Tag, Parent, ++count);
									Parent->Attach(Elem);
								}
								else
								{
									// Maybe there is no parent tag?
									switch (Elem->TagId)
									{
										case TAG_TD:
										case TAG_TH:
										{
											// Find a TBODY or TABLE to attach a new ROW
											GHtmlElement *Attach = Elem->Parent;
											while (Attach)
											{
												if (Attach->TagId == TAG_TABLE || Attach->TagId == TAG_TBODY)
													break;
												Attach = Attach->Parent;
											}
											if (Attach)
											{
												// Create a new ROW
												GHtmlElement *NewRow = CreateElement(Attach);
												if (NewRow)
												{
													NewRow->Tag.Reset(NewStr("tr"));
													NewRow->TagId = TAG_TR;
													NewRow->Info = GetTagInfo(NewRow->Tag);
													
													bool IsAttach = Attach->Children.HasItem(NewRow);
													if (IsAttach)
													{
														OpenTags.Add(NewRow);
														NewRow->Attach(Elem);
														LgiTrace("Inserted new TAG_TR: %p\n", NewRow);
													}
												}
												else LgiAssert(!"Alloc error");
											}
											else LgiAssert(!"What now?");											
											break;
										}
										default:
										{
											LgiAssert(!"Impl me.");
											break;
										}
									}
								}
							}
						}
					}
					#endif

					OpenTags.Insert(Elem);
					
					if (Elem->TagId == TAG_IFRAME)
					{
						GVariant Src;
						if (Elem->GetValue("src", Src) && View && View->GetEnv())
						{
							GDocumentEnv::LoadJob *j = View->GetEnv()->NewJob();
							if (j)
							{
								j->Uri.Reset(Src.ReleaseStr());
								j->Env = View->GetEnv();
								j->UserData = this;
								j->UserUid = View ? View->GetDocumentUid() : 0;

								GDocumentEnv::LoadType Result = View->GetEnv()->GetContent(j);
								if (Result == GDocumentEnv::LoadImmediate)
								{
									GStreamI *s = j->GetStream();
									if (s)
									{
										uint64 Len = s->GetSize();
										if (Len > 0)
										{
											GAutoString a(new char[(size_t)Len+1]);
											int r = s->Read(a, (int)Len);
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
					}
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
							while (c->Children.Length())
							{
								GHtmlElement *Last = c->Children.Last();

								if (Last->TagId == CONTENT &&
									!ValidStrW(Last->Txt))
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
				while (*s == '/')
					s++;

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
					GAutoString Name(NewStr(s, e - s));
					GHtmlElement *Open = GetOpenTag(Name);					
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
				{
					// Error case happens with borked HTML
					s = EndBracket + 1;
				}

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
								!Elem->Txt)
							{
								Elem->Txt = Cur;
							}
							else if ((Child = CreateElement(Elem)))
							{
								Child->Txt = Cur;
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
								sprintf_s(css, sizeof(css), "x-rect: rect(%i,%i,%i,%i);", rc.y1, rc.x2, rc.y2, rc.x1);
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
		if (DocCharSet && View && View->GetCharset())
		{
			DocAndCsTheSame = _stricmp(DocCharSet, View->GetCharset()) == 0;
		}

		if (!DocAndCsTheSame &&
			DocCharSet &&
			View &&
			View->GetCharset() &&
			!View->GetOverideDocCharset())
		{
			const char *ViewCs = View->GetCharset();
			char *DocText = (char*)LgiNewConvertCp(DocCharSet, s, ViewCs, Len);
			t = (char16*) LgiNewConvertCp(LGI_WideCharset, DocText, DocCharSet, -1);
			DeleteArray(DocText);
		}
		else if (DocCharSet)
		{
			t = (char16*) LgiNewConvertCp(LGI_WideCharset, s, DocCharSet, Len);
		}
		else
		{
			const char *ViewCs = View ? View->GetCharset() : DefaultCs;
			t = (char16*) LgiNewConvertCp(LGI_WideCharset, s, ViewCs, Len);
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
							
							char16 Ch = 0;
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
									*p++ = (char)*i++;
								}
								*p = 0;
								Ch = htoi(n);
							}
							else
							{
								// Decimal number
								while (*i && IsDigit(*i) && (p - n) < 31)
								{
									*p++ = (char)*i++;
								}
								*p = 0;
								Ch = atoi(n);
							}
							
							if (Ch)
								*o++ = Ch;
							
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
		p.Print(", %s", t->Tag.Get());
		
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

bool GHtmlParser::ParseColour(const char *s, GCss::ColorDef &c)
{
	if (s)
	{
		int m;

		if (*s == '#')
		{
			s++;

			ParseHexColour:
			int i = htoi(s);
			int l = strlen(s);
			if (l == 3)
			{
				int r = i >> 8;
				int g = (i >> 4) & 0xf;
				int b = i & 0xf;

				c.Type = GCss::ColorRgb;
				c.Rgb32 = Rgb32(r | (r<<4), g | (g << 4), b | (b << 4));
			}
			else if (l == 4)
			{
				int r = (i >> 12) & 0xf;
				int g = (i >> 8) & 0xf;
				int b = (i >> 4) & 0xf;
				int a = i & 0xf;
				c.Type = GCss::ColorRgb;
				c.Rgb32 = Rgba32(	r | (r <<4 ),
									g | (g << 4),
									b | (b << 4),
									a | (a << 4));
			}
			else if (l == 6)
			{
				c.Type = GCss::ColorRgb;
				c.Rgb32 = Rgb32(i >> 16, (i >> 8) & 0xff, i & 0xff);
			}
			else if (l == 8)
			{
				c.Type = GCss::ColorRgb;
				c.Rgb32 = Rgba32(i >> 24, (i >> 16) & 0xff, (i >> 8) & 0xff, i & 0xff);
			}
			else
			{
				return false;
			}
			
			return true;
		}
		else if ((m = GHtmlStatic::Inst->ColourMap.Find(s)) >= 0)
		{
			c.Type = GCss::ColorRgb;
			c.Rgb32 = Rgb24To32(m);
			return true;
		}
		else if (!_strnicmp(s, "rgb", 3))
		{
			s += 3;
			SkipWhiteSpace(s);
			if (*s == '(')
			{
				s++;
				GArray<uint8> Col;
				while (Col.Length() < 3)
				{
					SkipWhiteSpace(s);
					if (IsDigit(*s))
					{
						Col.Add(atoi(s));
						while (*s && IsDigit(*s)) s++;
						SkipWhiteSpace(s);
						if (*s == ',') s++;
					}
					else break;
				}

				SkipWhiteSpace(s);
				if (*s == ')' && Col.Length() == 3)
				{
					c.Type = GCss::ColorRgb;
					c.Rgb32 = Rgb32(Col[0], Col[1], Col[2]);
					return true;
				}
			}
		}
		else if (IsDigit(*s) || (tolower(*s) >= 'a' && tolower(*s) <= 'f'))
		{
			goto ParseHexColour;
		}
	}

	return false;
}

bool GHtmlParser::Is8Bit(char *s)
{
	while (*s)
	{
		if (((uchar)*s) & 0x80)
			return true;
		s++;
	}
	return false;
}
