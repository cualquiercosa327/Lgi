#include <math.h>

#include "Lgi.h"
#include "GFilterUi.h"
#include "GPath.h"
#include "GEdit.h"
#include "GCombo.h"
#include "GToken.h"
#include "GDisplayString.h"

#define IconSize			15
static GColour TransparentBlk(0, 0, 0, 0);
static GColour White(255, 255, 255);
static GColour Black(0, 0, 0);
static GColour ContentColour(0, 0, 0, 160);

// Field size
#define SIZE_NOT			45
#define SIZE_FIELD_NAME		130
#ifdef MAC
#define SIZE_OP				85
#else
#define SIZE_OP				75
#endif
#define SIZE_VALUE			150

enum FilterIcon
{
	IconNewCond,
	IconNewAnd,
	IconNewOr,
	IconDelete,
	IconMoveUp,
	IconMoveDown,
	IconOptions,
	IconDropDown,
	IconBoolFalse,
	IconBoolTrue,
	IconMax,
};

#define IconAlpha	255

static GColour IconColour[IconMax] =
{
	GColour(192, 192, 192, IconAlpha), // new condition
	GColour(118, 160, 230, IconAlpha), // new and
	GColour(32, 224, 32, IconAlpha), // new or
	GColour(230, 90, 65, IconAlpha), // delete
	GColour(192, 192, 192, IconAlpha), // up
	GColour(192, 192, 192, IconAlpha), // down
	GColour(192, 192, 192, IconAlpha), // options
	GColour(192, 192, 192, IconAlpha), // drop down
	GColour(255, 192, 0, IconAlpha), // bool false
	GColour(255, 192, 0, IconAlpha), // bool true
};

static const char *IconName[IconMax];

static const char **GetIconNames()
{
	if (!IconName[0])
	{
		int i = 0;
		IconName[i++] = LgiLoadString(L_FUI_NEW_CONDITION, "New Condition");
		IconName[i++] = LgiLoadString(L_FUI_NEW_AND, "New And");
		IconName[i++] = LgiLoadString(L_FUI_NEW_OR, "New Or");
		IconName[i++] = LgiLoadString(L_FUI_DELETE, "Delete");
		IconName[i++] = LgiLoadString(L_FUI_MOVE_UP, "Move Up");
		IconName[i++] = LgiLoadString(L_FUI_MOVE_DOWN, "Move Down");
		i++;
		IconName[i++] = LgiLoadString(L_FUI_OPTIONS, "Options");
		i++;
		IconName[i++] = LgiLoadString(L_FUI_NOT, "Not");
	}

	return IconName;
}

void convert_to_nonpremul(GSurface *pDC)
{
	if (!pDC)
		return;
		
	switch (pDC->GetColourSpace())
	{
		case CsArgb32:
		{
			for (int y=0; y<pDC->Y(); y++)
			{
				GArgb32 *p = (GArgb32*) (*pDC)[y];
				GArgb32 *e = p + pDC->X();

				while (p < e)
				{
					if (p->a > 0 && p->a < 255)
					{
						p->r = (int) p->r * 255 / p->a;
						p->g = (int) p->g * 255 / p->a;
						p->b = (int) p->b * 255 / p->a;
					}

					p++;
				}
			}
			break;
		}
		default:
			LgiAssert(0);
			break;
	}
}

class GFilterTree : public GTree
{
	friend class GFilterItem;

public:
	GFilterTree() : GTree(-1, 0, 0, 100, 100)
	{
	}

	void OnFocus(bool f)
	{
		if (f && !Selection())
		{
			GTreeItem *n = GetChild();
			if (n) n->Select(true);
		}
	}
};

template<typename T>
void HalveAlpha(T *p, int width)
{
	T *e = p + width;
	while (p < e)
	{
		p->a >>= 1;
		p++;
	}
}

class GFilterViewPrivate
{
public:
	GFilterView *View;
	GAutoPtr<GFilterTree> Tree;
	bool ShowLegend;
	GRect Info;
	GArray<GSurface*> Icons;
	FilterUi_Menu Callback;
	void *CallbackData;
	GAutoPtr<GDisplayString> dsNot;
	GAutoPtr<GDisplayString> dsAnd;
	GAutoPtr<GDisplayString> dsOr;
	GAutoPtr<GDisplayString> dsNew;
	GAutoPtr<GDisplayString> dsLegend;
	GArray<char*> OpNames;

