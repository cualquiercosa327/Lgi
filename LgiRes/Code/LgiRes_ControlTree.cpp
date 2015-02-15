#include <stdlib.h>
#include "LgiResEdit.h"
#include "LgiRes_Dialog.h"
#include "GButton.h"
#include "GVariant.h"
#include "GToken.h"

#define IDM_NEW_CHILD		100
#define IDM_NEW_NEXT		101
#define VAL_ControlType		"ControlType"
#define VAL_ControlTag		"ControlTag"

class CtrlControlTreePriv
{
public:
	ResDialog *Dlg;
	bool DiscardClick;
	GLanguageId CurLang;

	CtrlControlTreePriv(ResDialog *dlg)
	{
		CurLang = 0;
		Dlg = dlg;
		DiscardClick = false;
	}
};

class CtNode : public GTreeItem
{
	CtrlControlTreePriv *d;

public:
	ResString *Str;
	GAutoString Type;
	GAutoString Tag;

	CtNode(CtrlControlTreePriv *priv, GView *update, int StringRef)
	{
		d = priv;
		Str = 0;

		if (d->Dlg)
		{
			if (d->Dlg && d->Dlg->Symbols)
			{
				if (StringRef)
					// Find the ref'd string in the string group...
					Str = d->Dlg->Symbols->FindRef(StringRef);
				else
					// We create a symbol for this resource
					Str = d->Dlg->Symbols->CreateStr();
			}

			if (Str)
				Str->UpdateWnd = update;
		}
	}

	char *GetText(int i)
	{
		if (GetCss(true))
		{
			GCss::ColorDef c;
			if (!Str || !Str->Get())
				GetCss()->Color(GCss::ColorDef(GCss::ColorRgb, Rgb32(0xbb, 0xbb, 0xbb)));
			else
				GetCss()->DeleteProp(GCss::PropColor);
		}

		if (!Str)
			return (char*)"(no string)";

		if (!Str->Get())
			return (char*)"(empty string)";

		return Str->Get();
	}

	void OnMouseClick(GMouse &m)
	{
		if (m.IsContextMenu())
		{
			GSubMenu s;
			s.AppendItem("New Child", IDM_NEW_CHILD, true);
			s.AppendItem("New Next", IDM_NEW_NEXT, true);
			s.AppendItem("Delete", IDM_DELETE, true);
			s.AppendSeparator();
			s.AppendItem("Copy Text", IDM_COPY_TEXT, true);
			s.AppendItem("Paste Text", IDM_PASTE_TEXT, true);
			m.ToScreen();

			int Cmd;
			if ((Cmd = s.Float(GetTree(), m.x, m.y)))
			{
				d->DiscardClick = true;

				switch (Cmd)
				{
					case IDM_NEW_CHILD:
					{
						// Insert at the start... user can use "next" for positions other than the start.
						Insert(new CtNode(d, Str->UpdateWnd, 0), 0);
						Expanded(true);
						break;
					}
					case IDM_NEW_NEXT:
					{
						int Idx = IndexOf();
						if (GetParent())
						{
							if (Idx >= 0)
							{
								GetParent()->Insert(new CtNode(d, Str->UpdateWnd, 0), Idx+1);
							}
						}
						else
						{
							if (Idx >= 0)
							{
								GetTree()->Insert(new CtNode(d, Str->UpdateWnd, 0), Idx+1);
							}
						}
						break;
					}
					case IDM_DELETE:
					{
						delete this;
						break;
					}
					case IDM_COPY_TEXT:
					{
						if (Str)
							Str->CopyText();
						break;
					}
					case IDM_PASTE_TEXT:
					{
						if (Str)
						{
							Str->PasteText();
							Update();
						}
						break;
					}
				}
			}
		}
	}
};

CtrlControlTree::CtrlControlTree(ResDialog *dlg, GXmlTag *load) :
	ResDialogCtrl(dlg, Res_ControlTree, load),
	GTree(100, 0, 0, 100, 100)
{
	d = new CtrlControlTreePriv(dlg);

	if (!load)
	{
		Insert(new CtNode(d, this, 0));
	}
}

CtrlControlTree::~CtrlControlTree()
{
	delete d;
}

