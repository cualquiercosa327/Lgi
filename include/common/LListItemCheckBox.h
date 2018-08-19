/// \file
/// \author Matthew Allen (fret@memecode.com)
/// \brief A checkbox for a list item

#ifndef __LList_ITEM_CHECKBOX_H
#define __LList_ITEM_CHECKBOX_H

#include "LList.h"

/// A checkbox suitable for a LListItem.
class LListItemCheckBox : public LListItemColumn
{
public:
	/// Constructor
	LListItemCheckBox
	(
		/// The hosting list item.
		LListItem *host,
		/// The column to draw in.
		int column,
		// The initial value.
		bool value = false
	) : LListItemColumn(host, column)
	{
		Value(value);
	}

	void OnPaintColumn(GItem::ItemPaintCtx &Ctx, int i, GItemColumn *Col)
	{
		GSurface *pDC = Ctx.pDC;
		GRect c(0, 0, 10, 10);
		c.Offset(Ctx.x1 + ((Ctx.X()-c.X())/2), Ctx.y1 + ((Ctx.Y()-c.Y())/2));

		// Box
		pDC->Colour(LC_TEXT, 24);
		pDC->Box(&c);
		c.Size(1, 1);
		pDC->Colour(LC_WORKSPACE, 24);
		pDC->Rectangle(&c);

		// Value
		if (Value())
		{
			pDC->Colour(LC_TEXT, 24);

			pDC->Line(c.x1+1, c.y1+1, c.x2-1, c.y2-1);
			pDC->Line(c.x1+1, c.y1+2, c.x2-2, c.y2-1);
			pDC->Line(c.x1+2, c.y1+1, c.x2-1, c.y2-2);

			pDC->Line(c.x1+1, c.y2-1, c.x2-1, c.y1+1);
			pDC->Line(c.x1+1, c.y2-2, c.x2-2, c.y1+1);
			pDC->Line(c.x1+2, c.y2-1, c.x2-1, c.y1+2);
		}
	}

	void OnMouseClick(GMouse &m)
	{
		if (m.Down() && m.Left())
		{
			LList *l = GetList();
			List<LListItem> Sel;
			if (l->GetSelection(Sel) && Sel.First())
			{
				bool v = !Value();
				Sel.Delete(GetItem());

				for (LListItem *i=Sel.First(); i; i=Sel.Next())
				{
					LListItemColumn *c = GetItemCol(i, GetColumn());
					if (c)
					{
						c->Value(v);
					}
				}

				Value(v);
				GetItem()->Update();
			}
		}
	}
};

#endif
