/*
**	FILE:			LgiRes_String.cpp
**	AUTHOR:			Matthew Allen
**	DATE:			10/9/1999
**	DESCRIPTION:	String Resource Editor
**
**
**	Copyright (C) 1999, Matthew Allen
**		fret@memecode.com
*/

////////////////////////////////////////////////////////////////////
#include <stdlib.h>
#include "LgiResEdit.h"
#include "LgiRes_String.h"
#include "GToken.h"
#include "GVariant.h"
#include "GTextLabel.h"
#include "GCombo.h"
#include "GButton.h"
#include "GClipBoard.h"
#include "resdefs.h"

////////////////////////////////////////////////////////////////////////////
LangDlg::LangDlg(GView *parent, List<GLanguage> &l, int Init)
{
	Lang = 0;
	SetParent((parent) ? parent : MainWnd);
	GRect r(0, 0, 260, 90);
	SetPos(r);
	MoveToCenter();
	Name("Language");

	Children.Insert(new GTextLabel(-1, 10, 10, -1, -1, "Select language:"));
	Children.Insert(Sel = new GCombo(-1, 20, 30, 150, 20, ""));
	if (Sel)
	{
		if (l.Length() > 40)
		{
			Sel->Sub(GV_STRING);
		}

		for (GLanguage *li = l.First(); li; li = l.Next())
		{
			Langs.Insert(li);
			Sel->Insert(li->Name);
		}

		if (Init >= 0)
		{
			Sel->Value(Init);
		}
	}

	Children.Insert(new GButton(IDOK, 180, 5, 60, 20, "Ok"));
	Children.Insert(new GButton(IDCANCEL, 180, 30, 60, 20, "Cancel"));
}

int LangDlg::OnNotify(GViewI *Ctrl, int Flags)
{
	switch (Ctrl->GetId())
	{
		case IDOK:
		{
			Lang = Langs.ItemAt(Sel->Value());
			EndModal(1);
			break;
		}
		case IDCANCEL:
		{
			EndModal(0);
			break;
		}
	}

	return 0;
}	

////////////////////////////////////////////////////////////////////
StrLang::StrLang()
{
	Lang = "en";
	Str = 0;
}

StrLang::~StrLang()
{
	DeleteArray(Str);
}

GLanguageId StrLang::GetLang()
{
	return Lang;
}

void StrLang::SetLang(GLanguageId i)
{
	LgiAssert(i);
	Lang = i;
}

char *&StrLang::GetStr()
{
	return Str;
}

void StrLang::SetStr(const char *s)
{
	char *n = NewStr(s);
	DeleteArray(Str);
	Str = n;
}

bool StrLang::IsEnglish()
{
	return stricmp(Lang, "en") == 0;
}

bool StrLang::operator ==(GLanguageId LangId)
{
	return stricmp(Lang, LangId) == 0;
}

bool StrLang::operator !=(GLanguageId LangId)
{
	return !(*this == LangId);
}

////////////////////////////////////////////////////////////////////
ResString::ResString(ResStringGroup *grp, int init_ref)
{
	Ref = init_ref;
	Group = grp;
	Id = 0;
	Tag = 0;
	UpdateWnd = 0;
	IdStr[0] = 0;
	RefStr[0] = 0;

	if (Group)
	{
		LgiAssert(!Group->Strs.HasItem(this));

		Group->Strs.Insert(this);
		Group->LList::Insert(this);
	}
	else
	{
		LgiAssert(0);
	}

	// LgiStackTrace("%p::ResString\n", this);
}

ResString::~ResString()
{
	if (Group)
	{
		Group->App()->OnObjDelete(this);

		if (!Group->Strs.Delete(this))
		{
			LgiAssert(0);
		}

		Group->LList::Remove(this);
	}

	for (StrLang *s = Items.First(); s; s = Items.Next())
	{
		DeleteObj(s);
	}

	DeleteArray(Tag);
}

int ResString::SetRef(int r)
{
	LgiAssert(r != 0 && r != -1);

	if (r != Ref)
	{
		Ref = r;

		Update();
		if (UpdateWnd)
		{
			UpdateWnd->Invalidate();
		}
	}	

	return Ref;
}

int ResString::SetId(int id)
{
	LgiAssert(id != 0);
	if (Id != id)
	{
		Id = id;

		Update();
		if (UpdateWnd)
		{
			UpdateWnd->Invalidate();
		}
	}
	return Id;
}

void ResString::SetDefine(const char *s)
{
	Define.Reset(NewStr(s));
}