	GFilterViewPrivate(GFilterView *v)
	{
		View = v;

		dsNot.Reset(new GDisplayString(SysFont, (char*)LgiLoadString(L_FUI_NOT, "Not")));
		dsNew.Reset(new GDisplayString(SysBold, (char*)LgiLoadString(L_FUI_NEW, "New")));
		dsAnd.Reset(new GDisplayString(SysBold, (char*)LgiLoadString(L_FUI_AND, "And")));
		dsOr.Reset(new GDisplayString(SysBold, (char*)LgiLoadString(L_FUI_OR, "Or")));
		dsLegend.Reset(new GDisplayString(SysBold, (char*)LgiLoadString(L_FUI_LEGEND, "Legend:")));
		
		ShowLegend = true;
		Callback = 0;
		CallbackData = 0;

		for (int i=0; i<IconMax; i++)
		{
			Icons[i] = new GMemDC;
			if (Icons[i] && Icons[i]->Create(IconSize, IconSize, 32))
			{
				Draw(Icons[i], IconColour[i], (FilterIcon)i);
			}
		}
	}

	~GFilterViewPrivate()
	{
		Icons.DeleteObjects();
		OpNames.DeleteArrays();
	}

	void Draw(GSurface *pDC, GColour c, FilterIcon Icon)
	{
		// Clear to transparent black
		pDC->Colour(TransparentBlk);
		pDC->Rectangle();

		// Draw the icon backrgound
		double n = pDC->X() - 2;
		GPointF Ctr(n/2, n), Rim(n/2, 0);

		if (1)
		{
			// Shadow
			GPath p;
			double z = ((double)pDC->X()-0.5) / 2;
			p.Circle(z, z, z);
			GSolidBrush b(Rgba32(0, 0, 0, 40));
			p.Fill(pDC, b);
		}

		if (1)
		{
			GPath p;
			p.Circle(n/2, n/2, n/2);

			GBlendStop r[] =
			{
				{0.0, White.Mix(c, 0.1f).c32()},
				{0.3, White.Mix(c, 0.3f).c32()},
				{0.6, c.c32()},
				{1.0, Black.Mix(c, 0.8f).c32()},
			};

			GRadialBlendBrush b(Ctr, Rim, CountOf(r), r);
			p.Fill(pDC, b);
		}

		if (1)
		{
			// Draw the highlight
			GPath p;
			p.Circle(n/2, n/2, n/2);
			GBlendStop Highlight[] =
			{
				{0.0, TransparentBlk.c32()},
				{0.7, TransparentBlk.c32()},
				{0.9, White.c32()},
			};

			GLinearBlendBrush b2(Ctr, Rim, CountOf(Highlight), Highlight);
			p.Fill(pDC, b2);
		}

		double k[4] =
		{
			n * 3 / 14,
			n * 3 / 7,
			n * 4 / 7,
			n * 11 / 14
		};

		int Plus[][2] =
		{
			{0, 1}, {0, 2}, {1, 2}, {1, 3}, {2, 3}, {2, 2}, {3, 2},
			{3, 1}, {2, 1}, {2, 0}, {1, 0}, {1, 1}, {0, 1}
		};

		// Draw the icon content
		switch (Icon)
		{
			default: break;
			case IconNewCond:
			case IconBoolTrue:
			{
				GPath p;
				p.Circle(n/2, n/2, n/6);
				GSolidBrush b(ContentColour);
				p.Fill(pDC, b);
				break;
			}
			case IconNewAnd:
			case IconDelete:
			{
				GPath p;
				GPointF Ctr(n/2, n/2);
				for (int i=0; i<CountOf(Plus); i++)
				{
					GPointF pt(k[Plus[i][0]], k[Plus[i][1]]);
					pt = pt - Ctr;
					pt.Rotate(LGI_DegToRad(45));
					pt = pt + Ctr;					
					if (i) p.LineTo(pt.x, pt.y);
					else p.MoveTo(pt.x, pt.y);
				}

				GSolidBrush b(ContentColour);
				p.Fill(pDC, b);
				break;
			}
			case IconNewOr:
			{
				GPath p;
				for (int i=0; i<CountOf(Plus); i++)
				{
					if (i) p.LineTo(k[Plus[i][0]], k[Plus[i][1]]);
					else p.MoveTo(k[Plus[i][0]], k[Plus[i][1]]);
				}
				GSolidBrush b(ContentColour);
				p.Fill(pDC, b);
				break;
			}
			case IconMoveUp:
			case IconMoveDown:
			{
				int Pt[][2] =
				{
					(int)(n/2), (int)(k[0]),
					(int)(k[0]), (int)(k[1]),
					(int)(k[1]), (int)(k[1]),
					(int)(k[1]), (int)(k[3]),
					(int)(k[2]), (int)(k[3]),
					(int)(k[2]), (int)(k[1]),
					(int)(k[3]), (int)(k[1]),
					(int)(n/2), (int)(k[0]),
				};

				GPath p;
				for (int i=0; i<CountOf(Pt); i++)
				{
					GPointF m(Pt[i][0]+0.5, Pt[i][1]+0.5);
					if (Icon == IconMoveDown)
						m.y = k[3] - (m.y - k[0]);
					if (i) p.LineTo(m.x, m.y);
					else p.MoveTo(m.x, m.y);
				}
				GSolidBrush b(ContentColour);
				p.Fill(pDC, b);
				break;
			}
			case IconOptions:
			{
				GPath p;
				for (int i=0; i<3; i++)
				{
					GPointF c(n * (1+i) / 4, n / 2);
					p.Circle(c, n/10);
				}
				
				GSolidBrush b(ContentColour);
				p.Fill(pDC, b);
				break;
			}
			case IconDropDown:
			{
				double x[3] =
				{
					n * 1 / 4,
					n * 2 / 4,
					n * 3 / 4,
				};
				double y[3] =
				{
					n * 2 / 6,
					n * 3 / 6,
					n * 4 / 6,
				};
				GPath p;
				p.MoveTo(x[0], y[0]+0.5);
				p.LineTo(x[1], y[2]+0.5);
				p.LineTo(x[2], y[0]+0.5);
				p.LineTo(x[0], y[0]+0.5);
				
				GSolidBrush b(ContentColour);
				p.Fill(pDC, b);
				break;
			}
		}

		if (1)
		{
			// Draw the outline
			GPath p;
			p.Circle(n/2, n/2, n/2);
			p.Circle(n/2, n/2, n/2 - 1);
			GSolidBrush b(Rgba32(64, 64, 64, 255));
			p.Fill(pDC, b);
		}

		if (1)
		{
			// Make new icons transparent
			switch (Icon)
			{
				default: break;
				case IconNewCond:
				case IconNewAnd:
				case IconNewOr:
				{
					for (int y=0; y<pDC->Y(); y++)
					{
						switch (pDC->GetColourSpace())
						{
							#define HalveAlphaCase(name) \
								case Cs##name: HalveAlpha((G##name*)(*pDC)[y], pDC->X()); break
							
							HalveAlphaCase(Rgba32);
							HalveAlphaCase(Bgra32);
							HalveAlphaCase(Argb32);
							HalveAlphaCase(Abgr32);
							default:
								LgiAssert(pDC->GetBits() < 32);
								break;
						}
					}
					break;
				}
			}
		}
	}
};

