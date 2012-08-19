/**
 * \file
 * \author Matthew Allen
 * \date 27/7/1995
 * \brief Container classes.\n
	Copyright (C) 1995-2003, <a href="mailto:fret@memecode.com">Matthew Allen</a>
 */

#ifdef LINUX
#define _ISOC99_SOURCE		1
#include <wchar.h>
#endif

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <ctype.h>

#include "GMem.h"
#include "GContainers.h"
#include "GString.h"
#include "LgiCommon.h"

/////////////////////////////////////////////////////////////////////////////////////////////////

#if 0 && defined(_DEBUG)
#define AssertValid()			LgiAssert(IsValid())
#else
#define AssertValid()
#endif

#if 0
#define InvalidListError()		{ _asm { int 3 } return false; }
#else
#define InvalidListError()		{ LgiAssert(0); return false; }
#endif

#define ITEM_PTRS	64
class Item
{
public:
	DLinkList *List;
	Item *Next, *Prev;
	char Count;
	void *Ptr[ITEM_PTRS];

	Item(DLinkList *list)
	{
		LgiAssert(list);

		List = list;
		Next = Prev = 0;
		Count = 0;
		ZeroObj(Ptr);

		LgiAssert(List->FirstObj == 0);
		LgiAssert(List->LastObj == 0);
		List->FirstObj = List->LastObj = this;
	}

	Item(Item *item)
	{
		LgiAssert(item);

		Count = 0;
		ZeroObj(Ptr);
		List = item->List;
		Prev = item;
		if (Prev->Next)
		{
			Next = Prev->Next;
			Next->Prev = this;
		}
		else
		{
			Next = 0;
			if (List->LastObj == item)
			{
				List->LastObj = this;
			}
		}
		Prev->Next = this;
	}

	~Item()
	{
		if (Prev != 0 && Next != 0)
		{
			LgiAssert(List->FirstObj != this);
			LgiAssert(List->LastObj != this);
		}

		if (Prev)
		{
			Prev->Next = Next;
		}
		else if (List)
		{
			LgiAssert(List->FirstObj == this);
			List->FirstObj = Next;
		}

		if (Next)
		{
			Next->Prev = Prev;
		}
		else if (List)
		{
			LgiAssert(List->LastObj == this);
			List->LastObj = Prev;
		}
	}

	bool Delete(int &Index, Item *&Owner)
	{
		if (Index < Count-1)
		{
			memmove(Ptr+Index, Ptr+Index+1, (Count-Index-1) * sizeof(*Ptr));
		}

		if (--Count == 0)
		{
			// This Item is now empty, remove and reset current
			// into the next Item
			Owner = Next;
			Index = 0;
			delete this;
			return true;
		}
		else if (Index >= Count)
		{
			// Carry current item over to next Item
			Owner = Next;
			Index = 0;
		}
		
		return false;
	}

	bool Full()
	{
		return Count >= ITEM_PTRS;
	}

	void Insert(void *p, int Index = -1)
	{
		if (!Full())
		{
			if (Index < 0)
			{
				Index = Count;
			}
			else if (Index < Count)
			{
				memmove(Ptr+Index+1, Ptr+Index, (Count-Index) * sizeof(*Ptr));
			}
			Ptr[Index] = p;
			Count++;

			LgiAssert(Count <= ITEM_PTRS);
		}
		else
		{
			if (!Next)
			{
				new Item(this);
			}

			if (Next)
			{
				if (Index < 0)
				{
					Next->Insert(p, Index);
				}
				else
				{
					// Push last pointer into Next
					if (Next->Full())
					{
						// Create an empty Next
						new Item(this);
						if (Next)
						{
							Next->Insert(Ptr[ITEM_PTRS-1], 0);
						}
					}
					else
					{
						Next->Insert(Ptr[ITEM_PTRS-1], 0);
					}

					// Shulffle up points
					Count--;
					
					// Insert here
					Insert(p, Index);
				}
			}
		}
	}
};