ResString &ResString::operator =(ResString &s)
{
	memcpy(RefStr, s.RefStr, sizeof(RefStr));
	memcpy(IdStr, s.IdStr, sizeof(IdStr));
	
	SetRef(s.GetRef());
	SetId(s.GetId());
	
	SetDefine(s.GetDefine());
	DeleteObj(Tag);
	Tag = NewStr(s.Tag);
	
	for (StrLang *l = s.Items.First(); l; l = s.Items.Next())
	{
		Set(l->GetStr(), l->GetLang());
	}

	return *this;
}

char *ResString::Get(GLanguageId Lang)
{
	if (!Lang)
	{
		GLanguage *L = Group->App()->GetCurLang();
		if (L)
		{
			Lang = L->Id;
		}
		else
		{
			LgiAssert(0);
			return 0;
		}
	}

	StrLang *s = GetLang(Lang);
	if (s)
	{
		return s->GetStr();
	}

	return 0;
}

void ResString::Set(const char *p, GLanguageId Lang)
{
	if (!Lang)
	{
		GLanguage *L = Group->App()->GetCurLang();
		if (L)
		{
			Lang = L->Id;
		}
		else
		{
			LgiAssert(0);
			return;
		}
	}

	StrLang *s = GetLang(Lang);
	if (!s)
	{
		Items.Insert(s = new StrLang);
		if (s)
		{
			s->SetLang(Lang);
		}
	}
	if (s)
	{
		s->SetStr(p);

		Update();
		if (UpdateWnd)
		{
			UpdateWnd->Invalidate();
		}
	}
	else
	{
		LgiAssert(0);
	}
}

void ResString::UnDuplicate()
{
	// reset Ref if it's a duplicate
	int OldStrRef = Ref;
	Ref = 0x80000000;
	ResString *Dupe = Group->App()->GetStrFromRef(OldStrRef);
	// now we return the Id to the string
	
	SetRef(OldStrRef);

	if (Dupe ||	Ref < 0)
	{
		// there is a duplicate string Id
		// so assign a new Id
		SetRef(Group->App()->GetUniqueStrRef());
	}				
}

char **ResString::GetIndex(int i)
{
	StrLang *s = Items.ItemAt(i);
	return (s) ? &s->GetStr() : 0;
}


bool ResString::Test(ErrorCollection *e)
{
	bool Status = true;

	if (ValidStr(Define) && Id == 0)
	{
		ErrorInfo *Err = &e->StrErr.New();
		Err->Str = this;
		Err->Msg.Reset(NewStr("No ctrl id."));
		Status = false;
	}

	if (Ref == 0)
	{
		ErrorInfo *Err = &e->StrErr.New();
		Err->Str = this;
		Err->Msg.Reset(NewStr("No resource ref."));
		Status = false;
	}

	#if 0
	if (!ValidStr(Define))
	{
		ErrorInfo *Err = &e->StrErr.New();
		Err->Str = this;
		Err->Msg.Reset(NewStr("No define name."));
		Status = false;
	}
	#endif

	for (StrLang *s = Items.First(); s; s = Items.Next())
	{
		if (!LgiIsUtf8(s->GetStr()))
		{
			ErrorInfo *Err = &e->StrErr.New();
			Err->Str = this;
			char m[256];
			sprintf(m, "Utf-8 error in translation '%s'", s->GetLang());
			Err->Msg.Reset(NewStr(m));
			Status = false;
		}
	}

	return Status;
}

bool ResString::Read(GXmlTag *t, SerialiseContext &Ctx)
{
	if (!t || !t->IsTag("string"))
	{
		Ctx.Log.Print("%s:%i - Not a string tag (Tag='%s').\n", _FL, t->GetTag());
		return false;
	}

	char *n = 0;

	int Ref = t->GetAsInt("Ref");
	if (!Ref)
	{
		Ctx.Log.Print("%s:%i - String has no Ref number.\n", _FL);
		return false;
	}
	
	SetRef(Ref);
	if ((n = t->GetAttr("Define")) &&
		ValidStr(n))
	{
		SetDefine(n);

		char *c;
		if ((c = t->GetAttr("Cid")) ||
			(c = t->GetAttr("Id")))
		{
			int Cid = atoi(c);
			if (!Cid)
			{
				Ctx.Log.Print("%s:%i - String missing id (ref=%i)\n", _FL, Ref);
				Ctx.FixId.Add(this);
			}
			else
			{
				SetId(Cid);
			}
		}
	}
	if ((n = t->GetAttr("Tag")))
	{
		DeleteArray(Tag);
		Tag = NewStr(n);
	}

	for (int i=0; i<t->Attr.Length(); i++)
	{
		GXmlAttr *v = &t->Attr[i];
		if (v->GetName())
		{
			GLanguage *Lang = GFindLang(v->GetName());
			if (!Lang &&
				v->GetName()[0] == 'T' &&
				v->GetName()[1] == 'e' &&
				v->GetName()[2] == 'x' &&
				v->GetName()[3] == 't' &&
				v->GetName()[4] == '(')
			{
				Lang = GFindOldLang(atoi(v->GetName()+5));
				LgiAssert(Lang); // This really should be a valid string
			}

			if (Lang)
			{
				if (Ctx.Format == Lr8File)
				{
					LgiAssert(LgiIsUtf8(v->GetName()));
					Set(v->GetValue(), Lang->Id);
				}
				else if (Ctx.Format == CodepageFile)
				{
					char *Utf8 = (char*)LgiNewConvertCp("utf-8", v->GetValue(), Lang->CodePage);
					Set(Utf8 ? Utf8 : v->GetValue(), Lang->Id);
					if (Utf8)
					{
						DeleteArray(Utf8);
					}
				}
				else if (Ctx.Format == XmlFile)
				{
					char *x = DecodeXml(v->GetValue());
					if (x)
					{
						Set(x, Lang->Id);
						DeleteArray(x);
					}
				}
				else
					LgiAssert(0);
			}
		}
	}

	return true;
}