///////////////////////////////////////////////////////////////////////////////
class GFilterItemPrivate
{
public:
	GFilterNode Node;
	GFilterViewPrivate *Data;
	GRect Btns[IconMax];
	GRect NotBtn;
	GRect FieldBtn, FieldDropBtn;
	GRect OpBtn, OpDropBtn;
	GRect ValueBtn;

	bool Not;
	char *Field, *Value;
	int Op;
	GEdit *FieldEd, *ValueEd;
	GCombo *OpCbo;

	GFilterItemPrivate()
	{
		Not = false;
		Data = 0;
		Node = LNODE_NULL;
		EmptyRects();
		Field = Value = 0;
		Op = 0;
		FieldEd = ValueEd = 0;
		OpCbo = 0;
	}

	~GFilterItemPrivate()
	{
		DeleteArray(Field);
		DeleteArray(Value);
		DeleteObj(FieldEd);
		DeleteObj(OpCbo);
		DeleteObj(ValueEd);
	}

	void EmptyRects()
	{
		for (int i=0; i<CountOf(Btns); i++)
			Btns[i].ZOff(-1, -1);

		FieldBtn.ZOff(-1, -1);
		FieldDropBtn.ZOff(-1, -1);
		OpBtn.ZOff(-1, -1);
		OpDropBtn.ZOff(-1, -1);
		ValueBtn.ZOff(-1, -1);
	}
};

GFilterItem::GFilterItem(GFilterViewPrivate *Data, GFilterNode Node)
{
	d = new GFilterItemPrivate;
	d->Data = Data;
	SetText("Node");
	SetNode(Node);
}

GFilterItem::~GFilterItem()
{
	DeleteObj(d);
}

