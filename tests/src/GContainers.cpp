#include "Lgi.h"
#include "UnitTests.h"
#include "LUnrolledList.h"

class GContainersPriv
{
public:
	bool ListDeleteOne(int pos, int sz)
	{
		GArray<int> a;
		List<int> l;
		for (int i=0; i<sz; i++)
		{
			l.Insert(new int(i));
			a.Add(i);
		}

		l.DeleteAt(pos);
		a.DeleteAt(pos, true);
		
		if (l.Length() != a.Length())
			return false;

		int idx = 0;
		for (auto i : l)
		{
			if (*i != a[idx])
				return false;
			idx++;
		}

		return true;
	}

	bool ListInsert(int pos, int sz)
	{
		GArray<int> a;
		List<int> l;
		for (int i=0; i<sz; i++)
		{
			l.Insert(new int(i));
			a.Add(i);
		}

		l.Insert(new int(pos), pos);
		a.AddAt(pos, pos);
		
		if (l.Length() != a.Length())
			return false;

		int idx = 0;
		for (auto i : l)
		{
			if (*i != a[idx])
				return false;
			idx++;
		}

		return true;
	}
};

GContainers::GContainers() : UnitTest("GContainers")
{
	UnitTest_ListClass();
}

GContainers::~GContainers()
{
}

bool GContainers::Run()
{
	int ref[] = {1, 4, 5, 8, -1, -1};

	{
		GArray<int> a;
		a.Add(1);
		a.Add(4);
		a.Add(5);
		a.Add(8);

		int n = 0;
		for (auto &i : a)
		{
			if (i != ref[n++])
				return FAIL(_FL, "iterator error");
		}
	}

	{
		List<int> a;
		a.Add(new int(1));
		a.Add(new int(4));
		a.Add(new int(5));
		a.Add(new int(8));

		int n = 0;
		for (auto i : a)
		{
			if (*i != ref[n++])
				return FAIL(_FL, "iterator error");
		}
		if (n != 4)
			return FAIL(_FL, "count error");

		n = 0;
		auto i3 = a.begin();
		for (int *v = *i3; i3; v = *++i3)
		{
			if (*a[n] != **i3)
				return FAIL(_FL, "iterator error");
			n++;
		}
		if (n != 4)
			return FAIL(_FL, "count error");

		if (!d->ListInsert(45, 80))
			return FAIL(_FL, "list insert");
		if (!d->ListInsert(63, 80))
			return FAIL(_FL, "list insert");
		if (!d->ListInsert(64, 80))
			return FAIL(_FL, "list insert");

		if (!d->ListDeleteOne(45, 80))
			return FAIL(_FL, "list delete one");
		if (!d->ListDeleteOne(63, 80))
			return FAIL(_FL, "list delete one");
		if (!d->ListDeleteOne(64, 80))
			return FAIL(_FL, "list delete one");
	}

	{
		GHashTbl<int, int> h;
		for (int i=1; i<=100; i++)
		{
			h.Add(i, i << 1);
		}
		int count = 0;
		for (auto i : h)
		{
			if (i.key << 1 != i.value)
				return FAIL(_FL, "hash iterate");
			count++;
		}
		if (count != h.Length())
			return FAIL(_FL, "hash iterate");
		count = 0;
		for (auto i : h)
		{
			if (i.key == 50)
				h.Delete(i.key);
			else
				count++;
		}
		if (count != h.Length())
			return FAIL(_FL, "hash delete during iterate");
	}

	{
		LUnrolledList<int> IntLst;
		IntLst.Add(123);

		LUnrolledList<GString> StrLst;
		StrLst.Add("Hello");
		for (auto i : StrLst)
		{
			if (_stricmp(i, "Hello") != 0)
				return FAIL(_FL, "str lst iteration");
		}

		StrLst.Add("Aacc");
		StrLst.Add("Tyertw");
		StrLst.Add("Bbbsd");

		StrLst.Sort
		(
			[](GString &a, GString &b, int Dir)
			{
				return _stricmp(a, b) * Dir;
			},
			1
		);

		int asd=0;
	}

	return true;
}

