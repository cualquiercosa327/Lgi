#include "Lgi.h"
#include "UnitTests.h"
#include "GStringClass.h"

#ifdef LGI_UNIT_TESTS
int32 GString::RefStrCount = 0;
#endif

class GStringClassTestPriv
{
public:
};

GStringClassTest::GStringClassTest() : UnitTest("GStringClassTest")
{
	d = new GStringClassTestPriv;
}

GStringClassTest::~GStringClassTest()
{
	DeleteObj(d);
}

bool GStringClassTest::Run()
{
	// Constructor test
	{
		GString a("test");
		GString b = a;
	}
	if (GString::RefStrCount != 0) return FAIL(_FL, "RefStrCount mismatch");

	// Assignment operator test
	{
		GString a("test");
		GString b;
		b = a;
	}
	if (GString::RefStrCount != 0) return FAIL(_FL, "RefStrCount mismatch");

	// Split/Join tests
	{
		const char *Base = "123,456,789";
		GString a(Base);
		if (GString::RefStrCount != 1) return FAIL(_FL, "RefStrCount mismatch");
		GString::Array b = a.Split(",");
		if (GString::RefStrCount != 4) return FAIL(_FL, "RefStrCount mismatch");
		GString sep("-");
		if (GString::RefStrCount != 5) return FAIL(_FL, "RefStrCount mismatch");
		GString c = sep.Join(b);
		if (GString::RefStrCount != 6) return FAIL(_FL, "RefStrCount mismatch");
		if (c.Get() && stricmp(c.Get(), "123-456-789"))
			return FAIL(_FL, "Joined string incorrect");
		if (b[1].Int() != 456)
			return FAIL(_FL, "Int cast failure");

		GString d(",,123,345,,");
		b = d.SplitDelimit(",", -1, false);
		if (b.Length() != 6)
			return FAIL(_FL, "Wrong token count");

		b = d.SplitDelimit(",", -1, true);
		if (b.Length() != 2)
			return FAIL(_FL, "Wrong token count");

		if (strcmp(Base, a))
			return FAIL(_FL, "Original string modified");
	}
	if (GString::RefStrCount != 0) return FAIL(_FL, "RefStrCount mismatch");
	
	// C string assignment / cast testing
	{
		const char *cstr = "Test";
		GString a;
		a = cstr;
		if (_stricmp(cstr, a))
			return FAIL(_FL, "C String assignment failed");
		if (strcmp(cstr, a))
			return FAIL(_FL, "Original string modified");
	}
	if (GString::RefStrCount != 0) return FAIL(_FL, "RefStrCount mismatch");
	
	// Upper / lower case conversion
	{
		const char *cstr = "Test";
		GString a = cstr, b;
		b = a.Lower();
		if (strcmp(a, "Test") != 0 ||
			strcmp(b, "test"))
			return FAIL(_FL, "Lower failed");

		b = a.Upper();
		if (strcmp(a, "Test") != 0 ||
			strcmp(b, "TEST"))
			return FAIL(_FL, "Upper failed");
	}
	if (GString::RefStrCount != 0) return FAIL(_FL, "RefStrCount mismatch");
	
	// Find testing
	{
		GString a("This is a test string");
		GString b; // Empty
		if (a.Find("asdasdwer") >= 0)
			return FAIL(_FL, "Find failed");
		if (a.Find(NULL) >= 0)
			return FAIL(_FL, "Find failed");
		if (b.Find(NULL) >= 0)
			return FAIL(_FL, "Find failed");
		if (b.Find("Asdwegewr") >= 0)
			return FAIL(_FL, "Find failed");
		if (a.Find("This") != 0)
			return FAIL(_FL, "Find failed");
		if (a.Find("test") != 10)
			return FAIL(_FL, "Find failed");
		if (a.Find("string") != 15)
			return FAIL(_FL, "Find failed");
		if (a.Find("is") != 2)
			return FAIL(_FL, "Find failed");
		if (a.RFind("is") != 5)
			return FAIL(_FL, "RFind failed");
		if (a.RFind("string") != 15)
			return FAIL(_FL, "RFind failed");
		if (a.RFind(NULL) >= 0)
			return FAIL(_FL, "RFind failed");
	}
	if (GString::RefStrCount != 0) return FAIL(_FL, "RefStrCount mismatch");
	
	// Substring testing
	{
		const char *Base = "This is a test string";
		GString a(Base);
		int b;
		b = a(5);
		if (b != 'i')
			return FAIL(_FL, "Char substring failed");
		b = a(-1);
		if (b != 'g')
			return FAIL(_FL, "Char substring failed");
		GString c;
		c = a(5, 7);
		if (_stricmp(c, "is"))
			return FAIL(_FL, "Range substring failed");
		if (strcmp(Base, a))
			return FAIL(_FL, "Original string modified");
	}
	if (GString::RefStrCount != 0) return FAIL(_FL, "RefStrCount mismatch");
	
	// Whitespace strip testing
	{
		const char *Base = "  \ta test. \t\r\n";
		GString a(Base);
		if (_stricmp(a.Strip(), "a test."))
			return FAIL(_FL, "Strip whitespace failed.");
		if (_stricmp(a.LStrip(), "a test. \t\r\n"))
			return FAIL(_FL, "Strip whitespace failed.");
		if (_stricmp(a.RStrip(), "  \ta test."))
			return FAIL(_FL, "Strip whitespace failed.");
		if (strcmp(Base, a))
			return FAIL(_FL, "Original string modified");
	}
	if (GString::RefStrCount != 0) return FAIL(_FL, "RefStrCount mismatch");

	// Concatenation tests
	{
		const char *Base1 = "This is the first part. ";
		const char *Base2 = "This is the 2nd part.";
		GString a(Base1);
		GString b(Base2);
		GString c;
		c = a + b;
		if (strcmp(c, "This is the first part. This is the 2nd part."))
			return FAIL(_FL, "Concat string incorrect");

		GString d(Base1);
		d += b;
		if (strcmp(d, "This is the first part. This is the 2nd part."))
			return FAIL(_FL, "Concat string incorrect");
		
		if (strcmp(Base1, a))
			return FAIL(_FL, "Original string modified");
		if (strcmp(Base2, b))
			return FAIL(_FL, "Original string modified");
	}
	if (GString::RefStrCount != 0) return FAIL(_FL, "RefStrCount mismatch");

	return true;
}
