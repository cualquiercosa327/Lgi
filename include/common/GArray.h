/// \file
/// \author Matthew Allen
/// \brief Growable, type-safe array.

#ifndef _GARRAY_H_
#define _GARRAY_H_

#include <stdlib.h>
#include <assert.h>
#include "LgiDefs.h"
#include "GMem.h"

#define GARRAY_MIN_SIZE			16

/// \brief Growable type-safe array.
///
/// You can store simple objects inline in this array, but all their contents are initialized 
/// to the octet 0x00. Which limits use to objects that don't have a virtual table and
/// don't need construction (constructors are not called).
///
/// The objects are copied around during use so you can't assume their pointer
/// will remain the same over time either. However when objects are deleted from the
/// array their destructors WILL be called. This allows you to have simple objects that
/// have dynamically allocated pointers that are freed properly. A good example of this
/// type of object is the GVariant or GAutoString class.
///
/// If you want to store objects with a virtual table, or that need their constructor
/// to be called then you should create the GArray with pointers to the objects instead
/// of inline objects. And to clean up the memory you can call GArray::DeleteObjects or
/// GArray::DeleteArrays.
template <class Type>
class GArray
{
	Type *p;
	uint32 len;
	uint32 alloc;

protected:
	bool fixed;

public:
	/// Constructor
	GArray(int PreAlloc = 0)
	{
		p = 0;
		len = 0;
		fixed = false;

		alloc = PreAlloc;
		if (alloc)
		{
			int Bytes = sizeof(Type) * alloc;
			p = (Type*) malloc(Bytes);
			if (p)
			{
				memset(p, 0, Bytes);
			}
			else
			{
				alloc = 0;
			}
		}
	}

	GArray(const GArray<Type> &c)
	{
		p = 0;
		alloc = len = 0;
		fixed = false;
		*this = c;
	}

	/// Destructor	
	~GArray()
	{
		Length(0);
	}	
	
	/// Returns the number of used entries
	uint32 Length() const
	{
		return len;
	}
	
	/// Makes the length fixed..
	void SetFixedLength(bool fix = true)
	{
		fixed = fix;
	}

	/// Sets the length of available entries
	bool Length(uint32 i)
	{
		if (i > 0)
		{
			if (i > len && fixed)
			{
				LgiAssert(!"Attempt to enlarged fixed array.");
				return false;
			}

			uint32 nalloc = alloc;
			if (i < len)
			{
			    // Shrinking
			}
			else
			{
			    // Expanding
			    uint32 b;
			    for (b = 4; ((uint32)1 << b) < i; b++)
			        ;
			    nalloc = 1 << b;
			    LgiAssert(nalloc >= i);
			}
			
			if (nalloc != alloc)
			{
				Type *np = (Type*)malloc(sizeof(Type) * nalloc);
				if (!np)
				{
					return false;
				}

				if (p)
				{
					// copy across common elements
					memcpy(np, p, min(len, i) * sizeof(Type));
					free(p);
				}
				p = np;
				alloc = nalloc;
			}

			if (i > len)
			{
				// zero new elements
				memset(p + len, 0, sizeof(Type) * (nalloc - len));
			}
			else if (i < len)
			{
				for (uint32 n=i; n<len; n++)
				{
					p[n].~Type();
				}
			}

			len = i;
		}
		else
		{
			if (p)
			{
				uint32 Length = len;
				for (uint32 i=0; i<Length; i++)
				{
					p[i].~Type();
				}
				free(p);
				p = 0;
			}
			len = alloc = 0;
		}

		return true;
	}

	GArray<Type> &operator =(const GArray<Type> &a)
	{
		Length(a.Length());
		if (p && a.p)
		{
			for (uint32 i=0; i<len; i++)
			{
				p[i] = a.p[i];
			}
		}
		return *this;
	}
	
	/// \brief Returns a reference a given entry.
	///
	/// If the entry is off the end of the array and "fixed" is false,
	/// it will grow to make it valid.
	Type &operator [](uint32 i)
	{
		static Type t;
		if
		(
			i < 0
			||
			(fixed && i >= len)
		)
		{
			ZeroObj(t);
			if (fixed && i >= len)
				LgiAssert(!"Attempt to enlarged fixed array.");
			return t;
		}
		
		#if 1
		if (i > 64<<20)
		{
			#if defined(_DEBUG) && defined(_MSC_VER)
			LgiAssert(0);
			#endif
			
			ZeroObj(t);
			return t;
		}
		#endif

		if (i >= alloc)
		{
			// increase array length
			uint nalloc = max(alloc, GARRAY_MIN_SIZE);
			while (nalloc <= i)
			{
				nalloc <<= 1;
			}

			// alloc new array
			Type *np = (Type*) malloc(sizeof(Type) * nalloc);
			if (np)
			{
				// clear new cells
				memset(np + len, 0, (nalloc - len) * sizeof(Type));
				if (p)
				{
					// copy across old cells
					memcpy(np, p, len * sizeof(Type));
					
					// clear old array
					free(p);
				}
				
				// new values
				p = np;
				alloc = nalloc;
			}
			else
			{
				static Type *t = 0;
				return *t;
			}
		}

		// adjust length of the the array
		if (i + 1 > len)
		{
			len = i + 1;
		}
		
		return p[i];
	}
	
