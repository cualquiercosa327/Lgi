#include "Lgi.h"
#include "UnitTests.h"
#include "GMatrix.h"

class GMatrixTestPriv
{
public:
	bool Error(char *Fmt, ...)
	{
		va_list Args;
		va_start(Args, Fmt);
		vprintf(Fmt, Args);
		va_end(Args);
		return false;
	}

	bool Test1()
	{
		// Identity testing
		GMatrix<double, 3, 3> m1;
		m1.SetIdentity();
		for (int y=0; y<m1.Y(); y++)
		{
			for (int x=0; x<m1.X(); x++)
			{
				if (m1.m[y][x] != (x == y ? 1.0 : 0.0))
					return false;
			}
		}
		if (!m1.IsIdentity())
			return false;
		
		// Multiply test
		GMatrix<double, 3, 2> a;
		a.SetStr(	"1 2 3\n"
					"4 5 6\n");
		GMatrix<double, 2, 3> b;
		b.SetStr(	"7 8\n"
					"9 10\n"
					"11 12\n");
		GMatrix<double, 2, 2> c, r;
		c = a * b;
		r.SetStr(	"58 64\n"
					"139 154\n");
		if (c != r)
			return false;
	
		// Inverse test
		GMatrix<double, 3, 3> m2, m3, m4;
		m2.SetStr(	"1	 5	 2\n"
					"1	 1	 7\n"
					"0	 -3	 4\n");
		m3 = m2;
		if (!m3.Inverse())
			return false;
		m4 = m2 * m3;
		if (!m4.IsIdentity())
			return false;	
	
		return true;
	}
};

GMatrixTest::GMatrixTest() : UnitTest("GMatrixTest")
{
	d = new GMatrixTestPriv;
}

GMatrixTest::~GMatrixTest()
{
	DeleteObj(d);
}

bool GMatrixTest::Run()
{
	return	d->Test1();
}