bool ResString::Write(GXmlTag *t, SerialiseContext &Ctx)
{
	t->SetTag("String");
	t->SetAttr("Ref", Ref);
	if (ValidStr(Define))
	{
		t->SetAttr("Cid", Id);
		t->SetAttr("Define", Define);
	}
	if (Tag) t->SetAttr("Tag", Tag);

	StrLang *s;
	char *English = 0;
	for (s = Items.First(); s; s = Items.Next())
	{
		if (s->IsEnglish())
		{
			English = s->GetStr();
		}
	}

	for (s = Items.First(); s; s = Items.Next())
	{
		if (ValidStr(s->GetStr()))
		{
			char *Mem = 0;
			char *String = 0;

			if (Ctx.Format == Lr8File)
			{
				// Don't save the string if it's the same as the English
				if (English && !s->IsEnglish())
				{
					if (strcmp(English, s->GetStr()) == 0)
					{
						continue;
					}
				}

				// Save this string
				String = s->GetStr();
			}
			else if (Ctx.Format == CodepageFile)
			{
				GLanguage *Li = GFindLang(s->GetLang());
				if (Li)
				{
					Mem = String = (char*)LgiNewConvertCp(Li->CodePage, s->GetStr(), "utf-8");
				}

				if (!String) String = s->GetStr();
			}
			else if (Ctx.Format == XmlFile)
			{
				// Don't save the string if it's the same as the English
				if (English && !s->IsEnglish())
				{
					if (strcmp(English, s->GetStr()) == 0)
					{
						continue;
					}
				}

				// Save this string
				String = Mem = EncodeXml(s->GetStr());
			}
			else
				LgiAssert(0);

			if (ValidStr(String))
			{
				t->SetAttr(s->GetLang(), String);
			}

			DeleteArray(Mem);
		}
	}

	return true;
}

StrLang *ResString::GetLang(GLanguageId i)
{
	for (StrLang *s = Items.First(); s; s = Items.Next())
	{
		if (stricmp(s->GetLang(), i) == 0)
		{
			return s;
		}
	}

	return 0;
}

bool ResString::GetFields(FieldTree &Fields)
{
	// string view [literal]
	Fields.Insert(this, DATA_INT, 199, VAL_Ref, "Ref");
	Fields.Insert(this, DATA_INT, 200, VAL_Id, "Id");

	// else dialog view [embeded]
	Fields.Insert(this, DATA_STR, 201, VAL_Define, "#define");
	Fields.Insert(this, DATA_STR, 198, VAL_Tag, "Tag");

	if (Group)
	{
		for (int i=0; i<Group->GetLanguages(); i++)
		{
			if (Group->App()->ShowLang(Group->Lang[i]->Id))
			{
				Fields.Insert(this, DATA_STR, 202+i, Group->Lang[i]->Name, Group->Lang[i]->Name);
			}
		}
	}

	return true;
}

bool ResString::Serialize(FieldTree &Fields)
{
	Fields.Serialize(this, VAL_Ref, Ref);
	Fields.Serialize(this, VAL_Id, Id);
	Fields.Serialize(this, VAL_Define, Define);
	Fields.Serialize(this, VAL_Tag, Tag);

	if (Group)
	{
		for (int i=0; i<Group->GetLanguages(); i++)
		{
			GLanguageId Id = Group->Lang[i]->Id;
			StrLang *s = GetLang(Id);
			if (Fields.GetMode() == FieldTree::UiToObj && !s)
			{
				s = new StrLang;
				if (s)
				{
					Items.Insert(s);
					s->SetLang(Id);
				}
			}
			if (s)
			{
				Fields.Serialize(this, Group->Lang[i]->Name, s->GetStr());
			}
		}

		Update();
	}

	return true;
}