class ItemIter
{
public:
	DLinkList *Lst;
	Item *i;
	int Cur;

	ItemIter(DLinkList *lst)
	{
		Lst = lst;
		i = 0;
		Cur = 0;
	}

	ItemIter(DLinkList *lst, Item *item, int c)
	{
		Lst = lst;
		i = item;
		Cur = c;
	}

	operator void*()
	{
		return (i && Cur>=0 && Cur<i->Count) ? i->Ptr[Cur] : 0;
	}
	
	ItemIter &operator =(Item *item)
	{
		i = item;
		return *this;
	}
	
	ItemIter &operator =(int c)
	{
		Cur = c;
		return *this;
	}

	ItemIter &operator =(ItemIter *iter)
	{
		Lst = iter->Lst;
		i = iter->i;
		Cur = iter->Cur;
		return *this;
	}

	int GetIndex(int Base)
	{
		if (i)
		{
			return Base + Cur;
		}

		return -1;
	}

	bool Next()
	{
		if (i)
		{
			Cur++;
			if (Cur >= i->Count)
			{
				i = i->Next;
				if (i)
				{
					Cur = 0;
					return i->Count > 0;
				}
			}
			else return true;
		}

		return false;
	}

	bool Prev()
	{
		if (i)
		{
			Cur--;
			if (Cur < 0)
			{
				i = i->Prev;
				if (i && i->Count > 0)
				{
					Cur = i->Count - 1;
					return true;
				}
			}
			else return true;
		}

		return false;
	}

	bool Delete()
	{
		if (i)
		{
			LgiAssert(Lst);
			i->Delete(Cur, i);
			return true;
		}

		return false;
	}
};

DLinkList::DLinkList()
{
	Items = 0;
	FirstObj = LastObj = 0;
	Cur = 0;
}

DLinkList::~DLinkList()
{
	Empty();
	DeleteObj(Cur);
}

DLinkList &DLinkList::operator =(DLinkList &lst)
{
	Empty();

	for (Item *p = lst.FirstObj; p; p = p->Next)
	{
		for (int i=0; i<p->Count; i++)
		{
			Insert(p->Ptr[i]);
		}
	}
	
	return *this;
}

bool DLinkList::IsValid()
{

	// Run down the list and verify objects
	{
		int Count = 0;

		for (Item *i = FirstObj; i; i = i->Next)
		{
			if (i->List != this)
			{
				InvalidListError();
			}

			if (i->Next)
			{
				if (i->Next->Prev != i)
				{
					InvalidListError();
				}
			}
			else
			{
				if (i != LastObj)
				{
					InvalidListError();
				}
			}

			if (i->Count == 0)
			{
				InvalidListError();
			}

			for (int n=0; n<i->Count; n++)
			{
				if (i->Ptr[n] == 0)
				{
					InvalidListError();
				}

				#if 0
				if (NOT LgiCanReadMemory(i->Ptr[n]))
				{
					InvalidListError();
				}
				#endif
			}

			Count += i->Count;
		}
		
		if (Items != Count)
		{
			InvalidListError();
		}
	}

	return true;
}

void DLinkList::Empty()
{
	AssertValid();

	DeleteObj(Cur);
	while (FirstObj)
	{
		Item *n = FirstObj->Next;
		DeleteObj(FirstObj);
		FirstObj = n;
	}
	Items = 0;

	AssertValid();
}

ItemIter DLinkList::GetIndex(int Index)
{
	ItemIter Iter(this);

	int n = 0;
	for (Item *i = FirstObj; i; i = i->Next)
	{
		if (Index >= n && Index < n + i->Count)
		{
			Iter = i;
			Iter = Index - n;
			break;
		}

		n += i->Count;
	}

	return Iter;
}

ItemIter DLinkList::GetPtr(void *Ptr, int &Base)
{
	ItemIter Iter(this);

	int n = 0;
	for (Item *i = FirstObj; i; i = i->Next)
	{
		for (int j=0; j<i->Count; j++)
		{
			if (i->Ptr[j] == Ptr)
			{
				Base = n;
				Iter = j;
				Iter = i;
				break;
			}
		}

		n += i->Count;
	}

	return Iter;
}

