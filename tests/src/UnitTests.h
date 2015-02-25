#ifndef _UNIT_TESTS_H_
#define _UNIT_TESTS_H_

class UnitTest : public GBase
{
public:
	UnitTest(const char *name)
	{
		Name(name);
	}
	virtual ~UnitTest() {}

	virtual bool Run() = 0;

	bool FAIL(char *File, int Line, char *Msg)
	{
		printf("%s:%i - Error in %s: %s\n", File, Line, Name(), Msg);
		return false;
	}
};

class GAutoPtrTest : public UnitTest
{
	class GAutoPtrTestPriv *d;

public:
	GAutoPtrTest();
	~GAutoPtrTest();

	bool Run();
};

class GCssTest : public UnitTest
{
	class GCssTestPriv *d;

public:
	GCssTest();
	~GCssTest();

	bool Run();
};

class GMatrixTest : public UnitTest
{
	class GMatrixTestPriv *d;

public:
	GMatrixTest();
	~GMatrixTest();

	bool Run();
};

class GContainers : public UnitTest
{
	class GContainersPriv *d;

public:
	GContainers();
	~GContainers();

	bool Run();
};

class GStringClassTest : public UnitTest
{
	class GStringClassTestPriv *d;

public:
	GStringClassTest();
	~GStringClassTest();

	bool Run();
};


#endif