int ResString::NewId()
{
	List<ResString> sl;

	if (Group && Group->AppWindow)
	{
		Group->AppWindow->FindStrings(sl, Define);
		int NewId = Group->AppWindow->GetUniqueCtrlId();
		for (ResString *s = sl.First(); s; s = sl.Next())
		{
			s->SetId(NewId);
			s->Update();
		}
	}
	else LgiAssert(!"Invalid ptrs.");

	return Id;
}

int ResString::GetCols()
{
	return 3 + Group->GetLanguages();
}

char *ResString::GetText(int i)
{
	switch (i)
	{
		case 0:
		{
			return (Define)?Define:(char*)"";
		}
		case 1:
		{
			sprintf(RefStr, "%i", Ref);
			return RefStr;
		}
		case 2:
		{
			sprintf(IdStr, "%i", Id);
			return IdStr;
		}
		default:
		{
			int LangIdx = i - 3;
			if (LangIdx < Group->Visible.Length())
			{
				GLanguage *Info = Group->Visible[LangIdx];
				if (Info)
				{
					for (StrLang *s = Items.First(); s; s = Items.Next())
					{
						if (*s == Info->Id)
						{
							return (s->GetStr()) ? s->GetStr() : (char*)"";
						}
					}
				}
			}
			break;
		}
	}

	return (char*)"";
}

void ResString::OnMouseClick(GMouse &m)
{
	if (m.IsContextMenu())
	{
		GSubMenu *RClick = new GSubMenu;
		if (RClick)
		{
			bool PasteTranslations = false;
			char *Clip;

			{
				GClipBoard c(Parent);
				Clip = c.Text();
				if (Clip)
					PasteTranslations = strstr(Clip, TranslationStrMagic);
			}

			RClick->AppendItem("Copy Text", IDM_COPY_TEXT, true);
			RClick->AppendItem("Paste Text", IDM_PASTE_TEXT, PasteTranslations);
			RClick->AppendSeparator();
			RClick->AppendItem("New Id", IDM_NEW_ID, true);
			RClick->AppendItem("Make Ref=Id", IDM_REF_EQ_ID, true);
			RClick->AppendItem("Make Id=Ref", IDM_ID_EQ_REF, true);

			if (Parent->GetMouse(m, true))
			{
				int Cmd = 0;
				switch (Cmd = RClick->Float(Parent, m.x, m.y))
				{
					case IDM_COPY_TEXT:
					{
						CopyText();
						break;
					}
					case IDM_PASTE_TEXT:
					{
						PasteText();
						break;
					}
					case IDM_NEW_ID:
					{
						List<ResString> Sel;
						if (!GetList()->GetSelection(Sel))
							break;

						auto App = Group->AppWindow;
						for (auto s : Sel)
						{
							List<ResString> sl;
							App->FindStrings(sl, s->Define);
								
							int NewId = App->GetUniqueCtrlId();
							for (auto i : sl)
							{
								i->SetId(NewId);
							}
						}
						break;
					}
					case IDM_REF_EQ_ID:
					{
						List<ResString> a;
						if (GetList()->GetSelection(a))
						{
							bool Dirty = false;
							for (ResString *s = a.First(); s; s = a.Next())
							{
								if (s->GetId() != s->GetRef())
								{
									ResString *Existing = Group->App()->GetStrFromRef(s->GetId());
									if (!Existing)
									{
										s->SetRef(s->GetId());
										Dirty = true;
									}
								}
							}
							
							if (Dirty)
							{
								Group->App()->SetDirty();
							}
						}
						break;
					}
					case IDM_ID_EQ_REF:
					{
						List<ResString> a;
						if (GetList()->GetSelection(a))
						{
							bool Dirty = false;
							for (ResString *s = a.First(); s; s = a.Next())
							{
								if (s->GetId() != s->GetRef())
								{
									List<ResString> Existing;
									int CtrlId = s->GetRef();
									Group->App()->FindStrings(Existing, 0, &CtrlId);
									if (Existing.Length() == 0)
									{
										s->SetId(s->GetRef());
										Dirty = true;
									}
								}
							}
							
							if (Dirty)
							{
								Group->App()->SetDirty();
							}
						}
						break;
					}
				}
			}

			DeleteObj(RClick);
		}
	}
}

void ResString::CopyText()
{
	if (Items.Length() > 0)
	{
		GStringPipe p;

		p.Push(TranslationStrMagic);
		p.Push(EOL_SEQUENCE);

		for (StrLang *s=Items.First(); s; s=Items.Next())
		{
			char Str[256];
			sprintf(Str, "%s,%s", s->GetLang(), s->GetStr());
			p.Push(Str);
			p.Push(EOL_SEQUENCE);
		}

		char *All = p.NewStr();
		if (All)
		{
			GClipBoard Clip(Parent);

			Clip.Text(All);

			char16 *w = Utf8ToWide(All);
			Clip.TextW(w, false);
			DeleteArray(w);

			DeleteArray(All);
		}
	}
	else
	{
		LgiMsg(Parent, "No translations to copy.", "ResDialogCtrl::CopyText", MB_OK);
	}
}