void *DLinkList::operator [](int Index)
{
	AssertValid();

	if (Cur)
	{
		return (*Cur = GetIndex(Index));
	}

	return 0;
}

int DLinkList::IndexOf(void *p)
{
	AssertValid();

	if (Cur)
	{
		int b;
		*Cur = GetPtr(p, b);
		return Cur->GetIndex(b);
	}

	return -1;
}

bool DLinkList::Delete()
{
	bool Status = false;

	AssertValid();

	if (Cur)
	{
		Cur->Delete();
		Items--;
		Status = true;

		if (Items == 0)
		{
			DeleteObj(Cur);
		}
	}

	AssertValid();

	return Status;
}

bool DLinkList::Delete(int Index)
{
	bool Status = false;

	AssertValid();

	if (Cur)
	{
		*Cur = GetIndex(Index);
		if (Status = Cur->Delete())
		{
			Items--;

			if (Items == 0)
			{
				DeleteObj(Cur);
			}
		}
	}

	AssertValid();

	return Status;
}

bool DLinkList::Delete(void *p)
{
	bool Status = false;

	AssertValid();

	if (Cur)
	{
		int b;
		*Cur = GetPtr(p, b);
		
		if (Status = Cur->Delete())
		{
			Items--;

			if (Items == 0)
			{
				DeleteObj(Cur);
			}
		}
	}

	AssertValid();

	return Status;
}

bool DLinkList::Insert(void *p, int Index)
{
	bool Status = false;

	#ifdef _DEBUG
	if ((NativeInt)p < 100)
	{
		static bool First = true;
		if (First)
		{
			LgiAssert(!"Null pointer insert.");
			First = false;
		}
		return false;
	}
	#endif

	AssertValid();

	LgiAssert(p);

	if (p)
	{
		if (Index >= 0 && Index < Items)
		{
			ItemIter Iter = GetIndex(Index);
			if (Iter.i)
			{
				Iter.i->Insert(p, Iter.Cur);
				Items++;
				Status = true;
			}
		}
		else
		{
			if (LastObj)
			{
				LastObj->Insert(p);
				Items++;
				Status = true;
			}
			else
			{
				new Item(this);
				if (FirstObj)
				{
					FirstObj->Insert(p);

					if (!Cur)
					{
						Cur = new ItemIter(this);
					}

					Items++;
					Status = true;
				}
			}
		}
	}

	AssertValid();

	return Status;
}

bool DLinkList::HasItem(void *p)
{
	AssertValid();

	if (Cur)
	{
		int b;
		*Cur = GetPtr(p, b);
		if ((void*)*Cur)
		{
			return true;
		}
	}

	return false;
}

void *DLinkList::ItemAt(int Index)
{
	AssertValid();

	if (Cur)
	{
		*Cur = GetIndex(Index);
		return *Cur;
	}

	return 0;
}

void *DLinkList::Current()
{
	AssertValid();

	if (Cur)
	{
		return *Cur;
	}

	return 0;
}

void *DLinkList::First()
{
	AssertValid();

	if (Cur)
	{
		*Cur = FirstObj;
		*Cur = 0;
		return *Cur;
	}

	return 0;
}

void *DLinkList::Last()
{
	AssertValid();

	if (Cur)
	{
		*Cur = LastObj;
		if (LastObj) *Cur = LastObj->Count-1;
		return *Cur;
	}

	return 0;
}

void *DLinkList::Next()
{
	if (Cur)
	{
		Cur->Next();
		return *Cur;
	}
	return 0;
}

void *DLinkList::Prev()
{
	if (Cur)
	{
		Cur->Prev();
		return *Cur;
	}
	return 0;
}

typedef int (*BTreeCompare)(void *a, void *b, NativeInt Data);
typedef void **AddrVoidPtr;

class BTreeNode
{
public:
	void *Node;
	BTreeNode *Left;
	BTreeNode *Right;

