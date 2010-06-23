#ifndef __GCOMBO_H
#define __GCOMBO_H

// Combo Box
class LgiClass GCombo :
#if defined BEOS
	public BMenuField,
#endif
	public ResObject,
	public GControl,
	public List<char>
{
	#if defined WIN32
	static GWin32Class WndClass;
	char *Class() { return LGI_COMBO; }
	uint32 GetStyle() { return WS_VISIBLE | WS_VSCROLL | CBS_DISABLENOSCROLL | CBS_DROPDOWN | CBS_AUTOHSCROLL | WS_CHILD | WS_TABSTOP | ((SortItems) ? CBS_SORT : 0) | GView::GetStyle(); }
	int SysOnNotify(int Code);
	#elif defined BEOS
	BMenuItem *CurItem;
	void AttachedToWindow() { OnCreate(); }
	void MessageReceived(BMessage *message) { OnEvent(message); }
	#endif

	int InitIndex;
	bool SortItems;

public:
	GCombo(int id, int x, int y, int cx, int cy, char *name);
	~GCombo();

	bool Sort() { return SortItems; }
	void Sort(bool s) { SortItems = s; }
	void Value(int64 i);
	int64 Value();
	void Index(int i);
	int Index();
	#if defined BEOS
	char *Name();
	bool Name(char *n);
	#endif

	int OnEvent(GMessage *Msg);
	bool Delete();
	bool Delete(int i);
	bool Delete(char *p);
	bool Insert(char *p, int Index = -1);
	#if defined BEOS
	void OnCreate();
	#endif
};

#endif