void ResString::PasteText()
{
	GClipBoard c(Parent);
	
	char *Clip = 0;
	char16 *w = c.TextW();
	if (w)
	{
		Clip = WideToUtf8(w);
	}
	else
	{
		Clip = c.Text();
	}

	if (Clip)
	{
		GToken Lines(Clip, "\r\n");
		if (Lines.Length() > 0 &&
			strcmp(Lines[0], TranslationStrMagic) == 0)
		{
			Items.DeleteObjects();

			for (int i=1; i<Lines.Length(); i++)
			{
				char *Translation = strchr(Lines[i], ',');
				if (Translation)
				{
					*Translation++ = 0;

					GLanguage *l = GFindLang(Lines[i]);
					if (l)
					{
						Set(Translation, l->Id);
					}
				}
			}

			if (Group && Group->App())
			{
				Group->App()->OnObjSelect(this);
			}
		}
	}

	if (w)
	{
		DeleteArray(Clip);
	}
}

////////////////////////////////////////////////////////////////////
ResStringGroup::ResStringGroup(AppWnd *w, int type) : 
	Resource(w, type),
	LList(90, 0, 0, 200, 200)
{
	Ui = 0;
	Wnd()->Name("<untitled>");

	Sunken(false);
	SortAscend = true;
	SortCol = 0;
	AppendLanguage("en");
	App()->OnLanguagesChange("en", true);
}

ResStringGroup::~ResStringGroup()
{
	ResString *s;
	while ((s = Strs.First()))
	{
		DeleteObj(s);
	}

	DeleteObj(Ui);
}

GLanguage *ResStringGroup::GetLanguage(GLanguageId Id)
{
	for (int i=0; i<Lang.Length(); i++)
	{
		if (Id == Lang[i]->Id)
			return Lang[i];
	}
	return 0;
}

int ResStringGroup::GetLangIdx(GLanguageId Id)
{
	for (int i=0; i<Lang.Length(); i++)
	{
		if (stricmp(Id, Lang[i]->Id) == 0)
			return i;
	}
	return -1;
}

int ResString::Compare(LListItem *li, int Column)
{
	ResString *r = dynamic_cast<ResString*>(li);
	static const char *Empty = "";
	bool Mod = (Column < 0) ? -1 : 1;
	Column = abs(Column) - 1;

	if (r)
	{
		switch (Column)
		{
			case 0:
			{
				return stricmp(Define?Define:Empty, r->Define?r->Define:Empty) * Mod;
				break;
			}
			case 1:
			{
				return (Ref - r->Ref) * Mod;
				break;
			}
			case 2:
			{
				return (Id - r->Id) * Mod;
				break;
			}
			default:
			{
				int Col = Column - 3;
				if (Group && Col >= 0 && Col < Group->GetLanguages())
				{
					GLanguageId Lang = Group->Visible[Column - 3]->Id;
					char *a = Get(Lang);
					char *b = r->Get(Lang);
					const char *Empty = "";
					return stricmp(a?a:Empty, b?b:Empty) * Mod;
				}
				break;
			}
		}
	}

	return -1;
}

int ResStringCompareFunc(LListItem *a, LListItem *b, NativeInt Data)
{
	ResString *A = dynamic_cast<ResString*>(a);
	if (A)
	{
		return A->Compare(dynamic_cast<ResString*>(b), Data);
	}
	return -1;
}

void ResStringGroup::OnColumnClick(int Col, GMouse &m)
{
	if (SortCol == Col)
	{
		SortAscend = !SortAscend;
	}
	else
	{
		SortCol = Col;
		SortAscend = true;
	}

	LList::Sort<NativeInt>(ResStringCompareFunc, (SortCol + 1) * ((SortAscend) ? 1 : -1));

	LListItem *Sel = GetSelected();
	if (Sel)
	{
		Sel->ScrollTo();
	}
}

void ResStringGroup::OnItemClick(LListItem *Item, GMouse &m)
{
	LList::OnItemClick(Item, m);
}

void ResStringGroup::OnItemSelect(GArray<LListItem*> &Items)
{
	if (IsAttached())
	{
		ResString *s = dynamic_cast<ResString*>(Items[0]);
		if (s && AppWindow)
		{
			AppWindow->OnObjSelect(s);
		}
	}
}

void ResStringGroup::OnShowLanguages()
{
	UpdateColumns();
	UpdateAllItems();
}

