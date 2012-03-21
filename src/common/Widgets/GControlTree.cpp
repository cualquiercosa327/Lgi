#include <stdio.h>

#include "Lgi.h"
#include "GControlTree.h"
#include "GToken.h"
#include "GEdit.h"
#include "GCheckBox.h"
#include "GCombo.h"
#include "GButton.h"

#define IDC_BROWSE -10

class GControlTreePriv
{
public:
    LgiResources *Factory;
    
    GControlTreePriv()
    {
        Factory = 0;
    }
};

GControlTree::Item::Item(char *Txt, const char *opt, GVariantType type, GArray<GControlTree::EnumValue> *pEnum)
{
	Opt.Reset(NewStr(opt));
	Enum.Reset(pEnum);
	SetText(Txt);
	Type = type;
	Ctrl = 0;
	Browse = 0;
}

GControlTree::Item::~Item()
{
}

void GControlTree::Item::OnVisible(bool v)
{
	if (Ctrl)
	{
		if (v)
			PositionControls();
		else
		{
			Ctrl->Visible(false);
			if (Browse) Browse->Visible(false);
		}
	}
}

GControlTree::Item *GControlTree::Item::Find(const char *opt)
{
	if (Opt && !stricmp(Opt, opt))
	{
		return this;
	}

	for (GTreeItem *i = GetChild(); i; i = i->GetNext())
	{
		GControlTree::Item *ci = dynamic_cast<GControlTree::Item*>(i);
		if (ci)
		{
			GControlTree::Item *f = ci->Find(opt);
			if (f)
				return f;
		}
	}

	return 0;
}

bool GControlTree::Item::Serialize(GDom *Store, bool Write)
{
	if (Opt)
	{
		if (Write)
		{
			Save();
			Store->SetValue(Opt, Value);
		}
		else
		{
			Store->GetValue(Opt, Value);
		}
	}

	for (GTreeItem *i = GetChild(); i; i = i->GetNext())
	{
		GControlTree::Item *ci = dynamic_cast<GControlTree::Item*>(i);
		if (ci)
		{
			ci->Serialize(Store, Write);
		}
	}
	
	return true;
}

void GControlTree::Item::SetValue(GVariant &v)
{
	Value = v;
	if (GTreeItem::Select())
	{
		DeleteObj(Ctrl);
		DeleteObj(Browse);
		Select(true);
	}
	else Update();
}

GRect &GControlTree::Item::GetRect()
{
	static GRect r;
	r.ZOff(-1, -1);
	GRect *p = Pos();
	if (p)
	{
		bool HasBrowse = (Flags & TYPE_FILE) != 0;
		int x = p->x2 + 5;
		r.x1 = x;
		r.x2 = GetTree()->GetPos().X() - (HasBrowse ? 50 : 10);
		int Cy = Ctrl ? Ctrl->Y() : 16;
		r.y1 = (p->y1 + (p->Y()/2)) - (Cy / 2);
		r.y2 = r.y1 + Cy - 1;
	}
	return r;
}

void GControlTree::Item::Save()
{
	if (Ctrl)
	{
		switch (Type)
		{
			case GV_STRING:
				Value = Ctrl->Name();
				break;
			case GV_BOOL:
				Value = Ctrl->Value() != 0;
				break;
			default:
			{
				if (Enum && Enum->Length())
				{
					GControlTree::EnumValue &e = (*Enum)[0];
					if (e.Value.Type == GV_STRING)
						Value = Ctrl->Name();
					else
						Value = (int)Ctrl->Value();
				}
				else
				{
					Value = (int)Ctrl->Value();
				}
				break;
			}
		}
	}
}

