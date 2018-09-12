/**
	\file
	\author Matthew Allen
	\date 27/11/1996
	\brief Container class header.\n
	Copyright (C) 1996-2003, <a href="mailto:fret@memecode.com">Matthew Allen</a>
	
 */

#ifndef _CONTAIN_H_
#define _CONTAIN_H_

#include "LgiInc.h"
#include "LgiOsDefs.h"
#include "GStream.h"

///	Template for using DLinkList with a type safe API.
#define ITEM_PTRS	64
LgiFunc bool UnitTest_ListClass();
#ifdef _DEBUG
	#define VALIDATE() Validate()
#else
	#define VALIDATE()
#endif

template<typename T>
class List
{
	struct LstBlk
	{
		LstBlk *Next, *Prev;
		uint8 Count;
		T *Ptr[ITEM_PTRS];

		LstBlk()
		{
			Next = Prev = NULL;
			Count = 0;
			ZeroObj(Ptr);
		}

		bool Full()
		{
			return Count >= ITEM_PTRS;
		}

		int Remaining()
		{
			return ITEM_PTRS - Count;
		}
	};

public:
	class Iter
	{
	public:
		List<T> *Lst;
		LstBlk *i;
		int Cur;

		Iter(List<T> *lst)
		{
			Lst = lst;
			i = 0;
			Cur = 0;
		}

		Iter(List<T> *lst, LstBlk *item, int c)
		{
			Lst = lst;
			i = item;
			Cur = c;
		}

		bool operator ==(const Iter &it) const
		{
			int x = (int)In() + (int)it.In();
			if (x == 2)
				return (i == it.i) && (Cur == it.Cur);
			return x == 0;
		}

		bool operator !=(const Iter &it) const
		{
			return !(*this == it);
		}

		bool In() const
		{
			return	i &&
					Cur >= 0 &&
					Cur < i->Count;
		}

		operator T*() const
		{
			return In() ? i->Ptr[Cur] : NULL;
		}

		T *operator *() const
		{
			return In() ? i->Ptr[Cur] : NULL;
		}
	
		Iter &operator =(LstBlk *item)
		{
			i = item;
			if (!i)
				Cur = 0;
			return *this;
		}
	
		Iter &operator =(int c)
		{
			Cur = c;
			return *this;
		}

		Iter &operator =(Iter *iter)
		{
			Lst = iter->Lst;
			i = iter->i;
			Cur = iter->Cur;
			return *this;
		}

		int GetIndex(int Base)
		{
			if (i)
				return Base + Cur;

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

		Iter &operator ++() { Next(); return *this; }
		Iter &operator --() { Prev(); return *this; }
		Iter &operator ++(int) { Next(); return *this; }
		Iter &operator --(int) { Prev(); return *this; }
	};

	typedef Iter I;
	// typedef int (*CompareFn)(T *a, T *b, NativeInt data);

protected:
	size_t Items;
	LstBlk *FirstObj, *LastObj;
	Iter Local;

	LstBlk *NewBlock(LstBlk *Where)
	{
		LstBlk *i = new LstBlk;
		LgiAssert(i != NULL);
		if (!i)
			return NULL;

		if (Where)
		{
			i->Prev = Where;
			if (i->Prev->Next)
			{
				// Insert
				i->Next = Where->Next;
				i->Prev->Next = i->Next->Prev = i;
			}
			else
			{
				// Append
				i->Prev->Next = i;
				LgiAssert(LastObj == Where);
				LastObj = i;
			}
		}
		else
		{
			// First object
			LgiAssert(FirstObj == 0);
			LgiAssert(LastObj == 0);
			FirstObj = LastObj = i;
		}

		return i;
	}