void ResStringGroup::UpdateColumns()
{
	EmptyColumns();
	AddColumn("#define", 150);
	AddColumn("Ref", 70);
	AddColumn("Id", 70);

	Visible.Length(0);
	for (int i=0; i<GetLanguages(); i++)
	{
		if (App()->ShowLang(Lang[i]->Id))
		{
			Visible.Add(Lang[i]);
			AddColumn(Lang[i]->Name, 100);
		}
	}
}

void ResStringGroup::DeleteLanguage(GLanguageId Id)
{
	// Check for presence
	int Index = -1;
	for (int i=0; i<GetLanguages(); i++)
	{
		if (Lang[i]->Id == Id)
		{
			Index = i;
			break;
		}
	}

	if (Index >= 0)
	{
		// Remove lang
		Lang.DeleteAt(Index);
		UpdateColumns();
	}
}

void ResStringGroup::AppendLanguage(GLanguageId Id)
{
	// Check for presence
	int Index = -1;
	for (int i=0; i<Lang.Length(); i++)
	{
		if (Lang[i]->Id == Id)
		{
			Index = i;
			break;
		}
	}

	if (Index < 0)
	{
		GLanguage *n = GFindLang(Id);
		if (n)
		{
			Lang[Lang.Length()] = n;
			UpdateColumns();
		}
	}
}

int RefCmp(ResString *a, ResString *b, NativeInt d)
{
	return a->GetRef() - b->GetRef();
}

void ResStringGroup::Sort()
{
	Strs.Sort(RefCmp);
}

void ResStringGroup::RemoveUnReferenced()
{
	for (ResString *s = Strs.First(); s; )
	{
		if (!s->UpdateWnd)
		{
			DeleteStr(s);
			s = Strs.Current();
		}
		else
		{
			s = Strs.Next();
		}
	}
}

int ResStringGroup::OnCommand(int Cmd, int Event, OsView hWnd)
{
	switch (Cmd)
	{
		case IDM_NEW:
		{
			CreateStr(true);
			break;
		}
		case IDM_DELETE:
		{
			List<LListItem> l;
			if (GetSelection(l))
			{
				for (	ResString *s = dynamic_cast<ResString*>(l.First());
						s;
						s = dynamic_cast<ResString*>(l.Next()))
				{
					DeleteObj(s);
				}
			}
			break;
		}
		case IDM_NEW_LANG:
		{
			// List all the languages minus the ones already
			// being edited
			List<GLanguage> l;
			for (GLanguage *li = GFindLang(NULL); li->Id; li++)
			{
				bool Present = false;
				for (int i=0; i<Lang.Length(); i++)
				{
					if (Lang[i]->Id == li->Id)
					{
						Present = true;
						break;
					}
				}

				if (!Present)
				{
					l.Insert(li);
				}
			}

			// Display the list
			LangDlg Dlg(this, l);
			if (Dlg.DoModal())
			{
				if (Dlg.Lang)
				{
					AppendLanguage(Dlg.Lang->Id);

					// Update the global language list
					AppWindow->ShowLang(Dlg.Lang->Id, true);
				}
			}
			break;
		}
		case IDM_DELETE_LANG:
		{
			// List all the languages being edited
			List<GLanguage> l;
			for (int i=0; i<Lang.Length(); i++)
			{
				l.Insert(Lang[i]);
			}

			// Display the list
			LangDlg Dlg(this, l);
			if (Dlg.DoModal() && Dlg.Lang)
			{
				if (Dlg.Lang)
				{
					DeleteLanguage(Dlg.Lang->Id);
					// CurrentLang = limit(CurrentLang, 0, Lang.Length()-1);
				}
			}
			break;
		}
	}
	return 0;
}

ResString *ResStringGroup::CreateStr(bool User)
{
    int NextRef = App()->GetUniqueStrRef();
	ResString *s = new ResString(this, NextRef);
	if (s)
	{
		s->SetId(s->SetRef(NextRef));

		if (User)
		{
			char Str[256];
			sprintf(Str, "IDS_%i", s->Ref);
			s->SetDefine(Str);

			s->GetList()->Select(NULL);
			s->ScrollTo();
			s->Select(true);
		}
	}
	return s;
}

void ResStringGroup::DeleteStr(ResString *Str)
{
	DeleteObj(Str);
}

ResString *ResStringGroup::FindName(char *Name)
{
	if (Name)
	{
		for (	ResString *s = Strs.First();
				s;
				s = Strs.Next())
		{
			if (stricmp(s->Define, Name) == 0)
			{
				return s;
			}
		}
	}
	return NULL;
}

ResString *ResStringGroup::FindRef(int Ref)
{
	for (	ResString *s = Strs.First();
			s;
			s = Strs.Next())
	{
		if (s->GetRef() == Ref)
		{
			return s;
		}
	}

	return NULL;
}

