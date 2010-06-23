#ifndef _GCTRLTREE_H_
#define _GCTRLTREE_H_

#include "GTree.h"
#include "GVariant.h"

class GControlTree : public GTree, public GDom
{
public:
	struct EnumValue
	{
		char *Name;
		GVariant Value;
	};

	class Item : public GTreeItem
	{
	public:
		typedef GArray<EnumValue> EnumArr;

	private:
		GAutoString Opt;
		GVariantType Type;
		GVariant Value;
		GViewI *Ctrl;
		GButton *Browse;
		GAutoPtr<EnumArr> Enum;

		void Save();

	public:
		enum ItemFlags
		{
			TYPE_FILE = 0x1,
		};
		int Flags;

		Item(char *Txt, char *opt, GVariantType type, GArray<EnumValue> *pEnum);
		~Item();

		Item *Find(char *opt);
		bool Serialize(GDom *Store, bool Write);
		void SetValue(GVariant &v);
		GRect &GetRect();
		void Select(bool b);
		void OnPaint(ItemPaintCtx &Ctx);
		void SetEnum(GAutoPtr<EnumArr> e) { Enum = e; }
		void OnVisible(bool v);
		void PositionControls();
	};

protected:
	class GControlTreePriv *d;

	class Item *Resolve(bool Create, char *Path, GVariantType Type = GV_NULL, GArray<EnumValue> *Enum = 0);

public:
	GControlTree();
	~GControlTree();

	Item *Find(char *opt);
	GTreeItem *Insert(char *DomPath, GVariantType Type, GVariant *Value = 0, GArray<EnumValue> *Enum = 0);
	bool SetVariant(char *Name, GVariant &Value, char *Array = 0);
	bool Serialize(GDom *Store, bool Write);
	int OnNotify(GViewI *c, int f);
};

#endif