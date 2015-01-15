/// \file
/// \author Matthew Allen (fret@memecode.com)
/// \brief A radio button for a list item

#ifndef __GLIST_ITEM_RADIO_H
#define __GLIST_ITEM_RADIO_H

/// A radio button control for use in a GListItem. It will select one option amongst many rows.
/// (Not one option amongst many columns)
class GListItemRadioBtn : public GListItemColumn
{
public:
	/// Constructor
	GListItemRadioBtn
	(
		/// The host list item.
		GListItem *host,
		/// The column to draw in.
		int column,
		/// The initial value.
		bool value = false
	) : GListItemColumn(host, column)
	{
		GListItemColumn::Value(value);
	}

	void OnPaintColumn(ItemPaintCtx &r, int i, GItemColumn *Col)
	{
		GSurface *pDC = r.pDC;
		GRect c(0, 0, 10, 10);
		c.Offset(r.x1 + ((r.X()-c.X())/2), r.y1 + ((r.Y()-c.Y())/2));

		// Box
		pDC->Colour(LC_WORKSPACE, 24);
		pDC->FilledCircle(c.x1 + 5, c.y1 + 5, 5);

		pDC->Colour(LC_TEXT, 24);
		pDC->Circle(c.x1 + 5, c.y1 + 5, 5);

		// Value
		if (Value())
		{
			pDC->Colour(LC_TEXT, 24);
			pDC->FilledCircle(c.x1 + 5, c.y1 + 5, 2);
		}
	}

	void OnMouseClick(GMouse &m)
	{
		if (m.Down() && m.Left())
		{
			Value(!Value());
		}
	}

	int64 Value()
	{
		return GListItemColumn::Value();
	}

	void Value(int64 i)
	{
		// Set me
		GListItemColumn::Value(i);

		// Switch off any other controls in the list...
		if (i &&
			GetList())
		{
			List<GListItem>::I Items = GetAllItems()->Start();
			for (GListItem *i=*Items; i; i=*++Items)
			{
				List<GListItemColumn>::I Cols = i->GetItemCols()->Start();
				for (GListItemColumn *c=*Cols; c; c=*++Cols)
				{
					if (c->GetColumn() == GetColumn())
					{
						GListItemRadioBtn *r = dynamic_cast<GListItemRadioBtn*>(c);
						if (r != this)
						{
							r->Value(0);
							break;
						}
					}
				}
			}

			GetList()->SendNotify(GITEM_NOTIFY_CHANGE);
		}
	}
};

#endif
