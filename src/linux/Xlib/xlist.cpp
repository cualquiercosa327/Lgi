#include "LgiLinux.h"

_List::_List()
{
	_First = _Last = Cur = 0;
	Count = 0;
}

_List::~_List()
{
	Empty();
}

void _List::Empty()
{
	while (_First)
	{
		Item *i = _First;
		_First = _First->Next;
		delete i;
	}

	Count = 0;
	Cur = _First = _Last = 0;
}

void *_List::First()
{
	Cur = _First;
	return Cur ? Cur->Ptr : 0;
}

void *_List::Last()
{
	Cur = _Last;
	return Cur ? Cur->Ptr : 0;
}

void *_List::Next()
{
	if (Cur) Cur = Cur->Next;
	return Cur ? Cur->Ptr : 0;
}

void *_List::Prev()
{
	if (Cur) Cur = Cur->Prev;
	return Cur ? Cur->Ptr : 0;
}

void *_List::ItemAt(int n)
{
	if (n < 0) return 0;
	
	for (Cur=_First; Cur; Cur = Cur->Next, n--)
	{
		if (n == 0) return Cur->Ptr;
	}
	
	return 0;
}

void _List::Insert(void *p, int where)
{
	Item *New = new Item;
	if (New)
	{
		New->Ptr = p;
		New->Next = New->Prev = 0;

		if (_First)
		{
			// add to existing list
			if (where == 0)
			{
				// at start
				_First->Prev = New;
				New->Next = _First;
				New->Prev = 0;
				_First = New;
			}
			else
			{
				// at middle or end
				if (where < 0)
				{
					_Last->Next = New;
					New->Prev = _Last;
					New->Next = 0;
					_Last = New;
				}
				else
				{
					Item *i = _First;
					while (--where)
					{
						if (i->Next)
						{
							i = i->Next;
						}
					}

					New->Next = i->Next;
					New->Prev = i;
					if (i->Next)
					{
						i->Next->Prev = New;
					}
					else
					{
						_Last = New;
					}
					i->Next = New;
				}
			}

			Count++;
		}
		else
		{
			// add to empty list
			_First = _Last = New;
			Count = 1;
		}
	}
}

bool _List::Delete(void *p)
{
	for (Item *i=_First; i; i = i->Next)
	{
		if (i->Ptr == p)
		{
			if (Cur == i)
			{
				Cur = Cur->Next;
			}

			if (i->Prev)
				i->Prev->Next = i->Next;
			else
				_First = i->Next;

			if (i->Next)
				i->Next->Prev = i->Prev;
			else
				_Last = i->Prev;

			delete i;
			Count--;
			
			return true;
		}
	}
	
	return false;
}

int _List::Items()
{
	return Count;
}

bool _List::HasItem(void *p)
{
	for (Item *i=_First; i; i = i->Next)
	{
		if (i->Ptr == p)
		{
			return true;
		}
	}

	return false;
}

int _List::IndexOf(void *p)
{
	int n = 0;
	for (Item *i=_First; i; i = i->Next, n++)
	{
		if (i->Ptr == p) return n;
	}

	return -1;
}