bool GFilterItem::GetNot()
{
	return d->Not;
}

char *GFilterItem::GetField()
{
	if (d->FieldEd) return d->FieldEd->Name();

	return d->Field;
}

int GFilterItem::GetOp()
{
	if (d->OpCbo != NULL)
		return (int)d->OpCbo->Value();
	return d->Op;
}

char *GFilterItem::GetValue()
{
	if (d->ValueEd) return d->ValueEd->Name();

	return d->Value;
}

void GFilterItem::SetNot(bool b)
{
	d->Not = b;
	Update();
}

void GFilterItem::SetField(const char *s)
{
	DeleteArray(d->Field);
	d->Field = NewStr(s);
	if (d->FieldEd) d->FieldEd->Name(s);
	else Update();
}

void GFilterItem::SetOp(int s)
{
	d->Op = s;
	if (d->OpCbo) d->OpCbo->Value(s);
	else Update();
}

void GFilterItem::SetValue(char *s)
{
	DeleteArray(d->Value);
	d->Value = NewStr(s);
	if (d->ValueEd) d->ValueEd->Name(s);
	else Update();
}

#define StartCtrl(Rc, Ed, Name, Obj) \
	if (!d->Ed) \
	{ \
		GdcPt2 sc = d->Data->Tree->_ScrollPos(); \
		d->Ed = new Obj(n++, c.x1 + Pos->x1 + Rc.x1 - sc.x, c.y1 + Pos->y1 + Rc.y1 - sc.y, \
								Rc.X(), Rc.Y(), \
								Name); \
		if (d->Ed) \
		{ \
			d->Ed->Attach(GetTree()); \
		} \
	} \
	else \
	{ \
		GRect r = Rc; \
		GdcPt2 sc = d->Data->Tree->_ScrollPos(); \
		r.Offset(c.x1 + Pos->x1 - sc.x, c.y1 + Pos->y1 - sc.y); \
		d->Ed->SetPos(r); \
	}

#define EndEdit(Rc, Ed, Var) \
	if (d->Ed) \
	{ \
		DeleteArray(d->Var); \
		d->Var = NewStr(d->Ed->Name()); \
		DeleteObj(d->Ed); \
	}

void GFilterItem::_PourText(GdcPt2 &Size)
{
	Size.y = SysFont->GetHeight() +
	#ifdef MAC
	14; // Not sure what the deal is here... it just looks better.
	#else
	10;
	#endif
	
	switch (d->Node)
	{
		case LNODE_NEW:
		case LNODE_AND:
		case LNODE_OR:
			Size.x = 120;
			break;
		default:
			Size.x = 126 + SIZE_NOT + SIZE_FIELD_NAME + SIZE_OP + SIZE_VALUE;
			break;
	}
}