int ResStringGroup::UniqueRef()
{
	int n = 1;
	for (ResString *i = dynamic_cast<ResString*>(Items.First()); i; i = dynamic_cast<ResString*>(Items.Next()))
	{
		n = MAX(n, i->Ref);
	}

	return n + 1;
}

int ResStringGroup::UniqueId(char *Define)
{
	int n = 1;

	for (ResString *i = dynamic_cast<ResString*>(Items.First()); i;
					i = dynamic_cast<ResString*>(Items.Next()))
	{
		if (i->Id &&
			i->Define &&
			Define &&
			stricmp(Define, i->Define) == 0)
		{
			return i->Id;
		}

		n = MAX(n, i->Id);
	}

	return n + 1;
}

int ResStringGroup::FindId(int Id, List<ResString*> &Strs)
{
	return 0;
}

void ResStringGroup::SetLanguages()
{
	List<GLanguage> l;
	bool EnglishFound = false;

	for (	ResString *s = Strs.First();
			s;
			s = Strs.Next())
	{
		for (StrLang *sl = s->Items.First(); sl; sl = s->Items.Next())
		{
			GLanguage *li = 0;
			for (li = l.First(); li && *sl != li->Id; li = l.Next());
			if (!li)
			{
				GLanguage *NewLang = GFindLang(sl->GetLang());
				if (NewLang) l.Insert(NewLang);
				
				if (*sl == (GLanguageId)"en")
					EnglishFound = true;
			}
		}
	}

	// CurrentLang = 0;
	// LgiAssert(CurrentLang < Lang.Length());

	Lang.Length(l.Length() + ((EnglishFound) ? 0 : 1));

	int n = 0;
	Lang[n++] = GFindLang("en");
	for (GLanguage *li = l.First(); li; li = l.Next())
	{
		if (stricmp(li->Id, "en") != 0) // !English
		{
			Lang[n] = GFindLang(li->Id);
			LgiAssert(Lang[n]);
			n++;
		}
	}

	UpdateColumns();
}

void ResStringGroup::DeleteSel()
{
}

void ResStringGroup::Copy(bool Delete)
{
}

void ResStringGroup::Paste()
{
}

GView *ResStringGroup::CreateUI()
{
	return Ui = new ResStringUi(this);
}

bool ResStringGroup::Test(ErrorCollection *e)
{
	bool Status = true;

	for (ResString *s = Strs.First(); s; s = Strs.Next())
	{
		if (!s->Test(e))
		{
			Status = false;
		}
	}

	return Status;
}

bool ResStringGroup::Read(GXmlTag *t, SerialiseContext &Ctx)
{
	bool Status = false;

	char *p = 0;
	if ((p = t->GetAttr("Name")))
	{
		if (Ctx.Format == XmlFile)
		{
			char *x = DecodeXml(p);
			if (x)
			{
				Name(x);
				DeleteArray(x);
			}
		}
		else
		{
			Name(p);
		}
		
		Empty();

		GHashTbl<const char*, bool> L;
		L.Add("en", true);
		Status = true;
		for (GXmlTag *c = t->Children.First(); c; c = t->Children.Next())
		{
			ResString *s = new ResString(this);
			if (s && s->Read(c, Ctx))
			{
				for (StrLang *i=s->Items.First(); i; i=s->Items.Next())
				{
					if (!L.Find(i->GetLang()))
					{
						L.Add(i->GetLang(), true);
						AppendLanguage(i->GetLang());
					}
				}
			}
			else
			{
				// Lets not fail everything here... just keep trying to read
				DeleteObj(s);
			}
		}

		ResString *te = FindRef(441);
		int asd=0;
	}

	if (Item)
	{
		Item->Update();
	}

	return Status;
}

bool ResStringGroup::Write(GXmlTag *t, SerialiseContext &Ctx)
{
	bool Status = true;

	t->SetTag("string-group");
	char *n = Ctx.Format == XmlFile ? EncodeXml(Name()) : NewStr(Name());
	t->SetAttr("Name", n);
	DeleteArray(n);

	for (LListItem *i = Strs.First(); i; i = Strs.Next())
	{
		ResString *s = dynamic_cast<ResString*>(i);
		if (s && (s->Define || s->Items.Length() > 0))
		{
			GXmlTag *c = new GXmlTag;
			if (c && s->Write(c, Ctx))
			{
				t->InsertTag(c);
			}
			else
			{
				Status = false;
				DeleteObj(c);
			}
		}
	}

	return Status;
}

void ResStringGroup::OnRightClick(GSubMenu *RClick)
{
	if (RClick)
	{
		if (Enabled())
		{
			RClick->AppendSeparator();
			if (Type() > 0)
			{
				GSubMenu *Export = RClick->AppendSub("Export to...");
				if (Export)
				{
					Export->AppendItem("Lgi File", IDM_EXPORT, true);
					Export->AppendItem("Win32 Resource Script", IDM_EXPORT_WIN32, false);
				}
			}
		}
	}
}