	bool DeleteBlock(LstBlk *i)
	{
		if (!i)
		{
			LgiAssert(!"No ptr.");
			return false;
		}

		if (i->Prev != 0 && i->Next != 0)
		{
			LgiAssert(FirstObj != i);
			LgiAssert(LastObj != i);
		}

		if (i->Prev)
		{
			i->Prev->Next = i->Next;
		}
		else
		{
			LgiAssert(FirstObj == i);
			FirstObj = i->Next;
		}

		if (i->Next)
		{
			i->Next->Prev = i->Prev;
		}
		else
		{
			LgiAssert(LastObj == i);
			LastObj = i->Prev;
		}

		delete i;

		return true;
	}

	bool Insert(LstBlk *i, T *p, ssize_t Index = -1)
	{
		if (!i)
			return false;

		if (i->Full())
		{
			if (!i->Next)
			{
				// Append a new LstBlk
				if (!NewBlock(i))
					return false;
			}

			if (Index < 0)
				return Insert(i->Next, p, Index);

			// Push last pointer into Next
			if (i->Next->Full())
				NewBlock(i); // Create an empty Next
			if (!Insert(i->Next, i->Ptr[ITEM_PTRS-1], 0))
				return false;
			i->Count--;
			Items--; // We moved the item... not inserted it.

			// Fall through to the local "non-full" insert...
		}

		LgiAssert(!i->Full());
		if (Index < 0)
			Index = i->Count;
		else if (Index < i->Count)
			memmove(i->Ptr+Index+1, i->Ptr+Index, (i->Count-Index) * sizeof(p));
		i->Ptr[Index] = p;
		i->Count++;
		Items++;

		LgiAssert(i->Count <= ITEM_PTRS);
		return true;
	}

	bool Delete(Iter &Pos)
	{
		if (!Pos.In())
			return false;

		int &Index = Pos.Cur;
		LstBlk *&i = Pos.i;
		if (Index < i->Count-1)
			memmove(i->Ptr+Index, i->Ptr+Index+1, (i->Count-Index-1) * sizeof(T*));

		Items--;
		if (--i->Count == 0)
		{
			// This Item is now empty, remove and reset current
			// into the next Item
			bool ClearLocal = i == Local.i;

			LstBlk *n = i->Next;
			bool Status = DeleteBlock(i);
			Pos.Cur = 0;
			Pos.i = n;

			if (ClearLocal)
				Local.i = NULL;
			return Status;
		}
		else if (Index >= i->Count)
		{
			// Carry current item over to next Item
			Pos.i = Pos.i->Next;
			Pos.Cur = 0;
		}
		
		return true;
	}

	Iter GetIndex(size_t Index, size_t *Base = NULL)
	{
		size_t n = 0;
		for (LstBlk *i = FirstObj; i; i = i->Next)
		{
			if (Index >= n && Index < n + i->Count)
			{
				if (Base)
					*Base = n;
				return Iter(this, i, (int) (Index - n));
			}
			n += i->Count;
		}

		if (Base)
			*Base = 0;
		return Iter(this);
	}

	Iter GetPtr(T *Ptr, size_t *Base = NULL)
	{
		size_t n = 0;
		for (LstBlk *i = FirstObj; i; i = i->Next)
		{
			for (int k=0; k<i->Count; k++)
			{
				if (i->Ptr[k] == Ptr)
				{
					if (Base)
						*Base = n;
					return Iter(this, i, k);
				}
			}
			n += i->Count;
		}

		if (Base)
			*Base = 0;
		return Iter(this);
	}

	class BTreeNode
	{
	public:
		T *Node;
		BTreeNode *Left;
		BTreeNode *Right;

		T ***Index(T ***Items)
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
		size_t Items;
		size_t Used;
		BTreeNode *Node;
		BTreeNode *Min;
		BTreeNode *Max;

	public:
		BTree(size_t i)
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