void GControlTree::Item::Select(bool b)
{
	GTreeItem::Select(b);

	if ((Ctrl != 0) ^ b)
	{
		if (b)
		{
			LgiAssert(Ctrl == 0);
			int FontY = SysFont->GetHeight();
			int CtrlY = FontY + (FontY >> 1);

			switch (Type)
			{
				case GV_STRING:
					if (Ctrl = new GEdit(-1, 0, 0, 200, CtrlY, 0))
						Ctrl->Name(Value.Str());
					if (Flags & TYPE_FILE)
					{
						Browse = new GButton(IDC_BROWSE, 0, 0, -1, CtrlY, "...");
					}
					break;
				case GV_BOOL:
					if (Ctrl = new GCheckBox(-1, 0, 0, 14, 16, 0))
						Ctrl->Value(Value.CastInt32());
					break;
				case GV_INT32:
					if (Enum)
					{
						GCombo *Cbo;
						if (Ctrl = Cbo = new GCombo(-1, 0, 0, 120, CtrlY, 0))
						{
							int Idx = -1;

							for (int i=0; i<Enum->Length(); i++)
							{
								EnumValue &e = (*Enum)[i];
								Cbo->Insert(e.Name);
								if (e.Value == Value)
									Idx = i;
							}

							if (Idx >= 0)
								Ctrl->Value(Idx);
						}
					}
					else
					{
						if (Ctrl = new GEdit(-1, 0, 0, 60, CtrlY, 0))
							Ctrl->Value(Value.CastInt32());
					}
					break;
			}

			if (Ctrl)
			{
				Ctrl->SetBackgroundFill(new GViewFill(LC_WORKSPACE, 24));
				Ctrl->Visible(false);
				Ctrl->Attach(GetTree());

				if (Browse)
				{
					Browse->SetBackgroundFill(new GViewFill(LC_WORKSPACE, 24));
					Browse->Visible(false);
					Browse->Attach(GetTree());
				}

				PositionControls();
			}
		}
		else if (Ctrl)
		{
			Save();
			DeleteObj(Ctrl);
			DeleteObj(Browse);
		}

		Update();
	}
}

void GControlTree::Item::PositionControls()
{
	if (Ctrl)
	{
		GRect r = GetRect();
		Ctrl->SetPos(r);
		Ctrl->Visible(true);
		if (Browse)
		{
			GRect b = Browse->GetPos();
			b.Offset(r.x2 + 5 - b.x1, r.y1 - b.y1);
			Browse->SetPos(b);
			Browse->Visible(true);
		}
	}
}

void GControlTree::Item::OnPaint(ItemPaintCtx &Ctx)
{
	GTreeItem::OnPaint(Ctx);

	if (!Ctrl)
	{
		SysBold->Colour(LC_TEXT, LC_WORKSPACE);
		SysBold->Transparent(true);

		GRect p = GetRect();
		switch (Type)
		{
			case GV_INT32:
			{
				char s[32], *Disp = 0;
				if (Enum)
				{
					for (int i=0; i<Enum->Length(); i++)
					{
						EnumValue &e = (*Enum)[i];
						if (e.Value == Value)
						{
							Disp = e.Name;
							break;
						}
					}

					if (Disp)
					{
						GDisplayString ds(SysBold, Disp);
						ds.Draw(Ctx.pDC, p.x1 + 8, p.y1 + 1);
					}
				}
				else
				{
					sprintf(Disp = s, "%i", Value.CastInt32());
					GDisplayString ds(SysBold, Disp);
					ds.Draw(Ctx.pDC, p.x1 + 6, p.y1 + 2);
				}
				break;
			}
			case GV_STRING:
			{
				GDisplayString ds(SysBold, Value.Str());
				ds.Draw(Ctx.pDC, p.x1 + 6, p.y1 + 2);
				break;
			}
			case GV_BOOL:
			{
				GDisplayString ds(SysBold, (char*) (Value.CastInt32() ? "true" : "false"));
				ds.Draw(Ctx.pDC, p.x1 + 1, p.y1 + 1);
				break;
			}
		}
	}
	else
	{
		PositionControls();
	}
}

///////////////////////////////////////////////////////////////////////
GControlTree::GControlTree() : GTree(-1, 0, 0, 100, 100)
{
	_ObjName = Res_ControlTree;
	MultipleSelect = false;
	d = new GControlTreePriv;
}

GControlTree::~GControlTree()
{
	DeleteObj(d);
}

GControlTree::Item *GControlTree::Find(const char *opt)
{
	for (GTreeItem *i = GetChild(); i; i = i->GetNext())
	{
		GControlTree::Item *ci = dynamic_cast<GControlTree::Item*>(i);
		if (ci)
		{
			GControlTree::Item *f = ci->Find(opt);
			if (f)
				return f;
		}
	}

	return 0;
}

