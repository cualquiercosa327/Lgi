#include "Lgi.h"
#include "UnitTests.h"

class GContainersPriv
{
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
		List<int>::I i3 = a.begin();
		for (int *v = *i3; i3; v = *++i3)
		{
			if (*a[n] != **i3)
				return FAIL(_FL, "iterator error");
			n++;
		}
		if (n != 4)
			return FAIL(_FL, "count error");
	}


	return true;
}

