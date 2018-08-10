#ifndef _LUNROLLED_LIST_H_
#define _LUNROLLED_LIST_H_

#include <algorithm>

#ifdef _DEBUG
	#define VALIDATE_UL() Validate()
#else
	#define VALIDATE_UL()
#endif

template<typename T, int BlockSize = 64>
class LUnrolledList
{
	struct LstBlk
	{
		LstBlk *Next, *Prev;
		int Count;
		T Obj[BlockSize];

		LstBlk()
		{
			Next = Prev = NULL;
			Count = 0;
		}

		~LstBlk()
		{
			for (size_t n=0; n<Count; n++)
			{
				Obj[n].~T();
			}
		}

		bool Full()
		{
			return Count >= BlockSize;
		}

		int Remaining()
		{
			return BlockSize - Count;
		}
	};

public:
	class Iter
	{
	public:
		LUnrolledList<T> *Lst;
		LstBlk *i;
		int Cur;

		Iter(LUnrolledList<T> *lst)
		{
			Lst = lst;
			i = 0;
			Cur = 0;
		}

		Iter(LUnrolledList<T> *lst, LstBlk *item, int c)
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

		T &operator *()
		{
			if (In())
				return i->Obj[Cur];

			LgiAssert(!"Invalid iterator.");			
			static T empty;
			return empty;
		}

		T *operator ->()
		{
			return In() ? &i->Obj[Cur] : NULL;
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
			LgiAssert(!"No Obj.");
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

	bool Insert(LstBlk *i, T p, int Index = -1)
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
			if (!Insert(i->Next, i->Obj[BlockSize-1], 0))
				return false;
			i->Count--;
			Items--; // We moved the item... not inserted it.

			// Fall through to the local "non-full" insert...
		}

		LgiAssert(!i->Full());
		if (Index < 0)
			Index = i->Count;
		else if (Index < i->Count)
			memmove(i->Obj+Index+1, i->Obj+Index, (i->Count-Index) * sizeof(p));
		i->Obj[Index] = p;
		i->Count++;
		Items++;

		LgiAssert(i->Count <= BlockSize);
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