bool GControlTree::Serialize(GDom *Store, bool Write)
{
	bool Error = false;

	if (!Store)
		return false;

	for (GTreeItem *i = GetChild(); i; i = i->GetNext())
	{
		GControlTree::Item *ci = dynamic_cast<GControlTree::Item*>(i);
		if (ci)
		{
			Error = Error | ci->Serialize(Store, Write);
		}
	}

	return !Error;
}

GControlTree::Item *GControlTree::Resolve(bool Create, const char *Path, GVariantType Type, GArray<EnumValue> *Enum)
{
	GToken t(Path, ".");
	if (t.Length() > 0)
	{
		GTreeNode *Cur = this;
		for (int i=0; i<t.Length(); i++)
		{
			GTreeItem *Match = 0;
			for (GTreeItem *c = Cur->GetChild(); c; c = c->GetNext())
			{
				char *s = c->GetText();
				if (s AND stricmp(t[i], s) == 0)
				{
					Match = c;
					break;
				}
			}

			if (Match)
			{
				Cur = Match;
			}
			else
			{
				if (Create AND i == t.Length() - 1)
				{
					GControlTree::Item *Ci = new GControlTree::Item(t[i], Path, Type, Enum);
					if (Ci)
					{
						Cur->Insert(Ci);
						
						GTreeItem *p = Ci->GetParent();
						if (p)
							p->Expanded(true);
						return Ci;
					}
				}

				return 0;
			}
		}

		return dynamic_cast<GControlTree::Item*>(Cur);
	}

	return 0;
}

GTreeItem *GControlTree::Insert(const char *DomPath, GVariantType Type, GVariant *Value, GArray<EnumValue> *Enum)
{
	GControlTree::Item *c = Resolve(true, DomPath, Type, Enum);
	if (c)
	{
		if (Value)
			c->SetValue(*Value);
	}

	return 0;
}

void GControlTree::ReadTree(GXmlTag *t, GTreeNode *n)
{
	for (GXmlTag *c = t->Children.First(); c; c = t->Children.Next())
	{
		int StrRef = c->GetAsInt("ref");
		LgiStringRes *Str = d->Factory->StrFromRef(StrRef);
		LgiAssert(Str);

		char *Type = c->GetAttr("ControlType");
		GVariantType iType = GV_NULL;
		int Flags = 0;
		if (Type)
		{
			if (!stricmp(Type, "string"))
				iType = GV_STRING;
			else if (!stricmp(Type, "file"))
			{
				iType = GV_STRING;
				Flags |= GControlTree::Item::TYPE_FILE;
			}
			else if (!stricmp(Type, "bool"))
				iType = GV_BOOL;
			else if (!stricmp(Type, "int") ||
					 !stricmp(Type, "enum"))
				iType = GV_INT32;
		}

		GControlTree::Item *ct = new GControlTree::Item(Str?Str->Str:(char*)"#error", c->GetAttr("ControlTag"), iType, 0); // GArray<EnumValue> *pEnum
		if (ct)
		{
			ct->Flags = Flags;
			n->Insert(ct);
			ReadTree(c, ct);
			ct->Expanded(true);
		}
	}
}

bool GControlTree::SetVariant(const char *Name, GVariant &Value, char *Array)
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
		else if (d->Factory)
			ReadTree(x, this);
	    else
	        LgiAssert(!"No factory.");
			
		d->Factory = 0;
	}
	else if (!stricmp(Name, "LgiFactory"))
	{
	    d->Factory = dynamic_cast<LgiResources*>((ResFactory*)Value.CastVoidPtr());
	}
	else return false;

	return true;
}

int GControlTree::OnNotify(GViewI *c, int f)
{
	switch (c->GetId())
	{
		case IDC_BROWSE:
		{
			Item *i = dynamic_cast<Item*>(Selection());
			if (i)
			{
				GFileSelect s;
				s.Parent(this);
				if (s.Open())
				{
					GVariant v;
					i->SetValue(v = s.Name());
				}
			}
			return 0;
		}
	}

	return GTree::OnNotify(c, f);
}

///////////////////////////////////////////////////////////////////////////////
class GControlTree_Factory : public GViewFactory
{
	GView *NewView(const char *Class, GRect *Pos, const char *Text)
	{
		if (stricmp(Class, "GControlTree") == 0)
		{
			return new GControlTree;
		}

		return 0;
	}

} ControlTree_Factory;
