#ifndef _POPUP_LIST_H_
#define _POPUP_LIST_H_

#include "GList.h"
#include "GCss.h"
#include "GCssTools.h"

/// This class displays a list of search results in a 
/// popup connected to an editbox. To use override:
/// - ToString
/// - OnSelect
///
/// 'T' is the type of record that maps to one list item.
/// The user can select an item with [Enter] or click on it.
/// [Escape] cancels the popup.
template<typename T>
class GPopupList : public GPopup
{
public:
	enum {
		IDC_BROWSE_LIST = 50,
	};
	enum PositionType
	{
		PopupAbove,
		PopupBelow,
	};

	class Item : public GListItem
	{
	public:
		T *Value;
		
		Item(T *val = NULL)
		{
			Value = val;
		}
	};

protected:
	GList *Lst;
	GViewI *Edit;
	bool Registered;
	PositionType PosType;

public:
	GPopupList(GViewI *edit, PositionType pos, int width = 200, int height = 300) : GPopup(edit->GetGView())
	{
		Registered = false;
		PosType = pos;

		GRect r(width - 1, height - 1);
		Edit = edit;
		SetPos(r);
		AddView(Lst = new GList(IDC_BROWSE_LIST, r.x1+1, r.y1+1, r.X()-3, r.Y()-3));
		Lst->Sunken(false);
		Lst->AddColumn("Name", r.X());
		Lst->ShowColumnHeader(false);
		Lst->MultiSelect(false);
		
		// Set default border style...
		GCss *Css = GetCss(true);
		if (Css)
		{
			GCss::BorderDef b(Css, "1px solid #888;");
			Css->Border(b);
		}

		Attach(edit);
	}
	
	~GPopupList()
	{
		if (GetWindow() && Registered)
			GetWindow()->UnregisterHook(this);
	}

	// Events:
	// ------------------------------------------------------------------------
	
	/// Override this to convert an object to a string for the list items
	virtual GString ToString(T *Obj) = 0;
	
	/// Override this to handle the selection of an object
	virtual void OnSelect(T *Obj) = 0;
	
	// Implementation:
	// ------------------------------------------------------------------------	
	void SetPosType(PositionType t)
	{
		PosType = t;
	}
	
	bool SetItems(GArray<T*> &a)
	{
		Lst->Empty();
		
		for (unsigned i=0; i<a.Length(); i++)
		{
			T *obj = a[i];
			if (obj)
			{
				Item *li = new Item(obj);
				if (li)
				{
					GString s = ToString(obj);
					if (s)
					{
						li->SetText(s);
						Lst->Insert(li);
					}
					else
					{
						LgiAssert(!"ToString failed.");
						return false;
					}
				}
				else LgiAssert(!"Alloc failed.");
			}
			else LgiAssert(!"Null array entry.");
		}
		
		return true;
	}
	
	void OnPaint(GSurface *pDC)
	{
		// Draw the CSS border... (the default value is set in the constructor)
		GCssTools t(GetCss(true), GetFont());
		GRect c = GetClient();
		c = t.PaintBorder(pDC, c);
		
		// Move Lst if needed...
		GRect r = Lst->GetPos();
		if (r != c)
			Lst->SetPos(c);
	}
		
	int OnNotify(GViewI *Ctrl, int Flags)
	{
		if (Lst &&
			Ctrl == Edit &&
			!Flags)
		{
			char *Str = Edit->Name();
			Name(Str);

			bool Has = ValidStr(Str) && Lst->Length();
			bool Vis = Visible();
			if (Has ^ Vis)
			{
				// Set position relative to editbox
				GRect r = GetPos();
				GdcPt2 p(0, PosType == PopupAbove ? 0 : Edit->Y());
				Edit->PointToScreen(p);
				if (PosType == PopupAbove)
					r.Offset(p.x - r.x1, (p.y - r.Y()) - r.y1);
				else
					r.Offset(p.x - r.x1, p.y - r.y1);
				SetPos(r);				

				Visible(Has);
				if (Has)
				{
					AttachChildren();
					OnPosChange();
					
					if (GetWindow() && !Registered)
					{
						Registered = true;
						GetWindow()->RegisterHook(this, GKeyEvents);
					}

					Edit->Focus(true);
				}
			}
		}
		else if (Ctrl == Lst)
		{
			if (Flags == GNotify_ReturnKey ||
				Flags == GNotifyItem_Click)
			{
				Item *Sel = dynamic_cast<Item*>(Lst->GetSelected());
				if (Sel)
					OnSelect(Sel->Value);
				
				Visible(false);
			}
		}
		
		return 0;
	}

	bool OnViewKey(GView *v, GKey &k)
	{
		if (Visible())
		{
			switch (k.vkey)
			{
				case VK_TAB:
				{
					Visible(false);
					return false;
				}
				case VK_ESCAPE:
				{
					if (!k.Down())
					{
						Visible(false);
					}
					return true;
					break;
				}
				case VK_RETURN:
				{
					if (Lst)
						Lst->OnKey(k);
					return true;
					break;
				}
				case VK_UP:
				case VK_DOWN:
				{
					if (!k.IsChar)
					{
						if (Lst)
							Lst->OnKey(k);
						
						return true;
					}
					break;
				}
			}
		}

		return false;	
	}
};

#endif