	Iter GetPtr(T Obj, size_t *Base = NULL)
	{
		size_t n = 0;
		for (LstBlk *i = FirstObj; i; i = i->Next)
		{
			for (int k=0; k<i->Count; k++)
			{
				if (i->Obj[k] == Obj)
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

public:
	LUnrolledList<T,BlockSize>()
	{
		FirstObj = LastObj = NULL;
		Items = 0;
	}

	~LUnrolledList<T,BlockSize>()
	{
		VALIDATE_UL();
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
			
		VALIDATE_UL();
		
		bool Status = false;
		
		if (Len < Items)
		{
			// Decrease list size...
			size_t Base = 0;
			Iter i = GetIndex(Len, &Base);
			if (i.i)
			{
				size_t Offset = Len - Base;
				LgiAssert(Offset <= i.i->Count);
				i.i->Count = Len - Base;
				LgiAssert(i.i->Count >= 0 && i.i->Count < BlockSize);
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
				
		VALIDATE_UL();
		return Status;		
	}

	bool Empty()
	{
		VALIDATE_UL();

		LstBlk *n;
		for (LstBlk *i = FirstObj; i; i = n)
		{
			n = i->Next;
			delete i;
		}
		FirstObj = LastObj = NULL;
		Items = 0;

		VALIDATE_UL();
		return true;
	}

	bool DeleteAt(size_t i)
	{
		VALIDATE_UL();
		Iter p = GetIndex(i);
		if (!p.In())
			return false;
		bool Status = Delete(p);
		VALIDATE_UL();
		return Status;
	}

	bool Delete(Iter Pos)
	{
		if (!Pos.In())
			return false;

		int &Index = Pos.Cur;
		LstBlk *&i = Pos.i;
		if (Index < i->Count-1)
			memmove(i->Obj+Index, i->Obj+Index+1, (i->Count-Index-1) * sizeof(T*));

		Items--;
		if (--i->Count == 0)
		{
			// This Item is now empty, remove and reset current
			// into the next Item
			LstBlk *n = i->Next;
			bool Status = DeleteBlock(i);
			Pos.Cur = 0;
			Pos.i = n;

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

	bool Delete(T Obj)
	{
		VALIDATE_UL();
		auto It = GetPtr(Obj);
		if (!It.In())
			return false;
		bool Status = Delete(It);
		VALIDATE_UL();
		return Status;
	}

	T &New()
	{
		VALIDATE_UL();

		if (!FirstObj || LastObj->Count >= BlockSize)
			NewBlock(LastObj);

		if (LastObj->Count >= BlockSize)
		{
			LgiAssert(!"No block for new object.");
			static T empty;
			return empty;
		}

		T &Ref = LastObj->Obj[LastObj->Count++];
		Items++;
		
		VALIDATE_UL();
	
		return Ref;
	}

	bool Insert(T p, Iter &it)
	{
		if (it.Lst != this)
		{
			LgiAssert(!"Wrong list.");
			return false;
		}

		if (!LastObj)
			return Insert(p, -1);

		VALIDATE_UL();
		bool Status = Insert(it.i, p, it.Cur);
		VALIDATE_UL();
		return Status;
	}

	bool Insert(T p, int Index = -1)
	{
		VALIDATE_UL();
		if (!LastObj)
		{
			LstBlk *b = NewBlock(NULL);
			if (!b)
				return false;

			b->Obj[b->Count++] = p;
			Items++;
			VALIDATE_UL();
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
		VALIDATE_UL();
		LgiAssert(Status);
		return Status;
	}

	bool Add(T p)
	{
		return Insert(p);
	}
	
	T operator [](int Index)
	{
		VALIDATE_UL();
		auto i = GetIndex(Index);
		VALIDATE_UL();
		return i;
	}
	
	ssize_t IndexOf(T p)
	{
		VALIDATE_UL();
		size_t Base = -1;
		auto Local = GetPtr(p, &Base);
		LgiAssert(Base != -1);
		ssize_t Idx = Local.In() ? Base + Local.Cur : -1;
		VALIDATE_UL();
		return Idx;
	}

	bool HasItem(T p)
	{
		VALIDATE_UL();
		Iter Pos = GetPtr(p);
		bool Status = Pos.In();
		VALIDATE_UL();
		return Status;
	}

	T *ItemAt(int i)
	{
		VALIDATE_UL();
		auto Local = GetIndex(i);
		VALIDATE_UL();
		return Local;
	}

	void Compact()
	{
		LgiAssert(!"Impl me.");
	}

	void Swap(LUnrolledList<T> &other)
	{
		LSwap(Items, other.Items);
		LSwap(FirstObj, other.FirstObj);
		LSwap(LastObj, other.LastObj);		
	}

	class RandomAccessIter
	{
	public:
		typedef LUnrolledList<T,BlockSize> Unrolled;
		typedef RandomAccessIter It;
		typedef std::random_access_iterator_tag iterator_category;
		typedef T value_type;
		typedef ssize_t difference_type;
		typedef T *pointer;
		typedef T &reference;
	
	private:
		Unrolled *u;
		ssize_t Idx;
		int Shift;
		int Mask;
		struct BlkMap
		{
			int Count, Refs;
			LstBlk **Blocks;

			BlkMap(Unrolled *u)
			{
				Refs = 1;
				Count = (u->Items + (BlockSize-1)) / BlockSize;
				Blocks = new LstBlk*[Count];
				int Idx = 0;
				for (LstBlk *b=u->FirstObj; b; b = b->Next)
					Blocks[Idx++] = b;
			}

			~BlkMap()
			{
				delete [] Blocks;
			}

		}	*Map;		

		bool Init()
		{
			Mask = 0;
			for (Shift=0; Shift<32 && (1<<Shift) != BlockSize; Shift++);
			if (Shift >= 32)
			{
				LgiAssert(!"Not a power of 2 size");
				return false;
			}
			Mask = (1 << Shift) - 1;

			// Create array of block pointers
			if (u)
			{
				// Make sure there are no gaps in the arrays
				u->Compact();

				// Get a count of blocks and alloc index
				LgiAssert(Map == NULL);
				Map = new BlkMap(u);
			}

			return true;
		}

	public:
		RandomAccessIter(Unrolled *lst = NULL, ssize_t idx = 0)
		{
			u = lst;
			Idx = idx;
			Map = NULL;
			Init();
		}

		RandomAccessIter(const It &rhs)
		{
			u = rhs.u;
			Idx = rhs.Idx;
			Shift = rhs.Shift;
			Mask = rhs.Mask;
			Map = rhs.Map;
			if (Map)
				Map->Refs++;
		}

		~RandomAccessIter()
		{
			if (Map)
			{
				Map->Refs--;
				if (Map->Refs == 0)
				{
					delete Map;
					Map = 0;
				}
			}
		}

		bool IsValid() const
		{
			return	u != NULL &&
					Idx >= 0 && Idx < u->Items &&
					Shift != 0 &&
					Mask != 0 &&
					Map != NULL;
		}

		inline T& operator*() const
		{
			LgiAssert(IsValid());
			T &Obj = Map->Blocks[Idx>>Shift]->Obj[Idx&Mask];
			return Obj;
		}
		inline T* operator->() const
		{
			LgiAssert(IsValid());
			return &Map->Blocks[Idx>>Shift]->Obj[Idx&Mask];
		}
		inline T& operator[](difference_type rhs) const
		{
			LgiAssert(IsValid() && rhs >= 0 && rhs < u->Items);
			return Map->Blocks[rhs>>Shift]->Obj[rhs&Mask];
		}

		inline It& operator+=(difference_type rhs) {Idx += rhs; return *this;}
		inline It& operator-=(difference_type rhs) {Idx -= rhs; return *this;}
		inline It& operator++() {++Idx; return *this;}
		inline It& operator--() {--Idx; return *this;}
		inline It operator++(int) {It tmp(*this); ++Idx; return tmp;}
		inline It operator--(int) {It tmp(*this); --Idx; return tmp;}
		inline difference_type operator-(const It& rhs) const {return Idx-rhs.Idx;}
		inline It operator+(difference_type amt) {return It(u, Idx+amt);}
		inline It operator-(difference_type amt) {return It(u, Idx-amt);}
		friend inline It operator+(difference_type lhs, const It& rhs) {return It(lhs+rhs.Idx);}
		friend inline It operator-(difference_type lhs, const It& rhs) {return It(lhs-rhs.Idx);}
		inline bool operator==(const It& rhs) const {return Idx == rhs.Idx;}
		inline bool operator!=(const It& rhs) const {return Idx != rhs.Idx;}
		inline bool operator>(const It& rhs) const {return Idx > rhs.Idx;}
		inline bool operator<(const It& rhs) const {return Idx < rhs.Idx;}
		inline bool operator>=(const It& rhs) const {return Idx >= rhs.Idx;}
		inline bool operator<=(const It& rhs) const {return Idx <= rhs.Idx;}
	};

	// Sort with no extra user data
	template<typename Fn>
	void Sort(Fn Compare)
	{
		RandomAccessIter Start(this, 0);
		RandomAccessIter End(this, Items);
		std::sort
		(
			Start, End,
			[Compare](T &a, T &b)->bool
			{
				return Compare(a, b) < 0;
			}
		);
	}
	
	// Sort with extra user data
	template<typename Fn, typename User>
	void Sort(Fn Compare, User Data)
	{
		RandomAccessIter Start(this, 0);
		RandomAccessIter End(this, Items);
		std::sort
		(
			Start, End,
			[Compare, Data](T &a, T &b)->bool
			{
				return Compare(a, b, Data) < 0;
			}
		);
	}

	/// Assign the contents of another list to this one
	LUnrolledList<T,BlockSize> &operator =(const LUnrolledList<T,BlockSize> &lst)
	{
		VALIDATE_UL();

		// Make sure we have enough blocks allocated
		size_t i = 0;
		
		// Set the existing blocks to empty...
		for (LstBlk *out = FirstObj; out; out = out->Next)
		{
			out->Count = 0;
			i += BlockSize;
		}
		
		// If we don't have enough, add more...
		while (i < lst.Length())
		{
			LstBlk *out = NewBlock(LastObj);
			if (out)
				i += BlockSize;
			else
			{
				LgiAssert(!"Can't allocate enough blocks?");
				return *this;
			}
		}
		
		// If we have too many, free some...
		while (LastObj && i > lst.Length() + BlockSize)
		{
			DeleteBlock(LastObj);
			i -= BlockSize;
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
				memcpy(out->Obj + out->Count, in->Obj + pos, Cp * sizeof(T*));
				out->Count += Cp;
				pos += Cp;
				Items += Cp;
			}
		}

		VALIDATE_UL();

		return *this;
	}

	Iter begin(int At = 0) { return GetIndex(At); }
	Iter rbegin(int At = 0) { return GetIndex(Length()-1); }
	Iter end() { return Iter(this, NULL, -1); }

	bool Validate() const
	{
		if (FirstObj == NULL &&
			LastObj == NULL &&
			Items == 0)
			return true;

		size_t n = 0;
		LstBlk *Prev = NULL;
		for (LstBlk *i = FirstObj; i; i = i->Next)
		{
			for (int k=0; k<i->Count; k++)
			{
				n++;
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

		if (Items != n)
		{
			LgiAssert(!"Item count cache incorrect.");
			return false;
		}
		
		return true;
	}
};


#endif