		template<typename User>
		bool Add(T *Obj, int (*Cmp)(T *a, T *b, User data), User Data)
		{
			if (Used)
			{
				BTreeNode *Cur = Node;
			
				if (Used < Items)
				{
					if (Cmp(Obj, Max->Node, Data) >= 0)
					{
						Max->Right = Node + Used++;
						Max = Max->Right;
						Max->Node = Obj;
						return true;
					}

					if (Cmp(Obj, Min->Node, Data) < 0)
					{
						Min->Left = Node + Used++;
						Min = Min->Left;
						Min->Node = Obj;
						return true;
					}

					while (true)
					{
						int c = Cmp(Obj, Cur->Node, Data);
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

		void Index(T ***Items)
		{
			if (Node)
			{
				Node->Index(Items);
			}
		}
	};


public:
	List<T>() : Local(this, NULL, 0)
	{
		FirstObj = LastObj = NULL;
		Items = 0;
	}

	~List<T>()
	{
		VALIDATE();
		Empty();
	}

	size_t Length() const
	{
		return Items;
	}
	
	bool Length(size_t Len)
	{
		if (Len == 0)
			return Empty();
		else if (Len == Items)
			return true;
			
		VALIDATE();
		
		bool Status = false;
		
		if (Len < Items)
		{
			// Decrease list size...
			size_t Base = 0;
			Iter i = GetIndex(Len, &Base);
			if (i.i)
			{
				size_t Offset = Len - Base;
				if (!(Offset <= i.i->Count))
				{
					LgiAssert(!"Offset error");
				}
				i.i->Count = (uint8) (Len - Base);
				LgiAssert(i.i->Count >= 0 && i.i->Count < ITEM_PTRS);
				while (i.i->Next)
				{
					DeleteBlock(i.i->Next);
				}
				Items = Len;
			}
			else LgiAssert(!"Iterator invalid.");
		}
		else
		{
			// Increase list size...
			LgiAssert(!"Impl me.");
		}
				
		VALIDATE();
		return Status;		
	}

	bool Empty()
	{
		VALIDATE();

		Local.i = NULL;

		LstBlk *n;
		for (LstBlk *i = FirstObj; i; i = n)
		{
			n = i->Next;
			delete i;
		}
		FirstObj = LastObj = NULL;
		Items = 0;

		VALIDATE();
		return true;
	}

	bool Delete()
	{
		VALIDATE();
		bool Status = Delete(Local);
		VALIDATE();
		return Status;
	}

	bool DeleteAt(size_t i)
	{
		VALIDATE();
		Iter p = GetIndex(i);
		if (!p.In())
			return false;
		bool Status = Delete(p);
		VALIDATE();
		return Status;
	}

	bool Delete(T *Ptr)
	{
		VALIDATE();
		Local = GetPtr(Ptr);
		if (!Local.In())
			return false;
		bool Status = Delete(Local);
		VALIDATE();
		return Status;
	}

	bool Insert(T *p, ssize_t Index = -1)
	{
		VALIDATE();
		if (!LastObj)
		{
			LstBlk *b = NewBlock(NULL);
			if (!b)
				return false;

			b->Ptr[b->Count++] = p;
			Items++;
			VALIDATE();
			return true;
		}

		bool Status;
		size_t Base;
		Iter Pos(this);

		if (Index < 0)
			Status = Insert(LastObj, p, Index);
		else
		{
			Pos = GetIndex(Index, &Base);
			if (Pos.i)
				Status = Insert(Pos.i, p, (int) (Index - Base));
			else
				Status = Insert(LastObj, p, -1);				
		}
		VALIDATE();
		LgiAssert(Status);
		return Status;
	}

	bool Add(T *p)
	{
		return Insert(p);
	}
	
	T *First()
	{
		VALIDATE();
		Local = FirstObj;
		Local.Cur = 0;
		VALIDATE();
		return Local;
	}

	T *Last()
	{
		VALIDATE();
		Local = LastObj;
		if (Local.i) Local.Cur = Local.i->Count - 1;
		VALIDATE();
		return Local;
	}

	T *Next()
	{
		return ++Local;
	}

	T *Prev()
	{
		return --Local;
	}

	T *Current() const
	{
		VALIDATE();
		return Local;
	}

	T *operator [](size_t Index)
	{
		VALIDATE();
		Local = GetIndex(Index);
		VALIDATE();
		return Local;
	}
	
	ssize_t IndexOf(T *p)
	{
		VALIDATE();
		size_t Base = -1;
		Local = GetPtr(p, &Base);
		LgiAssert(Base != -1);
		ssize_t Idx = Local.In() ? Base + Local.Cur : -1;
		VALIDATE();
		return Idx;
	}

	bool HasItem(T *p)
	{
		VALIDATE();
		Iter Pos = GetPtr(p);
		bool Status = Pos.In();
		VALIDATE();
		return Status;
	}

	T *ItemAt(ssize_t i)
	{
		VALIDATE();
		Local = GetIndex(i);
		VALIDATE();
		return Local;
	}

	/// Sorts the list
	template<typename User>
	void Sort
	(
		/// The callback function used to compare 2 pointers
		int (*Compare)(T *a, T *b, User data),
		/// User data that is passed into the callback
		User Data = 0
	)
	{
		if (Items < 1)
			return;

		VALIDATE();
		BTree Tree(Items);
		T ***iLst = new T**[Items];
		if (iLst)
		{
			int n=0;
			LstBlk *i = FirstObj;
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
			delete [] iLst;
		}
		VALIDATE();
	}

	/// Delete all pointers in the list as dynamically allocated objects
	void DeleteObjects()
	{
		VALIDATE();
		LstBlk *n;
		for (LstBlk *i = FirstObj; i; i = n)
		{
			n = i->Next;
			for (int n=0; n<i->Count; n++)
			{
				if (i->Ptr[n])
				{
					#ifdef _DEBUG
					size_t Objs = Items;
					#endif
					delete i->Ptr[n];
					#ifdef _DEBUG
					if (Objs != Items)
						LgiAssert(!"Do you have self deleting objects?");
					#endif
					i->Ptr[n] = NULL;
				}
			}
			delete i;
		}
		FirstObj = LastObj = NULL;
		Items = 0;
		Local.i = NULL;
		VALIDATE();
	}

	/// Delete all pointers in the list as dynamically allocated arrays
	void DeleteArrays()
	{
		VALIDATE();
		LstBlk *n;
		for (LstBlk *i = FirstObj; i; i = n)
		{
			n = i->Next;
			for (char n=0; n<i->Count; n++)
			{
				delete [] i->Ptr[n];
				i->Ptr[n] = NULL;
			}
		}
		FirstObj = LastObj = NULL;
		Items = 0;
		Local.i = NULL;
		VALIDATE();
	}

	void Swap(List<T> &other)
	{
		LSwap(FirstObj, other.FirstObj);
		LSwap(LastObj, other.LastObj);
		LSwap(Items, other.Items);
		LSwap(Local.Lst, other.Local.Lst);
		LSwap(Local.i, other.Local.i);
		LSwap(Local.Cur, other.Local.Cur);
	}

	/// Assign the contents of another list to this one
	#if 0
	List<T> &operator=(const List<T> &lst)
	{
		Empty();
		
		for (auto i : lst)
			Add(i);
			
		return *this;
	}
	#else
	List<T> &operator =(const List<T> &lst)
	{
		VALIDATE();

		// Make sure we have enough blocks allocated
		size_t i = 0;
		
		// Set the existing blocks to empty...
		for (LstBlk *out = FirstObj; out; out = out->Next)
		{
			out->Count = 0;
			i += ITEM_PTRS;
		}
		
		// If we don't have enough, add more...
		while (i < lst.Length())
		{
			LstBlk *out = NewBlock(LastObj);
			if (out)
				i += ITEM_PTRS;
			else
			{
				LgiAssert(!"Can't allocate enough blocks?");
				return *this;
			}
		}
		
		// If we have too many, free some...
		while (LastObj && i > lst.Length() + ITEM_PTRS)
		{
			DeleteBlock(LastObj);
			i -= ITEM_PTRS;
		}

		// Now copy over the block's contents.
		LstBlk *out = FirstObj;
		Items = 0;
		for (LstBlk *in = lst.FirstObj; in; in = in->Next)
		{
			for (int pos = 0; pos < in->Count; )
			{
				if (!out->Remaining())
				{
					out = out->Next;
					if (!out)
					{
						LgiAssert(!"We should have pre-allocated everything...");
						return *this;
					}
				}

				int Cp = MIN(out->Remaining(), in->Count - pos);
				LgiAssert(Cp > 0);
				memcpy(out->Ptr + out->Count, in->Ptr + pos, Cp * sizeof(T*));
				out->Count += Cp;
				pos += Cp;
				Items += Cp;
			}
		}

		VALIDATE();

		return *this;
	}
	#endif

	Iter begin(ssize_t At = 0) { return GetIndex(At); }
	Iter rbegin(ssize_t At = 0) { return GetIndex(Length()-1); }
	Iter end() { return Iter(this, NULL, -1); }

	bool Validate() const
	{
		if (FirstObj == NULL &&
			LastObj == NULL &&
			Items == 0)
			return true;

		size_t n = 0;
		LstBlk *Prev = NULL;
		bool SeenLocalBlk = false;
		for (LstBlk *i = FirstObj; i; i = i->Next)
		{
			if (Local.i == i)
				SeenLocalBlk = true;

			for (int k=0; k<i->Count; k++)
			{
				if (!i->Ptr[k])
				{
					LgiAssert(!"NULL pointer in LstBlk.");
					return false;
				}
				else
				{
					n++;
				}
			}

			if (i == FirstObj)
			{
				if (i->Prev)
				{
					LgiAssert(!"First object's 'Prev' should be NULL.");
					return false;
				}
			}
			else if (i == LastObj)
			{
				if (i->Next)
				{
					LgiAssert(!"Last object's 'Next' should be NULL.");
					return false;
				}
			}
			else
			{
				if (i->Prev != Prev)
				{
					LgiAssert(!"Middle LstBlk 'Prev' incorrect.");
					return false;
				}
			}

			Prev = i;
		}

		if (Local.i != NULL)
		{
			if (!SeenLocalBlk && Local.i != NULL)
			{
				LgiAssert(!"The local iterator is not present in the list.");
				return false;
			}
		}
		
		if (Items != n)
		{
			LgiAssert(!"Item count cache incorrect.");
			return false;
		}
		
		return true;
	}
};

/// \brief Data storage class.
///
/// Allows data to be buffered in separate memory
/// blocks and then written to a continuous block for processing. Works
/// as a first in first out que. You can (and should) provide a suitable
/// PreAlloc size to the constructor. This can reduce the number of blocks
/// of memory being used (and their associated alloc/free time and
/// tracking overhead) in high volume situations.
class LgiClass GMemQueue : public GStream
{
protected:
	/// Data block. These can contain a mix of 3 types of data:
	/// 1) Bytes that have been read (always at the start of the block)
	/// 2) Bytes that have been written (always in the middle)
	/// 3) Unwritten buffer space (always at the end)
	///
	/// Initially a block starts out as completely type 3 bytes, just garbage
	/// data waiting to be written to. Then something writes into the pipe and bytes
	/// are stored into this free space, and the 'Used' variable is set to show
	/// how much of the available buffer is used. At this point we have type 2 and
	/// type 3 bytes in the block. The buffer can fill up completely in which 
	/// case Used == Size and there are no type 3 bytes left. Also at some point
	/// something can start reading bytes out of the block which causes the 'Next'
	/// value to be increased, at which point the block starts with 'Next' bytes
	/// of type 1. Crystal?
	struct Block
	{
		/// Number of bytes in this block that have been read
		/// Type 1 or 'read' bytes are in [0,Next-1].
		int Next;
		/// Number of bytes that are used in this block include read bytes.
		/// Type 2 or 'used' bytes are in [Next,Used-1].
		int Used;
		/// Total size of the memory block
		/// Type 3 or 'unused' bytes are in [Used,Size-1].
		int Size;
		uint8 *Ptr() { return (uint8*) (this + 1); }

		Block()
		{
			Next = 0;
			Size = 0;
			Used = 0;
		}
	};

	int PreAlloc;
	List<Block> Mem;

public:
	/// Constructor
	GMemQueue
	(
		/// Sets the block size, which means allocating ahead and then joining
		/// together smaller inserts into 1 continuous block.
		int PreAlloc = 0
	);
	/// Destructor
	virtual ~GMemQueue();

	GMemQueue &operator =(GMemQueue &p);

	/// Empties the container freeing any memory used.
	virtual void Empty();
	
	/// Returns a dynamically allocated block that contains all the data in the container
	/// in a continuous block.
	virtual void *New
	(
		/// If this is > 0 then the function add's the specified number of bytes containing
		/// the value 0 on the end of the block. This is useful for NULL terminating strings
		/// or adding buffer space on the end of the block returned.
		int AddBytes = 0
	);
	
	/// Reads data from the start of the container without removing it from
	/// the que. Returns the bytes copied.
	virtual int64 Peek
	(
		/// Buffer for output
		uchar *Ptr,
		/// Bytes to look at
		int Size
	);

	/// Gets the total bytes in the container
	int64 GetSize() override;
	
	/// Reads bytes off the start of the container
	ssize_t Read(void *Buffer, ssize_t Size, int Flags = 0) override;
	
	/// Writes bytes to the end of the container
	ssize_t Write(const void *Buffer, ssize_t Size, int Flags = 0) override;
};

/// A version of GBytePipe for strings. Adds some special handling for strings.
class LgiClass GStringPipe : public GMemQueue
{
	ssize_t LineChars();
	ssize_t SaveToBuffer(char *Str, ssize_t BufSize, ssize_t Chars);

public:
	/// Constructs the object
	GStringPipe
	(
		/// Number of bytes to allocate per block.
		int PreAlloc = -1
	) : GMemQueue(PreAlloc) {}
	
	~GStringPipe() {}

	virtual ssize_t Pop(char *Str, ssize_t Chars);
	virtual GString Pop();
	virtual ssize_t Push(const char *Str, ssize_t Chars = -1);
	virtual ssize_t Push(const char16 *Str, ssize_t Chars = -1);
	char *NewStr() { return (char*)New(sizeof(char)); }
	GString NewGStr();
	char16 *NewStrW() { return (char16*)New(sizeof(char16)); }
	GStringPipe &operator +=(const GString &s) { Write(s.Get(), s.Length()); return *this; }

	#ifdef _DEBUG
	static bool UnitTest();
	#endif
};

#define GMEMFILE_BLOCKS		8
class LgiClass GMemFile : public GStream
{
	struct Block
	{
		size_t Offset;
		size_t Used;
		uint8 Data[1];
	};

	// The current file pointer
	uint64 CurPos;
	
	
	/// Number of blocks in use
	int Blocks;
	
	/// Data payload of blocks
	int BlockSize;

	/// Some blocks are built into the struct, no memory alloc needed.
	Block *Local[GMEMFILE_BLOCKS];
	
	// Buf if they run out we can alloc some more.
	GArray<Block*> Extra;

	Block *Get(int Index);
	bool FreeBlock(Block *b);
	Block *GetLast() { return Get(Blocks-1); }
	Block *Create();
	int CurBlock() { return (int) (CurPos / BlockSize); }

public:
	GMemFile(int BlkSize = 256);
	~GMemFile();
	
	void Empty();
	int64 GetSize() override;
	int64 SetSize(int64 Size) override;
	int64 GetPos() override;
	int64 SetPos(int64 Pos) override;
	ssize_t Read(void *Ptr, ssize_t Size, int Flags = 0) override;
	ssize_t Write(const void *Ptr, ssize_t Size, int Flags = 0) override;
};

#endif