void GFilterItem::_PaintText(GItem::ItemPaintCtx &Ctx)
{
	GRect *Pos = _GetRect(TreeItemText);

	// Create a memory context
	GMemDC Buf(Pos->X(), Pos->Y(), System32BitColourSpace);
	Buf.Colour(LC_WORKSPACE, 24);
	Buf.Rectangle(0);

	// Draw the background
	double ox = 1, oy = 1;
	double r = (double)(Pos->Y() - (oy * 2)) / 2;
	GColour BackCol = TransparentBlk;
	GColour Workspace(LC_WORKSPACE, 24);
	switch (d->Node)
	{
		case LNODE_NEW:
			BackCol.Rgb(0xd0, 0xd0, 0xd0);
			break;
		case LNODE_COND:
		default:
			BackCol.Set(LC_MED, 24);
			break;
		case LNODE_AND:
		case LNODE_OR:
			BackCol = White.Mix(IconColour[d->Node], 0.3f);
			break;
	}

	if (Select())
	{
		GPath p;
		GSolidBrush b(Rgb24To32(LC_FOCUS_SEL_BACK));
		GRectF PosF(0, 0, Pos->X()-1, Pos->Y());
		p.RoundRect(PosF, PosF.Y()/2);
		p.Fill(&Buf, b);
	}

	{
		GPath p;
		GSolidBrush b(BackCol);
		GRectF PosF(0, 0, Pos->X()-(ox*2)-1, Pos->Y()-(oy*2));
		PosF.Offset(ox, oy);
		p.RoundRect(PosF, r);
		p.Fill(&Buf, b);
	}

	if (d->Node == LNODE_NEW)
	{
		GPath p;
		GSolidBrush b(Workspace);
		GRectF PosF(0, 0, Pos->X()-(ox*2)-3, Pos->Y()-(oy*2)-2);
		PosF.Offset(ox+1, oy+1);
		p.RoundRect(PosF, r);
		p.Fill(&Buf, b);
	}

	// Clear the button rects
	d->EmptyRects();

	// Draw the content
	int IconY = (int)(oy + r) - (IconSize / 2);
	switch (d->Node)
	{
		case LNODE_NEW:
		{
			Buf.Op(GDC_ALPHA);
			int x = Pos->X() - (int)r - IconSize + 4;
			int y = (int)(oy + r) - (IconSize / 2);

			// New OR
			d->Btns[IconNewOr].ZOff(IconSize-1, IconSize-1);
			d->Btns[IconNewOr].Offset(x, y);
			Buf.Blt(x, y, d->Data->Icons[IconNewOr]);
			x -= 2 + IconSize;

			// New AND
			d->Btns[IconNewAnd].ZOff(IconSize-1, IconSize-1);
			d->Btns[IconNewAnd].Offset(x, y);
			Buf.Blt(x, y, d->Data->Icons[IconNewAnd]);
			x -= 2 + IconSize;

			if (!IsRoot())
			{
				// New Condition
				d->Btns[IconNewCond].ZOff(IconSize-1, IconSize-1);
				d->Btns[IconNewCond].Offset(x, y);
				Buf.Blt(x, y, d->Data->Icons[IconNewCond]);
			}
			break;
		}
		default:
		{
			Buf.Op(GDC_ALPHA);
			int x = (int)((double)Pos->X() - r - (IconSize / 2) - 2);
			
			// Delete
			d->Btns[IconDelete].ZOff(IconSize-1, IconSize-1);
			d->Btns[IconDelete].Offset(x, IconY);
			Buf.Blt(x, IconY, d->Data->Icons[IconDelete]);
			x -= 2 + IconSize;
			
			if (!IsRoot())
			{
				// Move down
				d->Btns[IconMoveDown].ZOff(IconSize-1, IconSize-1);
				d->Btns[IconMoveDown].Offset(x, IconY);
				Buf.Blt(x, IconY, d->Data->Icons[IconMoveDown]);
				x -= 2 + IconSize;
				
				// Move up
				d->Btns[IconMoveUp].ZOff(IconSize-1, IconSize-1);
				d->Btns[IconMoveUp].Offset(x, IconY);
				Buf.Blt(x, IconY, d->Data->Icons[IconMoveUp]);
				x -= 2 + IconSize;
			}

			if (d->Node == LNODE_AND ||
				d->Node == LNODE_OR)
			{
				// Configure
				d->Btns[IconOptions].ZOff(IconSize-1, IconSize-1);
				d->Btns[IconOptions].Offset(x, IconY);
				Buf.Blt(x, IconY, d->Data->Icons[IconOptions]);
				x -= 2 + IconSize;
			}
			break;
		}
	}

	switch (d->Node)
	{
		default: break;
		case LNODE_COND:
		{
			// Layout stuff
			d->FieldBtn.ZOff(SIZE_FIELD_NAME, (int)(r * 2) - 5);
			d->FieldBtn.Offset((int)(ox + r)+SIZE_NOT, (int)oy + 2);
			d->FieldDropBtn.ZOff(IconSize-1, IconSize-1);
			d->FieldDropBtn.Offset(d->FieldBtn.x2 + 1, d->FieldBtn.y1 + ((d->FieldBtn.Y()-d->FieldDropBtn.Y())/2));
			d->NotBtn.ZOff(IconSize-1, IconSize-1);
			d->NotBtn.Offset((int)r, IconY);
			d->OpBtn.ZOff(SIZE_OP, d->FieldBtn.Y()+1);
			d->OpBtn.Offset(d->FieldDropBtn.x2 + 8, d->FieldBtn.y1-1);
			d->OpDropBtn.ZOff(IconSize-1, IconSize-1);
			d->OpDropBtn.Offset(d->OpBtn.x2 + 1, d->FieldDropBtn.y1);
			d->ValueBtn.ZOff(SIZE_VALUE, d->FieldBtn.Y()-1);
			d->ValueBtn.Offset(d->OpDropBtn.x2 + 8, d->FieldBtn.y1);

			// Draw stuff
			Buf.Op(GDC_ALPHA);
			Buf.Blt(d->NotBtn.x1, d->NotBtn.y1, d->Data->Icons[d->Not ? IconBoolTrue : IconBoolFalse]);
			Buf.Blt(d->OpDropBtn.x1, d->OpDropBtn.y1, d->Data->Icons[IconDropDown]);
			Buf.Blt(d->FieldDropBtn.x1, d->FieldDropBtn.y1, d->Data->Icons[IconDropDown]);
			Buf.Op(GDC_SET);

			SysFont->Transparent(true);
			SysFont->Colour(GColour(LC_TEXT, 24), Ctx.Back);
			d->Data->dsNot->Draw(&Buf, d->NotBtn.x2 + 3, d->NotBtn.y1);

			ShowControls(Select());
			
			if (!Select())
			{
				Buf.Colour(LC_WORKSPACE, 24);
				Buf.Rectangle(&d->FieldBtn);
				Buf.Rectangle(&d->OpBtn);
				Buf.Rectangle(&d->ValueBtn);

				int Tx = 6;
				int Ty = 2;

				if (d->Field)
				{
					GDisplayString ds(SysFont, d->Field);
					ds.Draw(&Buf, d->FieldBtn.x1+Tx, d->FieldBtn.y1+Ty, &d->FieldBtn);
				}
				if (d->Op >= 0)
				{
					GDisplayString ds(SysFont, d->Data->OpNames[d->Op]);
					ds.Draw(&Buf, d->OpBtn.x1+Tx, d->OpBtn.y1+Ty, &d->OpBtn);
				}
				if (d->Value)
				{
					GDisplayString ds(SysFont, d->Value);
					ds.Draw(&Buf, d->ValueBtn.x1+Tx, d->ValueBtn.y1+Ty, &d->ValueBtn);
				}
			}
			break;
		}
		case LNODE_NEW:
		{
			SysBold->Transparent(true);
			SysBold->Colour(BackCol, Workspace);
			d->Data->dsNew->Draw(&Buf, (int)r - 3, (int)(oy + r) - (SysBold->GetHeight() / 2));
			break;
		}
		case LNODE_AND:
		{
			SysBold->Transparent(true);
			SysBold->Colour(IconColour[d->Node], BackCol);
			d->Data->dsAnd->Draw(&Buf, (int)r - 3, (int)(oy + r) - (SysBold->GetHeight() / 2));
			break;
		}
		case LNODE_OR:
		{
			SysBold->Transparent(true);
			SysBold->Colour(IconColour[d->Node], BackCol);
			d->Data->dsOr->Draw(&Buf, (int)r - 3, (int)(oy + r) - (SysBold->GetHeight() / 2));
			break;
		}
	}

	// Blt result to the screen
	Ctx.pDC->Blt(Pos->x1, Pos->y1, &Buf);
	
	// Paint to the right of the item, filling the column.
	if (Pos->x2 < Ctx.x2)
	{
		Ctx.pDC->Colour(LC_WORKSPACE, 24);
		Ctx.pDC->Rectangle(Pos->x2 + 1, Ctx.y1, Ctx.x2, Ctx.y2);
	}
}

