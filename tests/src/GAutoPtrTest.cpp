#include "Lgi.h"
#include "UnitTests.h"

class GAutoPtrTestPriv
{
};

GAutoPtrTest::GAutoPtrTest() : UnitTest("GAutoPtrTest")
{
}

GAutoPtrTest::~GAutoPtrTest()
{
}

GAutoString CreateAutoString(char **Ptr)
{
	return GAutoString(*Ptr = NewStr("Something"));
}

bool GAutoPtrTest::Run()
{
	char *Ptr;
	GAutoString s1(Ptr = NewStr("string1"));
	GAutoString s2 = s1;
	if (s1.Get() != 0)
	{
		return FAIL(_FL, "Initial auto ptr should be zero now.");
	}
	if (s2.Get() != Ptr)
	{
		return FAIL(_FL, "Ptrs should be equal.");
	}

	char *Ptr2 = 0;
	GAutoString s3 = CreateAutoString(&Ptr2);
	if (s3.Get() != Ptr2)
	{
		return FAIL(_FL, "Ptr not the same.");
	}
	if (strnicmp(s3, "Something", 9))
	{
		return FAIL(_FL, "Ptr contents are wrong.");
	}

	return true;
}