void CtrlControlTree::OnMouseClick(GMouse &m)
{
	GTree::OnMouseClick(m);

	if (!d->DiscardClick)
	{
		ResDialogCtrl::OnMouseClick(m);
	}
	else d->DiscardClick = false;
}

void CtrlControlTree::OnMouseMove(GMouse &m)
{
	GTree::OnMouseMove(m);
	ResDialogCtrl::OnMouseMove(m);
}

void CtrlControlTree::OnPaint(GSurface *pDC)
{
	if (Dlg->App()->GetCurLang()->Id != d->CurLang)
	{
		d->CurLang = Dlg->App()->GetCurLang()->Id;
		UpdateAllItems();
	}

	GTree::OnPaint(pDC);
}

bool CtrlControlTree::GetFields(FieldTree &Fields)
{
	bool Status = ResDialogCtrl::GetFields(Fields);
	if (Status && Fields.GetDeep())
	{
		CtNode *n = dynamic_cast<CtNode*>(Selection());
		if (n && n->Str)
		{
			Status |= n->Str->GetFields(Fields);
			Fields.Insert(n->Str, DATA_STR, -1, VAL_ControlType, "Type");
			Fields.Insert(n->Str, DATA_STR, -1, VAL_ControlTag, "OptionTag");
		}
	}
	return Status;
}

bool CtrlControlTree::Serialize(FieldTree &Fields)
{
	bool Status = ResDialogCtrl::Serialize(Fields);
	if (Status && Fields.GetDeep())
	{
		CtNode *n = dynamic_cast<CtNode*>(Selection());
		if (n && n->Str)
		{
			Status |= n->Str->Serialize(Fields);
			Fields.Serialize(n->Str, VAL_ControlType, n->Type);
			Fields.Serialize(n->Str, VAL_ControlTag, n->Tag);
			n->Update();
		}
	}
	return Status;
}

void WriteTree(GXmlTag *t, GTreeNode *n)
{
	CtNode *ct = dynamic_cast<CtNode*>(n);
	if (ct)
	{
		t->SetAttr("Ref", ct->Str->GetRef());
		t->SetAttr(VAL_ControlType, ct->Type);
		t->SetAttr(VAL_ControlTag, ct->Tag);
	}

	for (GTreeNode *c = n->GetChild(); c; c = c->GetNext())
	{
		GXmlTag *h = new GXmlTag;
		WriteTree(h, c);
		DeleteArray(h->Tag);
		h->Tag = NewStr("Control");
		t->InsertTag(h);
	}
}

bool CtrlControlTree::GetVariant(const char *Name, GVariant &Value, char *Array)
{
	if (!Name)
		return false;

	if (!stricmp(Name, "Tree"))
	{
		GXmlTag *x = new GXmlTag;
		WriteTree(x, this);
		Value = x;
	}
	else return false;

	return true;
}

void ReadTree(GXmlTag *t, GTreeNode *n, CtrlControlTreePriv *d, GView *v)
{
	CtNode *ct = dynamic_cast<CtNode*>(n);
	if (ct && ct->Str)
	{
		ct->Type.Reset(NewStr(t->GetAttr(VAL_ControlType)));
		ct->Tag.Reset(NewStr(t->GetAttr(VAL_ControlTag)));
	}

	for (GXmlTag *c = t->Children.First(); c; c = t->Children.Next())
	{
		int StrRef = c->GetAsInt("ref");
		CtNode *nw = new CtNode(d, v, StrRef);
		ReadTree(c, nw, d, v);
		n->Insert(nw);

		GTreeItem *it = dynamic_cast<GTreeItem*>(n);
		if (it) it->Expanded(true);
	}
}

bool CtrlControlTree::SetVariant(const char *Name, GVariant &Value, char *Array)
{
	if (!Name)
		return false;

	if (!stricmp(Name, "Tree"))
	{
		if (Value.Type != GV_DOM)
			return false;

		Empty();

		GXmlTag *x = dynamic_cast<GXmlTag*>(Value.Value.Dom);
		if (!x)
			LgiAssert(!"Not the right object.");
		else
			ReadTree(x, this, d, this);
	}
	else return false;

	return true;
}