	AddrVoidPtr *Index(AddrVoidPtr *Items)
	{
		if (Left)
		{
			Items = Left->Index(Items);
		}

		**Items = Node;
		Items++;

		if (Right)
		{
			Items = Right->Index(Items);
		}

		return Items;
	}	
};

class BTree
{
	int Items;
	int Used;
	BTreeNode *Node;
	BTreeNode *Min;
	BTreeNode *Max;

public:
	BTree(int i)
	{
		Used = 0;
		Min = Max = 0;
		Node = new BTreeNode[Items = i];
		if (Node)
		{
			memset(Node, 0, Items * sizeof(BTreeNode));
		}
	}

	~BTree()
	{
		DeleteArray(Node);
	}

	int GetItems() { return Used; }

	bool Add(void *Obj, BTreeCompare Proc, NativeInt Data)
	{
		if (Used)
		{
			BTreeNode *Cur = Node;
			
			if (Used < Items)
			{
				if (Proc(Obj, Max->Node, Data) >= 0)
				{
					Max->Right = Node + Used++;
					Max = Max->Right;
					Max->Node = Obj;
					return true;
				}

				if (Proc(Obj, Min->Node, Data) < 0)
				{
					Min->Left = Node + Used++;
					Min = Min->Left;
					Min->Node = Obj;
					return true;
				}

				while (true)
				{
					int c = Proc(Obj, Cur->Node, Data);
					BTreeNode **Ptr = (c < 0) ? &Cur->Left : &Cur->Right;
					if (*Ptr)
					{
						Cur = *Ptr;
					}
					else if (Used < Items)
					{
						*Ptr = &Node[Used++];
						(*Ptr)->Node = Obj;
						return true;
					}
					else return false;
				}
			}
			else
			{
				LgiAssert(0);
			}

			return false;
		}
		else
		{
			Min = Max = Node;
			Node[Used++].Node = Obj;
			return true;
		}
	}

	void Index(void ***Items)
	{
		if (Node)
		{
			Node->Index(Items);
		}
	}
};

void DLinkList::Sort(ListSortFunc Compare, NativeInt Data)
{
	if (Items > 1)
	{
		BTree Tree(Items);
		AddrVoidPtr *iLst = new AddrVoidPtr[Items];
		if (iLst)
		{
			int n=0;
			Item *i = FirstObj;
			while (i)
			{
				for (int k=0; k<i->Count; k++)
				{
					iLst[n++] = i->Ptr + k;
					Tree.Add(i->Ptr[k], Compare, Data);
				}
				i = i->Next;
			}

			Tree.Index(iLst);
			DeleteArray(iLst);
		}
	}
}

////////////////////////////////////////////////////////////////////////////////////////////
DLinkIterator::DLinkIterator(DLinkList *list)
{
	List = list;
	Cur = new ItemIter(List);
}

DLinkIterator::DLinkIterator(const DLinkIterator &it)
{
	List = it.List;
	if (it.Cur)
		Cur = new ItemIter(List, it.Cur->i, it.Cur->Cur);
	else
		Cur = new ItemIter(List);
}

DLinkIterator::~DLinkIterator()
{
	List = 0;
	DeleteObj(Cur);
}

int DLinkIterator::Length()
{
	return List->Length();
}

DLinkIterator DLinkIterator::operator =(const DLinkIterator &it)
{
	DeleteObj(Cur);

	List = it.List;
	if (it.Cur)
		Cur = new ItemIter(List, it.Cur->i, it.Cur->Cur);
	else
		Cur = new ItemIter(List);

	return *this;
}

void *DLinkIterator::operator [](int Index)
{
	if (Cur)
	{
		return (*Cur = List->GetIndex(Index));
	}

	return 0;
}

void *DLinkIterator::Current()
{
	if (Cur)
	{
		return *Cur;
	}

	return 0;
}

void *DLinkIterator::First()
{
	if (Cur)
	{
		*Cur = List->FirstObj;
		*Cur = 0;
		return *Cur;
	}

	return 0;
}

