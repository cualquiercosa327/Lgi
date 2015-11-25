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
				if (!LgiCanReadMemory(i->Ptr[n]))
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
		if ((Status = (Cur->Delete())))
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
		
		if ((Status = Cur->Delete()))
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
	if ((UNativeInt)p < 100)
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
GMemQueue::GMemQueue(int prealloc)
{
	PreAlloc = prealloc;
}

GMemQueue::~GMemQueue()
{
	Empty();
}

GMemQueue &GMemQueue::operator=(GMemQueue &p)
{
	Empty();

	PreAlloc = p.PreAlloc;
	for (Block *b = p.Mem.First(); b; b = p.Mem.Next())
	{
		int Alloc = sizeof(Block) + b->Size;
		Alloc = LGI_ALLOC_ALIGN(Alloc);
		Block *n = (Block*) malloc(Alloc);
		if (n)
		{
			memcpy(n, b, sizeof(Block) + b->Size);
			Mem.Insert(n);
		}
		else break;		
	}

	return *this;
}

void GMemQueue::Empty()
{
	for (Block *b = Mem.First(); b; b = Mem.First())
	{
		Mem.Delete(b);
		free(b);
	}
}

int64 GMemQueue::GetSize()
{
	int Size = 0;
	for (Block *b = Mem.First(); b; b = Mem.Next())
	{
		Size += b->Used - b->Next;
	}
	return Size;
}

#if 0
int GMemQueue::Find(char *Str)
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

int64 GMemQueue::Peek(GStreamI *Str, int Size)
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

int GMemQueue::Pop(short &i)
{
	short n;
	if (Read((uchar*) &n, sizeof(n)))
	{
		i = n;
		return sizeof(n);
	}
	return 0;
}

int GMemQueue::Pop(int &i)
{
	int n;
	if (Read((uchar*) &n, sizeof(n)))
	{
		i = n;
		return sizeof(n);
	}
	return 0;
}

int GMemQueue::Pop(double &i)
{
	double n;
	if (Read((uchar*) &n, sizeof(n)))
	{
		i = n;
		return sizeof(n);
	}
	return 0;
}

#endif

int64 GMemQueue::Peek(uchar *Ptr, int Size)
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

void *GMemQueue::New(int AddBytes)
{
	int64 Len = GetSize();
	uchar *Data = Len > 0 ? new uchar[Len+AddBytes] : 0;
	if (Data)
	{
		int Rd = Read(Data, Len);
		if (Rd >= 0 && AddBytes)
		{
			memset(Data+Len, 0, AddBytes);
		}
	}

	return Data;
}

int GMemQueue::Read(void *Ptr, int Size, int Flags)
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