void ResStringGroup::OnCommand(int Cmd)
{
	switch (Cmd)
	{
		case IDM_IMPORT:
		{
			GFileSelect Select;
			Select.Parent(AppWindow);
			Select.Type("Text", "*.txt");
			if (Select.Open())
			{
				GFile F;
				if (F.Open(Select.Name(), O_READ))
				{
					SerialiseContext Ctx;
					Resource *Res = AppWindow->NewObject(Ctx, 0, -Type());
					if (Res)
					{
						// TODO
						// Res->Read();
					}
				}
				else
				{
					LgiMsg(AppWindow, "Couldn't open file for reading.");
				}
			}
			break;
		}
		case IDM_EXPORT:
		{
			GFileSelect Select;
			Select.Parent(AppWindow);
			Select.Type("Text", "*.txt");
			if (Select.Save())
			{
				GFile F;
				if (F.Open(Select.Name(), O_WRITE))
				{
					F.SetSize(0);
					// Serialize(F, true);
				}
				else
				{
					LgiMsg(AppWindow, "Couldn't open file for writing.");
				}
			}
			break;
		}
	}
}

void ResStringGroup::Create(GXmlTag *load, SerialiseContext *ctx)
{
	if (load)
	{
		if (ctx)
			Read(load, *ctx);
		else
			LgiAssert(0);
	}
}

////////////////////////////////////////////////////////////////////
ResStringUi::ResStringUi(ResStringGroup *Res)
{
	StringGrp = Res;
	Tools = 0;
	Status = 0;
	StatusInfo = 0;
}

ResStringUi::~ResStringUi()
{
	if (StringGrp)
	{
		StringGrp->Ui = 0;
	}
}

void ResStringUi::OnPaint(GSurface *pDC)
{
	GRegion Client(0, 0, X()-1, Y()-1);
	for (GViewI *w = Children.First(); w; w = Children.Next())
	{
		GRect r = w->GetPos();
		Client.Subtract(&r);
	}

	pDC->Colour(LC_MED, 24);
	for (GRect *r = Client.First(); r; r = Client.Next())
	{
		pDC->Rectangle(r);
	}
}

void ResStringUi::PourAll()
{
	GRegion Client(GetClient());
	GRegion Update;

	for (GViewI *v = Children.First(); v; v = Children.Next())
	{
		GRect OldPos = v->GetPos();
		Update.Union(&OldPos);

		if (v->Pour(Client))
		{
			if (!v->Visible())
			{
				v->Visible(true);
			}

			if (OldPos != v->GetPos())
			{
				// position has changed update...
				v->Invalidate();
			}

			Client.Subtract(&v->GetPos());
			Update.Subtract(&v->GetPos());
		}
		else
		{
			// make the view not visible
			v->Visible(false);
		}
	}

	for (int i=0; i<Update.Length(); i++)
	{
		Invalidate(Update[i]);
	}
}

void ResStringUi::OnPosChange()
{
	PourAll();
}

void ResStringUi::OnCreate()
{
	Tools = new GToolBar;
	if (Tools)
	{
		char *FileName = LgiFindFile("_StringIcons.gif");
		if (FileName && Tools->SetBitmap(FileName, 16, 16))
		{
			Tools->Attach(this);

			Tools->AppendButton("New String", IDM_NEW, TBT_PUSH, !StringGrp->SystemObject());
			Tools->AppendButton("Delete String", IDM_DELETE, TBT_PUSH, !StringGrp->SystemObject());
			Tools->AppendSeparator();

			Tools->AppendButton("New Language", IDM_NEW_LANG, TBT_PUSH);
			Tools->AppendButton("Delete Language", IDM_DELETE_LANG, TBT_PUSH);
		}
		else
		{
			DeleteObj(Tools);
		}
		DeleteArray(FileName);
	}

	Status = new GStatusBar;
	if (Status)
	{
		Status->Attach(this);
		StatusInfo = Status->AppendPane("", 2);
	}

	ResFrame *Frame = new ResFrame(StringGrp);
	if (Frame)
	{
		Frame->Attach(this);
	}
}

GMessage::Result ResStringUi::OnEvent(GMessage *Msg)
{
	switch (MsgCode(Msg))
	{
		case M_COMMAND:
		{
			StringGrp->OnCommand(MsgA(Msg)&0xffff, MsgA(Msg)>>16, (OsView) MsgB(Msg));
			break;
		}
		case M_DESCRIBE:
		{
			char *Text = (char*) MsgB(Msg);
			if (Text)
			{
				StatusInfo->Name(Text);
			}
			break;
		}
	}

	return GView::OnEvent(Msg);
}