void *DLinkIterator::Last()
{
	if (Cur)
	{
		*Cur = List->LastObj;
		if (List->LastObj) *Cur = List->LastObj->Count-1;
		return *Cur;
	}

	return 0;
}

void *DLinkIterator::Next()
{
	if (Cur)
	{
		Cur->Next();
		return *Cur;
	}
	return 0;
}

void *DLinkIterator::Prev()
{
	if (Cur)
	{
		Cur->Prev();
		return *Cur;
	}
	return 0;
}

int DLinkIterator::IndexOf(void *p)
{
	if (List)
	{
		int b;
		*Cur = List->GetPtr(p, b);
		if ((void*)*Cur)
		{
			return Cur->GetIndex(b);
		}
	}

	return -1;
}

bool DLinkIterator::HasItem(void *p)
{
	if (Cur)
	{
		int b;
		*Cur = List->GetPtr(p, b);
		if ((void*)*Cur)
		{
			return true;
		}
	}

	return false;
}

////////////////////////////////////////////////////////////////////////////////////////
GBytePipe::GBytePipe(int prealloc)
{
	PreAlloc = prealloc;
}

GBytePipe::~GBytePipe()
{
	Empty();
}

GBytePipe &GBytePipe::operator=(GBytePipe &p)
{
	Empty();

	PreAlloc = p.PreAlloc;
	for (Block *b = p.Mem.First(); b; b = p.Mem.Next())
	{
		Block *n = (Block*) malloc(sizeof(Block) + b->Size);
		if (n)
		{
			memcpy(n, b, sizeof(Block) + b->Size);
			Mem.Insert(n);
		}
		else break;		
	}

	return *this;
}

void GBytePipe::Empty()
{
	for (Block *b = Mem.First(); b; b = Mem.First())
	{
		Mem.Delete(b);
		free(b);
	}
}

int64 GBytePipe::GetSize()
{
	int Size = 0;
	for (Block *b = Mem.First(); b; b = Mem.Next())
	{
		Size += b->Used - b->Next;
	}
	return Size;
}

int GBytePipe::StringAt(char *Str)
{
	if (Str)
	{
		int Pos = 0;
		uint8 *UStr = (uint8*) Str;

		int SLen = strlen(Str);
		if (SLen > 0)
		{
			for (List<Block>::I Blocks = Mem.Start(); Blocks.In(); Blocks++)
			{
				Block *m = *Blocks;
				uint8 *Base = ((uint8*)m->Ptr()) + m->Next;
				uint8 *End = Base + (m->Used - m->Next);

				for (uint8 *Ptr = Base; Ptr < End; Ptr++)
				{
					if (*Ptr == *UStr)
					{
						// Check rest of string
						if (Ptr + SLen > End)
						{
							// Setup separate iterator to get to the next block
							List<Block>::I Match = Blocks;
							Block *Cur = *Match;
							if (Cur == m)
							{
								uint8 *b = Base;
								uint8 *e = End;
								uint8 *p = Ptr;
								bool m = true;

								for (int i=1; i<SLen; i++)
								{
									if (p + i >= e)
									{
										Match++;
										Cur = *Match;
										if (Cur)
										{
											b = ((uint8*)Cur->Ptr()) + Cur->Next;
											e = b + (Cur->Used - Cur->Next);
											p = b - i;
										}
										else break;
									}

									if (p[i] != UStr[i])
									{
										m = false;
										break;
									}
								}

								if (m)
								{
									return Pos + (Ptr - Base);
								}
							}
							else
							{
								LgiAssert(0);
							}
						}
						else
						{
							// String is entirely in this block...
							bool m = true;

							for (int i=1; i<SLen; i++)
							{
								if (Ptr[i] != UStr[i])
								{
									m = false;
									break;
								}
							}

							if (m)
							{
								return Pos + (Ptr - Base);
							}
						}
					}
				}

				Pos += m->Used - m->Next;
			}
		}
	}

	return -1;
}

