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

class DLinkList;
class DLinkInterator;

/// General linked list type class that stores void pointers.
typedef int (*ListSortFunc)(void*, void*, NativeInt);

class LgiClass DLinkList
{
	friend class DLinkIterator;
	friend class Item;
	friend class ItemIter;

protected:
	int Items;
	class Item *FirstObj, *LastObj;
	class ItemIter *Cur;

	ItemIter GetIndex(int Index);
	ItemIter GetPtr(void *Ptr, int &Base);

public:
	DLinkList();
	virtual ~DLinkList();

	bool IsValid();

	int Length() { return Items; }
	virtual void Empty();
	void *operator [](int Index);
	DLinkList &operator =(DLinkList &lst);

	bool Delete();
	bool Delete(int Index);
	bool Delete(void *p);
	bool Insert(void *p, int Index = -1);
	void *Current();
	void *First();
	void *Last();
	void *Next();
	void *Prev();
	int IndexOf(void *p);
	bool HasItem(void *p);
	void *ItemAt(int i);
	void Sort(ListSortFunc Compare, NativeInt Data);
};

///	Linked list type interator for void pointers.
class LgiClass DLinkIterator
{
protected:
	DLinkList *List;
	class ItemIter *Cur;

public:
	DLinkIterator(DLinkList *list);
	DLinkIterator(const DLinkIterator &it);
	~DLinkIterator();

	void *operator [](int Index);
	DLinkIterator operator =(const DLinkIterator &it);
	void *Current();
	void *First();
	void *Last();
	void *Next();
	void *Prev();
	bool HasItem(void *p);
	int IndexOf(void *p);
	int Length();
};

///	Template for using DLinkList with a type safe API.
template <class Type>
class List : public DLinkList
{
public:
	/// Deletes the current item
	virtual bool Delete()			{ return DLinkList::Delete(); }
	/// Deletes the item at position 'i'
	virtual bool Delete(int i)		{ return DLinkList::Delete(i); }
	/// Deletes the pointer 'p'
	virtual bool Delete(Type *p)	{ return DLinkList::Delete((void*)p); }
	/// Inserts a pointer
	virtual bool Insert
	(
		/// The pointer to insert
		Type *p,
		/// The index to insert at or -1 to insert at the end
		int Index = -1
	)
	{ return DLinkList::Insert((void*)p, Index); }
	/// Adds an item on the end of the list.
	bool Add(Type *p) { return Insert(p); }
	
	/// Return the first pointer
	Type *First()					{ return (Type*) DLinkList::First(); }
	/// Return the last pointer
	Type *Last()					{ return (Type*) DLinkList::Last(); }
	/// Return the pointer after the current one
	Type *Next()					{ return (Type*) DLinkList::Next(); }
	/// Return the pointer before the current one
	Type *Prev()					{ return (Type*) DLinkList::Prev(); }
	/// Return the current pointer
	Type *Current()					{ return (Type*) DLinkList::Current(); }
	/// Return the pointer at an index
	Type *operator [](int Index)	{ return ((Type*) ((*((DLinkList*)this))[Index])); }
	
	/// Return the index of a pointer or -1 if it's not in the list
	int IndexOf(Type *p)			{ return DLinkList::IndexOf((void*)p); }
	/// Return the TRUE if the pointer is in the list
	bool HasItem(Type *p)			{ return DLinkList::HasItem((void*)p); }
	/// Return the pointer at index 'i'
	Type *ItemAt(int i)				{ return (Type*) DLinkList::ItemAt(i); }
	/// Sorts the list
	void Sort
	(
		/// The callback function used to compare 2 pointers
		int (*Compare)(Type *a, Type *b, NativeInt data),
		/// User data that is passed into the callback
		NativeInt Data
	)
	{ DLinkList::Sort( (int (*)(void *a, void *b, NativeInt data)) Compare, Data); }
	
	/// Delete all pointers in the list as dynamically allocated objects
	void DeleteObjects()
	{
		for (Type *o=(Type*)DLinkList::First(); o; o=(Type*)DLinkList::Next())
		{
			DeleteObj(o);
		}
		DLinkList::Empty();
	}
	/// Delete all pointers in the list as dynamically allocated arrays
	void DeleteArrays()
	{
		for (Type *o=(Type*)DLinkList::First(); o; o=(Type*)DLinkList::Next())
		{
			delete [] o;
		}
		DLinkList::Empty();
	}

	/// Assign the contents of another list to this one
	List &operator =
	(
		/// The source list.
		List<Type> &lst
	)
	{
		DLinkList *l = this;
		*l = (DLinkList&)lst;
		return *this;
	}

	// Built in iterators
	template <class T>
	class Iter : public DLinkIterator
	{
		int8 each_dir;

	public:
		Iter(DLinkList *l, int At) : DLinkIterator(l)
		{
			if (At < 0) Last();
			else if (At) (*this)[At];
			else First();
			each_dir = 0;
		}

		Iter(const Iter<T> &it) : DLinkIterator(it)
		{
			each_dir = it.each_dir;
		}

