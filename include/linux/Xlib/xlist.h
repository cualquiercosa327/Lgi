

#ifndef __XList_h
#define __XList_h

class _List
{
	class Item
	{
	public:
		Item *Next, *Prev;
		void *Ptr;
	} *_First, *_Last, *Cur;
	int Count;

public:
	_List();
	virtual ~_List();

	void *First();
	void *Last();
	void *Next();
	void *Prev();
	void *ItemAt(int i);
	void Insert(void *p, int where);
	bool Delete(void *p);
	int Items();
	void Empty();
	bool HasItem(void *p);
	int IndexOf(void *p);
};

template <class c>
class XList : public _List
{
public:
	c *First() { return (c*) _List::First(); }
	c *Last() { return (c*) _List::Last(); }
	c *Next() { return (c*) _List::Next(); }
	c *Prev() { return (c*) _List::Prev(); }
	c *ItemAt(int i) { return (c*) _List::ItemAt(i); }
	void Insert(c *p, int where = -1) { _List::Insert(p, where); }
	bool Delete(c *p) { return _List::Delete(p); }
	bool HasItem(c *p) { return _List::HasItem(p); }
	void DeleteObjects() { c *p; while (p = First()) { Delete(p); DeleteObj(p); } }
	int IndexOf(c *p) { return _List::IndexOf(p); }
};

#endif