void GFilterItem::ShowControls(bool s)
{
	if (!GetTree() || d->Node != LNODE_COND)
		return;

	if (s)
	{
		GRect *Pos = _GetRect(TreeItemText);
		GRect c = GetTree()->GetClient();
		int n = 1;

		GRect Cbo = d->OpBtn;
		Cbo.Union(&d->OpDropBtn);

		StartCtrl(d->FieldBtn, FieldEd, d->Field, GEdit);
		StartCtrl(Cbo, OpCbo, 0, GCombo);
		StartCtrl(d->ValueBtn, ValueEd, d->Value, GEdit);
		
		if (d->OpCbo && !d->OpCbo->GetItems())
		{
			GArray<char*> Ops;
			if (d->Data->Callback(	(GFilterView*) GetTree(),
									this,
									FMENU_OP,
									d->OpBtn,
									&Ops,
									d->Data->CallbackData) > 0)
			{
				for (unsigned i=0; i<Ops.Length(); i++)
					d->OpCbo->Insert(Ops[i]);

				Ops.DeleteArrays();
			}

			if (d->Op)
				d->OpCbo->Value(d->Op);
		}
	}
	else
	{
		EndEdit(FieldBtn, FieldEd, Field);
		if (d->OpCbo)
		{
			d->Op = (int)d->OpCbo->Value();
			DeleteObj(d->OpCbo);
		}
		EndEdit(ValueBtn, ValueEd, Value);
	}
	
}