		operator bool() { return In(); }
		bool In() { return Current() != 0; }
		bool End() { return Current() == 0; }
		Iter<T> &operator ++() { Next(); return *this; }
		Iter<T> &operator --() { Prev(); return *this; }
		Iter<T> &operator ++(int) { Next(); return *this; }
		Iter<T> &operator --(int) { Prev(); return *this; }
		T *operator *() { return (T*)Current(); }

		bool Each()
		{
			if (each_dir > 0) return Next() != 0;
			else if (each_dir < 0) return Prev() != 0;
			else each_dir = IndexOf(Current()) == 0 ? 1 : -1;
			return Current() != 0;
		}
	};

	typedef Iter<Type> I;
	I Start(int At = 0) { return I(this, At); }
	I End() { return I(this, -1); }
};

#if 0
///	Templated iterator for using List<type> with a type safe API.
template <class Type>
class Iterator : public DLinkIterator
{
public:
	/// Constructor
	Iterator
	(
		/// The list to iterate.
		DLinkList *l
	) : DLinkIterator(l) {}
	/// Constructor
	Iterator
	(
		/// Source iterator
		const Iterator<Type> &it
	) : DLinkIterator(it) {}

	/// Destructor
	~Iterator() {}

	/// Return the pointer at 'Index'
	Type *operator [](int Index) { return (Type*) (* (DLinkIterator*)this)[Index]; }
	/// Return the first pointer
	Type *First() { return (Type*) DLinkIterator::First(); }
	/// Return the first pointer
	Type *Last() { return (Type*) DLinkIterator::Last(); }
	/// Return the next pointer
	Type *Next() { return (Type*) DLinkIterator::Next(); }
	/// Return the previous pointer
	Type *Prev() { return (Type*) DLinkIterator::Prev(); }
	/// Return the current pointer
	Type *Current() { return (Type*) DLinkIterator::Current(); }
	/// Return true if the pointer is in the list
	bool HasItem(Type *p) { return DLinkIterator::HasItem((Type*)p); }
	/// Return the index of the pointer if it's in the list, otherwise -1.
	int IndexOf(Type *p) { return DLinkIterator::IndexOf((Type*)p); }
};
#endif

/// \brief Data storage class.
///
/// Allows data to be buffered in separate memory
/// blocks and then written to a continuous block for processing. Works
/// as a first in first out que. You can (and should) provide a suitable
/// PreAlloc size to the constructor. This can reduce the number of blocks
/// of memory being used (and their associated alloc/free time and
/// tracking overhead) in high volume situations.
class LgiClass GBytePipe : public GStream
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
	GBytePipe
	(
		/// Sets the block size, which means allocating ahead and then joining
		/// together smaller inserts into 1 continuous block.
		int PreAlloc = 0
	);
	/// Desructor
	virtual ~GBytePipe();

	GBytePipe &operator =(GBytePipe &p);

	/// Returns true if the container is empty
	virtual bool IsEmpty() { return Mem.Length() == 0; }
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
	/// Reads data from the start of the container without removing it from
	/// the que. Returns the bytes copied.
	virtual int64 Peek
	(
		/// Buffer for output
		GStreamI *Str,
		/// Bytes to look at
		int Size
	);
	/// Looks for a sub-string in the buffered data
	/// \returns the index of the start of the string or -1 if not found
	int StringAt(char *Str);

	/// Gets the total bytes in the container
	int64 GetSize();
	/// Reads bytes off the start of the container
	int Read(void *Buffer, int Size, int Flags = 0);
	/// Writes bytes to the end of the container
	int Write(const void *Buffer, int Size, int Flags = 0);

	/// Reads a 2 byte integer from the start
	int Pop(short &i);
	/// Reads a 4 byte integer from the start
	int Pop(int &i);
	/// Reads a double from the start
	int Pop(double &i);

	/// Writes a 2 byte int to the end
	int Push(short i)				{ return Write((uchar*)&i, sizeof(i)); }
	/// Writes a 2 byte int to the end
	int Push(int i)					{ return Write((uchar*)&i, sizeof(i)); }
	/// Writes a double to the end
	int Push(double i)				{ return Write((uchar*)&i, sizeof(i)); }
};

/// A version of GBytePipe for strings. Adds some special handling for strings.
class LgiClass GStringPipe : public GBytePipe
{
public:
	/// Constructs the object
	GStringPipe
	(
		/// Number of bytes to allocate per block.
		int PreAlloc = -1
	) : GBytePipe(PreAlloc) {}
	
	~GStringPipe() {}

	/// Removes a utf-8 line of text from the container
	virtual int Pop
	(
		/// The output buffer
		char *Str,
		/// The size of the buffer
		int BufSize
	);
	/// Inserts a utf-8 string into the container
	virtual int Push
	(
		/// The string
		const char *Str,
		/// The length in bytes
		int Chars = -1
	);
	/// Inserts a wide char string into the container
	virtual int Push
	(
		/// The string
		const char16 *Str,
		/// The length in characters
		int Chars = -1
	);

	/// Creates a null terminated utf-8 string out of the classes contents
	char *NewStr();
	/// Creates a null terminated wide character string out of the classes contents
	char16 *NewStrW();
};

#endif