	/// Delete all the entries as if they are pointers to objects
	void DeleteObjects()
	{
		if (len > 0)
		{
			int InitialLen = len;
			delete p[0];
			if (InitialLen == len)
			{
				// Non self deleting
				for (uint i=1; i<len; i++)
				{
					delete p[i];
					p[i] = 0;
				}
				Length(0);
			}
			else
			{
				// Self deleting object...
				while (len)
				{
					delete p[0];
				}
			}
		}
	}
	
	/// Delete all the entries as if they are pointers to arrays
	void DeleteArrays()
	{
		for (uint32 i=0; i<len; i++)
		{
			DeleteArray(p[i]);
		}
		Length(0);
	}
	
	/// Find the index of entry 'n'
	int IndexOf(Type n)
	{
		for (uint i=0; i<len; i++)
		{
			if (p[i] == n) return i;
		}
		
		return -1;
	}

	/// Returns true if the item 'n' is in the array
	bool HasItem(Type n)
	{
		return IndexOf(n) >= 0;
	}
	
	/// Deletes an entry
	bool DeleteAt
	(
		/// The index of the entry to delete
		uint Index,
		/// true if the order of the array matters, otherwise false.
		bool Ordered = false
	)
	{
		if (p && Index >= 0 && Index < len)
		{
			// Delete the object
			p[Index].~Type();

			// Move the memory up
			if (Index < len - 1)
			{
				if (Ordered)
				{
					memmove(p + Index, p + Index + 1, (len - Index - 1) * sizeof(Type) );
				}
				else
				{
					p[Index] = p[len-1];
				}
			}

			// Adjust length
			len--;

			// Kill the element at the end... otherwise New() returns non-zero data.
			memset(p + len, 0, sizeof(Type));
			return true;
		}
		
		return false;
	}

	/// Deletes the entry 'n'
	bool Delete
	(
		/// The value of the entry to delete
		Type n,
		/// true if the order of the array matters, otherwise false.
		bool Ordered = false
	)
	{
		int i = IndexOf(n);
		if (p && i >= 0)
		{
			return DeleteAt(i, Ordered);
		}
		
		return false;
	}

	/// Appends an element
	void Add
	(
		/// Item to insert
		const Type &n
	)
	{
		(*this)[len] = n;
	}

	/// Appends multiple elements
	void Add
	(
		/// Items to insert
		Type *s,
		/// Length of array
		int count
		
	)
	{
		if (!s || count < 1)
			return;
			
		int i = len;
		Length(len + count);
		Type *d = p + i;
		while (count--)
		{
			*d++ = *s++;
		}
	}

	/// Appends an array of elements
	void Add
	(
		/// Array to insert
		GArray<Type> &a
		
	)
	{
		int old = len;
		if (Length(len + a.Length()))
		{
			for (int i=0; i<a.Length(); i++, old++)
				p[old] = a[i];
		}
	}
	GArray<Type> &operator +(GArray<Type> &a)
	{
		Add(a);
		return *this;
	}

	/// Inserts an element into the array
	bool AddAt
	(
		/// Item to insert before
		int Index,
		/// Item to insert
		Type n
	)
	{
		// Make room
		if (Length(len + 1))
		{
			if (Index < len - 1)
			{
				// Shift elements after insert point up one
				memmove(p + Index + 1, p + Index, (len - Index - 1) * sizeof(Type) );
			}
			else if (Index >= len)
			{
				// Add at the end, not after the end...
				Index = len - 1;
			}

			// Insert item
			p[Index] = n;

			return true;
		}

		return false;
	}

	/// Sorts the array
	void Sort(int (*Compare)(Type*, Type*))
	{
		typedef int (*qsort_compare)(const void *, const void *);
		qsort(p, len, sizeof(Type), (qsort_compare)Compare);
	}

	/// \returns a reference to a new object on the end of the array
	Type &New()
	{
		return (*this)[len];
	}

	/// Returns the memory held by the array and sets itself to empty
	Type *Release()
	{
		Type *Ptr = p;
		p = 0;
		len = alloc = 0;
		return Ptr;
	}

	template <class T>
	class Iter
	{
		int i;
		int8 each_dir;
		GArray<T> *a;

	public:
		Iter(GArray<T> *arr, int pos)
		{
			i = pos;
			a = arr;
			each_dir = 0;
		}

		bool Each()
		{
			if (each_dir) i += each_dir;
			else each_dir = i == 0 ? 1 : -1;
			return In();
		}

		operator bool() { return In(); }
		bool In() { return i >= 0 && i < a->Length(); }
		bool End() { return i < 0 || i >= a->Length(); }
		T &operator *() { return (*a)[i]; }
		Iter<T> &operator ++() { i++; return *this; }
		Iter<T> &operator --() { i--; return *this; }
		Iter<T> &operator ++(int) { i++; return *this; }
		Iter<T> &operator --(int) { i--; return *this; }
	};

	typedef Iter<Type> I;
	I Start() { return I(this, 0); }
	I End() { return I(this, Length()-1); }
};

#endif