void GFilterItem::OnExpand(bool b)
{
	ShowControls(Select() && b);

	for (GTreeNode *c = GetChild(); c; c = c->GetNext())
	{
		GFilterItem *i = dynamic_cast<GFilterItem*>(c);
		if (i)
			i->OnExpand(b);
	}
}

bool GFilterItem::OnKey(GKey &k)
{
	if (k.IsChar)
	{
		if (k.vkey == ' ')
		{
			if (k.Down())
			{
				SetNode(LNODE_COND);
			}
			return true;
		}
	}
	else if (k.IsContextMenu())
	{
		if (k.Down())
		{
			OptionsMenu();
		}
		return true;
	}
	else if (k.vkey == VK_DELETE)
	{
		if (k.Down() && GetParent() && d->Node != LNODE_NEW)
		{
			GTreeItem *p = GetNext();
			if (!p) p = GetPrev();
			if (!p) p = GetParent();
			delete this;
			if (p)
				p->Select(true);
		}
		return true;
	}

	return false;
}

void GFilterItem::OnMouseClick(GMouse &m)
{
	if (m.Down() && m.Left())
	{
		GRect *Pos = _GetRect(TreeItemText);
		if (!Pos) return;

		int Hit = -1;
		for (int i=0; i<IconMax; i++)
		{
			if (d->Btns[i].Overlap(m.x - Pos->x1, m.y - Pos->y1))
			{
				Hit = i;
				break;
			}
		}

		GRect Rc, Client = GetTree()->GetClient();
		if (d->NotBtn.Overlap(m.x - Pos->x1, m.y - Pos->y1))
		{
			d->Not = !d->Not;
			Update();
		}

		if (d->Data->Callback)
		{
			if (d->FieldDropBtn.Overlap(m.x - Pos->x1, m.y - Pos->y1))
			{
				Rc = d->FieldDropBtn;
				Rc.Offset(Pos->x1 + Client.x1, Pos->y1 + Client.y1);
				d->Data->Callback(	(GFilterView*) GetTree(),
									this,
									FMENU_FIELD,
									Rc,
									0,
									d->Data->CallbackData);
			}
			else if (d->OpDropBtn.Overlap(m.x - Pos->x1, m.y - Pos->y1))
			{
				Rc = d->OpDropBtn;
				Rc.Offset(Pos->x1 + Client.x1, Pos->y1 + Client.y1);
				d->Data->Callback(	(GFilterView*) GetTree(),
									this,
									FMENU_OP,
									Rc,
									0,
									d->Data->CallbackData);
			}
		}

		int Delta = 1;
		switch (Hit)
		{
			case IconMoveUp:
			{
				Delta = -1;
				// Fall thru
			}
			case IconMoveDown:
			{
				GFilterItem *p = dynamic_cast<GFilterItem*>(GetParent());
				if (p)
				{
					GTreeItem *m = this;
					int Count = p->Items.Length();
					int Idx = p->Items.IndexOf(m);
					if (Idx + Delta >= 0 && Idx + Delta < Count - 1)
					{
						GTreeItem *i = p->Items[Idx + Delta];
						p->Items.Delete(m);
						p->Items.Insert(m, Idx + Delta);
						p->_RePour();
						GetTree()->Invalidate();
					}
				}
				break;
			}
			case IconNewCond:
			{
				SetNode(LNODE_COND);
				break;
			}
			case IconNewAnd:
			{
				SetNode(LNODE_AND);
				break;
			}
			case IconNewOr:
			{
				SetNode(LNODE_OR);
				break;
			}
			case IconDelete:
			{
				if (IsRoot() && GetTree())
				{
					GetTree()->Insert(new GFilterItem(d->Data));
				}

				delete this;
				break;
			}
			case IconOptions:
			{
				OptionsMenu();
				break;
			}
		}
	}
}

void GFilterItem::OptionsMenu()
{
	GRect *Pos = _GetRect(TreeItemText);
	if (!Pos) return;
	GRect Client = GetTree()->GetClient();

	GSubMenu s;
	if (d->Node == LNODE_NEW)
		s.AppendItem(LgiLoadString(L_FUI_CONDITION, "Condition"), 3, true);
	s.AppendItem(LgiLoadString(L_FUI_AND, "And"), 1, true);
	s.AppendItem(LgiLoadString(L_FUI_OR, "Or"), 2, true);

	GRect r = d->Btns[d->Node == LNODE_NEW ? IconNewCond : IconOptions];
	r.Offset(Pos->x1 + Client.x1, Pos->y1 + Client.y1);
	GdcPt2 p(r.x1, r.y2+1);
	GetTree()->PointToScreen(p);

	int Cmd = s.Float(GetTree(), p.x, p.y, true);
	switch (Cmd)
	{
		case 1:
			d->Node = LNODE_AND;
			break;
		case 2:
			d->Node = LNODE_OR;
			break;
		case 3:
			d->Node = LNODE_COND;
			break;
	}
	Update();
}