int64 GBytePipe::Peek(GStreamI *Str, int Size)
{
	int64 Status = 0;

	if (Str && Size <= GetSize())
	{
		Block *b = 0;
		for (b = Mem.First(); b && Size > 0; b = Mem.Next())
		{
			int Copy = min(Size, b->Size - b->Next);
			if (Copy > 0)
			{
				Str->Write(b->Ptr() + b->Next, Copy);
				Size -= Copy;
				Status += Copy;
			}
		}
	}

	return Status;
}

int64 GBytePipe::Peek(uchar *Ptr, int Size)
{
	int64 Status = 0;

	if (Ptr && Size <= GetSize())
	{
		Block *b = 0;
		for (b = Mem.First(); b && Size > 0; b = Mem.Next())
		{
			int Copy = min(Size, b->Size - b->Next);
			if (Copy > 0)
			{
				memcpy(Ptr, b->Ptr() + b->Next, Copy);
				Ptr += Copy;
				Size -= Copy;
				Status += Copy;
			}
		}
	}

	return Status;
}

void *GBytePipe::New(int AddBytes)
{
	int64 Len = GetSize();
	uchar *Data = Len > 0 ? new uchar[Len+AddBytes] : 0;
	if (Data)
	{
		Read(Data, Len);
		if (AddBytes)
		{
			memset(Data+Len, 0, AddBytes);
		}
	}

	return Data;
}

int GBytePipe::Read(void *Ptr, int Size, int Flags)
{
	int Status = 0;

	if (Ptr && Size > 0)
	{
		Block *b = 0;
		for (b = Mem.First(); b && Size > 0; b = Mem.Next())
		{
			int Copy = min(Size, b->Used - b->Next);
			if (Copy > 0)
			{
				memcpy(Ptr, b->Ptr() + b->Next, Copy);
				((uchar*&)Ptr) += Copy;
				Size -= Copy;
				b->Next += Copy;
				Status += Copy;
			}
		}

		for (b = Mem.First(); b && b->Next >= b->Used; b = Mem.First())
		{
			Mem.Delete(b);
			free(b);
		}
	}

	return Status;
}

int GBytePipe::Pop(short &i)
{
	short n;
	if (Read((uchar*) &n, sizeof(n)))
	{
		i = n;
		return sizeof(n);
	}
	return 0;
}

int GBytePipe::Pop(int &i)
{
	int n;
	if (Read((uchar*) &n, sizeof(n)))
	{
		i = n;
		return sizeof(n);
	}
	return 0;
}

int GBytePipe::Pop(double &i)
{
	double n;
	if (Read((uchar*) &n, sizeof(n)))
	{
		i = n;
		return sizeof(n);
	}
	return 0;
}

int GBytePipe::Write(const void *Ptr, int Size, int Flags)
{
	int Status = 0;

	if (Ptr && Size > 0)
	{
		if (PreAlloc > 0)
		{
			Block *Last = Mem.Last();
			if (Last)
			{
				int Len = min(Size, Last->Size - Last->Used);
				if (Len > 0)
				{
					memcpy(Last->Ptr() + Last->Used, Ptr, Len);
					Last->Used += Len;
					Size -= Len;
					((uchar*&)Ptr) += Len;
					Status += Len;
				}
			}
		}

		if (Size > 0)
		{
			int Alloc = max(PreAlloc, Size);

			Block *b = (Block*) malloc(sizeof(Block) + Alloc);
			if (b)
			{
				void *p = b->Ptr();
				memcpy(p, Ptr, Size);
				b->Size = Alloc;
				b->Used = Size;
				b->Next = 0;
				Mem.Insert(b);
				Status += Size;
			}
		}
	}

	return Status;
}