int GMemQueue::Write(const void *Ptr, int Size, int Flags)
{
	int Status = 0;

	/*
	char m[256];
	sprintf_s(m, sizeof(m), "%p::Write(%p, %i, %i)\n", this, Ptr, Size, Flags);
	OutputDebugStringA(m);
	*/

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
			int Bytes = max(PreAlloc, Size);
			int Alloc = sizeof(Block) + Bytes;
			Alloc = LGI_ALLOC_ALIGN(Alloc);
			Block *b = (Block*) malloc(Alloc);
			if (b)
			{
				void *p = b->Ptr();
				memcpy(p, Ptr, Size);
				b->Size = Bytes;
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

int GStringPipe::Push(const char *Str, int Chars)
{
	if (!Str)
		return 0;

	if (Chars < 0)
		Chars = strlen(Str);

	return Write((void*)Str, Chars);
}

int GStringPipe::Push(const char16 *Str, int Chars)
{
	if (!Str)
		return 0;

	if (Chars < 0)
		Chars = StrlenW(Str);

	return Write((void*)Str, Chars * sizeof(char16));
}

//////////////////////////////////////////////////////////////////////////////////////////////////
GMemFile::GMemFile(int BlkSize)
{
	CurPos = 0;
	Blocks = 0;
	BlockSize = BlkSize < 16 ? 16 : BlkSize;
	ZeroObj(Local);
}

GMemFile::~GMemFile()
{
	Empty();
}

GMemFile::Block *GMemFile::Get(int Index)
{
	if (Blocks == 0 || Index < 0)
		return NULL;

	if (Index < GMEMFILE_BLOCKS)
		return Local[Index];
	
	if (Index - GMEMFILE_BLOCKS >= Extra.Length())
	{
		LgiAssert(!"Index beyond last block");
		return NULL;
	}

	return Extra[Index - GMEMFILE_BLOCKS];
}

GMemFile::Block *GMemFile::Create()
{
	int Alloc = sizeof(Block) + BlockSize - 1;
	Alloc = LGI_ALLOC_ALIGN(Alloc);
	Block *b = (Block*) malloc(Alloc);
	if (!b)
		return NULL;
	
	b->Used = 0;
	b->Offset = Blocks * BlockSize;
	
	if (Blocks < GMEMFILE_BLOCKS)
		Local[Blocks] = b;
	else
		Extra.Add(b);

	Blocks++;
	return b;
}

void GMemFile::Empty()
{
	CurPos = 0;
	for (int i=0; i<GMEMFILE_BLOCKS; i++)
		free(Local[i]);
	for (int i=0; i<Extra.Length(); i++)
		free(Extra[i]);
}

int64 GMemFile::GetSize()
{
	if (this == NULL)
		return -1;
	if (Blocks == 0)
		return 0;

	Block *b = GetLast();
	int64 Sz = (Blocks - 1) * BlockSize;
	return Sz + b->Used;	
}

bool GMemFile::FreeBlock(Block *b)
{
	if (!b)
		return false;
	
	int Idx = b->Offset / BlockSize;
	if (Idx < GMEMFILE_BLOCKS)
	{
		// Local block
		if (Local[Idx] != b ||
			Idx < Blocks-1)
		{
			LgiAssert(!"Block index error.");
			return false;
		}
		
		free(b);
		Local[Idx] = NULL;		
		Blocks--;
		return true;
	}

	// Extra block
	int Off = Idx - GMEMFILE_BLOCKS;
	if (Off != Extra.Length() - 1)
	{
		LgiAssert(!"Block index error.");
		return false;
	}
	
	free(b);
	Extra.DeleteAt(Off, true);
	Blocks--;
	return true;
}

int64 GMemFile::SetSize(int64 Size)
{
	if (Size <= 0)
	{
		Empty();
	}
	else
	{
		int64 CurSize = GetSize();
		if (Size > CurSize)
		{
			// Increase size...
			int64 Diff = Size - CurSize;
			Block *b = GetLast();
			if (b->Used < BlockSize)
			{
				// Add size to last incomplete block
				int Remaining = BlockSize - b->Used;
				int Add = min(Diff, Remaining);
				b->Used += Add;
				Diff -= Add;
			}
			while (Diff > 0)
			{
				// Add new blocks to cover the size...
				int Add = min(BlockSize, Diff);
				b = Create();
				b->Used = Add;
				Diff -= Add;
			}
		}
		else
		{
			// Decrease size...
			uint64 Diff = CurSize - Size;
			while (Diff > 0 && Blocks > 0)
			{
				Block *b = GetLast();
				if (!b)
					break;

				int Sub = min(b->Used, Diff);
				b->Used -= Sub;
				Diff -= Sub;
				if (b->Used == 0)
					FreeBlock(b);
			}
		}
	}
	
	return GetSize();
}

int64 GMemFile::GetPos()
{
	return CurPos;
}

int64 GMemFile::SetPos(int64 Pos)
{
	if (Pos <= 0)
		return CurPos = 0; // Off the start of the structure
	
	int BlockIndex = Pos / BlockSize;
	if (BlockIndex >= Blocks)
		return CurPos = GetSize(); // Off the end of the structure
	
	if (BlockIndex >= 0 && BlockIndex < Blocks - 1)
		return CurPos = Pos; // Inside a full block
	
	Block *Last = GetLast();
	uint64 Offset = Pos - Last->Offset;
	if (Offset >= Last->Used)
		return CurPos = Last->Offset + Last->Used; // End of last block
	
	return CurPos = Pos; // Inside the last block
}

int GMemFile::Read(void *Ptr, int Size, int Flags)
{
	if (!Ptr || Size < 1)
		return 0;
	
	uint8 *p = (uint8*) Ptr;
	uint8 *end = p + Size;
	while (p < end)
	{
		int Cur = CurBlock();
		if (Cur >= Blocks)
			break;
		Block *b = Get(Cur);
		
		// Where are we in the current block?
		int BlkOffset = CurPos - b->Offset;
		LgiAssert(b && BlkOffset >= 0 && BlkOffset <= b->Used);
		int Remaining = b->Used - BlkOffset;
		if (Remaining > 0)
		{
			int Common = min(Remaining, end - p);
			memcpy(p, b->Data + BlkOffset, Common);
			CurPos += Common;
			p += Common;
		}
		else break;
		
		LgiAssert(p <= end);
		if (p >= end)	// End of read buffer reached?
			break;		// Exit loop
	}	
	
	return p - (uint8*) Ptr;
}

int GMemFile::Write(const void *Ptr, int Size, int Flags)
{
	if (!Ptr || Size < 1)
		return 0;

	uint8 *p = (uint8*) Ptr;
	int len = Size;
	
	Block *b = GetLast();
	if (b && b->Used < BlockSize)
	{
		// Any more space in the last block?
		int Remaining = BlockSize - b->Used;
		int Common = min(Remaining, Size);
		if (Common > 0)
		{
			memcpy(b->Data + b->Used, p, Common);
			p += Common;
			len -= Common;
			b->Used += Common;
		}
	}

	// Store remaining data into new blocks		
	while (len > 0)
	{
		b = Create();
		if (!b)
			break;
		
		int Common = min(BlockSize, len);
		memcpy(b->Data, p, Common);
		b->Used = Common;
		p += Common;
		len -= Common;
	}
	
	return Size - len;
}