GFilterNode GFilterItem::GetNode()
{
	return d->Node;
}

void GFilterItem::SetNode(GFilterNode n)
{
	if (d->Node != n)
	{
		if (d->Node == LNODE_NEW &&
			GetParent() &&
			!IsRoot())
		{
			GetParent()->Insert(new GFilterItem(d->Data));
		}

		d->Node = n;
		Update();

		if (d->Node == LNODE_AND ||
			d->Node == LNODE_OR)
		{
			Insert(new GFilterItem(d->Data));
			Expanded(true);
		}
	}
}

///////////////////////////////////////////////////////////////////////////////
GFilterView::GFilterView(FilterUi_Menu Callback, void *Data)
{
	d = new GFilterViewPrivate(this);
	d->Callback = Callback;
	d->CallbackData = Data;
	d->Tree.Reset(new GFilterTree);
	Sunken(true);

	if (d->Callback)
	{
		GRect r;
		d->Callback(this,
					0,
					FMENU_OP,
					r,
					&d->OpNames,
					d->CallbackData);
	}

	d->Tree->Sunken(false);
	d->Tree->Insert(new GFilterItem(d));
	SetDefault();
}

GFilterView::~GFilterView()
{
	d->Tree->Empty();
	DelView(d->Tree);
	DeleteObj(d);
}

void GFilterView::OnCreate()
{
	OnPosChange();
	d->Tree->Attach(this);
}

void GFilterView::SetDefault()
{
	d->Tree->Empty();
	d->Tree->Insert(new GFilterItem(d));
}

GFilterItem *GFilterView::Create(GFilterNode Node)
{
	return new GFilterItem(d, Node);
}

void GFilterView::OnPosChange()
{
	GRect c = GLayout::GetClient();
	if (d->ShowLegend)
	{
		d->Info = c;
		d->Info.y2 = d->Info.y1 + SysFont->GetHeight() + 8;
		c.y1 += d->Info.y2 + 1;
	}

	d->Tree->SetPos(c);
}

/*
GRect &GFilterView::GetClient(bool ClientCoods)
{
	static GRect c;

	c = GView::GetClient(ClientCoods);
	if (d->ShowLegend)
	{
		d->Info = c;
		d->Info.y2 = d->Info.y1 + SysFont->GetHeight() + 8;
		c.y1 += d->Info.y2 + 1;
	}
	else
	{
		d->Info.ZOff(-1, -1);
	}
	
	return c;
}
*/

void GFilterView::OnPaint(GSurface *pDC)
{
	GLayout::OnPaint(pDC);

	if (d->ShowLegend)
	{
		GMemDC Buf(d->Info.X(), d->Info.Y(), System32BitColourSpace);

		Buf.Colour(LC_MED, 24);
		Buf.Rectangle(0, 0, Buf.X()-1, Buf.Y()-2);
		Buf.Colour(LC_LOW, 24);
		Buf.Line(0, Buf.Y()-1, Buf.X()-1, Buf.Y()-1);

		SysFont->Transparent(true);
		SysFont->Colour(LC_TEXT, LC_MED);
		SysBold->Transparent(true);
		SysBold->Colour(LC_TEXT, LC_MED);
		int x = 4, y = 4;

		d->dsLegend->Draw(&Buf, x, y);
		x += 8 + d->dsLegend->X();

		const char **Ico = GetIconNames();
		for (int i=0; i<IconMax; i++)
		{
			if (Ico[i])
			{
				Buf.Op(GDC_ALPHA);
				Buf.Blt(x, y, d->Icons[i]);
				x += 3 + d->Icons[i]->X();
				Buf.Op(GDC_SET);
				GDisplayString ds(SysFont, (char*)IconName[i]);
				ds.Draw(&Buf, x, y);
				x += 12 + ds.X();
			}
		}

		pDC->Blt(d->Info.x1, d->Info.y1, &Buf);
	}
}

GTreeNode *GFilterView::GetRootNode()
{
	return d->Tree;
}

void GFilterView::Empty()
{
	d->Tree->Empty();
}