//////////////////////////////////////////////////////////////////////////
int GStringPipe::Pop(char *Str, int BufSize)
{
	if (Str)
	{
		char *Start = Str;
		char *End = Str + BufSize - 2;
		Block *m;
		bool HasLf = false;

		for (m = Mem.First(); m; m = Mem.Next())
		{
			char *MPtr = (char*)m->Ptr();
			if (strnchr(MPtr + m->Next, '\n', m->Used - m->Next))
			{
				HasLf = true;
				break;
			}
		}

		if (HasLf)
		{
			m = Mem.First();
			while (m)
			{
				char *MPtr = (char*)m->Ptr();
				while (Str < End && m->Next < m->Used)
				{
					*Str = MPtr[m->Next++];
					if (*Str == '\n')
					{
						Str++;
						goto EndPop;
					}
					Str++;
				}

				if (m->Next >= m->Used)
				{
					Mem.Delete(m);
					free(m);
					m = Mem.First();
				}
				else if (Str >= End)
				{
					break;
				}
			}

			EndPop:
			*Str = 0;
			return Str - Start;
		}
	}

	return 0;
}

int GStringPipe::Push(const char *Str, int Len)
{
	if (Str)
	{
		return Write((void*)Str, (Len>=0) ? Len : strlen(Str));
	}
	return 0;
}

int GStringPipe::Push(const char16 *Str, int Len)
{
	if (Str)
	{
		return Write((void*)Str, ((Len>=0) ? Len : StrlenW(Str)) * sizeof(char16));
	}
	return 0;
}

/*
bool GStringPipe::Printf(char *Str, ...)
{
	bool Status = false;

	if (Str)
	{
		va_list Arg;
		va_start(Arg, Str);
		#ifdef WIN32
		// Stupid windows can't tell me how many bytes I need to
		// allocate for the buffer, so I just keep increasing the
		// size until it works.
		for (int i=256; i <= (1 << 20); i <<= 2)
		{
			char *Buf = new char[i];
			if (Buf)
			{
				int Len = vsnprintf(Buf, i, Str, Arg);
				if (Len >= 0)
				{
					Status = Push(Buf, Len);
					DeleteArray(Buf);
					break;
				}

				DeleteArray(Buf);
			}
		}
		#else
		int Len = vsnprintf(0, 0, Str, Arg);
		if (Len > 0)
		{
			char *Buf = new char[Len+1];
			if (Buf)
			{
				vsprintf(Buf, Str, Arg);
				Status = Push(Buf, Len);
				DeleteArray(Buf);
			}
		}
		else
		{
			LgiAssert(0);
		}
		#endif
		va_end(Arg);
	}

	return Status;
}

bool GStringPipe::Printf(char16 *Str, ...)
{
	bool Status = false;
	
	if (Str)
	{
		va_list Arg;
		va_start(Arg, Str);

		#if defined(LINUX) || defined(BEOS) || defined(MAC)	|| defined(__GNUC__)

			char *Format = LgiNewUtf16To8(Str);
			if (Format)
			{
				int Len = vsnprintf(0, 0, Format, Arg);
				if (Len > 0)
				{
					char *Buf = new char[Len+1];
					if (Buf)
					{
						vsprintf(Buf, Format, Arg);
						Status = Push(Buf, Len);
						DeleteArray(Buf);
					}
				}
				else
				{
					LgiAssert(0);
				}
				
				DeleteArray(Format);
			}
		
		#elif defined(WIN32)
		
			// Stupid windows can't tell me how many bytes I need to
			// allocate for the buffer, so I just keep increasing the
			// size until it works.
			for (int i=256; i <= (1 << 20); i <<= 2)
			{
				char16 *Buf = new char16[i];
				if (Buf)
				{
					int Len = vsnwprintf(Buf, i, Str, Arg);
					if (Len >= 0)
					{
						Status = Push(Buf, Len);
						DeleteArray(Buf);
						break;
					}

					DeleteArray(Buf);
				}
			}

		#else

			#error "Impl. Me"

		#endif

		va_end(Arg);
	}

	return Status;
}
*/

char *GStringPipe::NewStr()
{
	return (char*)New(1);
}

char16 *GStringPipe::NewStrW()
{
	return (char16*)New(sizeof(char16));
}

