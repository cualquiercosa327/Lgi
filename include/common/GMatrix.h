#ifndef _GMATRIX_H_
#define _GMATRIX_H_

#define MATRIX_DOUBLE_EPSILON 1e-10

template<typename T, int Xs, int Ys>
struct GMatrix
{
	T m[Ys][Xs];

	GMatrix()
	{
		ZeroObj(m);
	}
	
	int X()
	{
		return Xs;
	}
	
	int Y()
	{
		return Ys;
	}
	
	T *operator[](int i)
	{
		LgiAssert(i >= 0 && i < Ys);
		return m[i];
	}
	
	template<int C>
	GMatrix<T, C, Ys> operator *(const GMatrix<T, C, Xs> &b)
	{
		// a<ay, ax> * b<by, bx> where ax == by, and the output matrix is <ay, bx>
		// Xs = 2
		// Ys = 4
		// Bx = 3
		// 
		GMatrix<T, C, Ys> r;
		
		for (int y=0; y<Ys; y++)
		{
			for (int x=0; x<C; x++)
			{
				r[y][x] = 0;
				for (int i=0; i<Xs; i++)
				{
					r[y][x] += m[y][i] * b.m[i][x];
				}
			}
		}
		
		return r;
	}
	
	GMatrix<T, Xs, Ys> &operator *(T scalar)
	{
		for (int y=0; y<Ys; y++)
		{
			for (int x=0; x<Xs; x++)
			{
				m[y][x] *= scalar;
			}
		}
		return *this;
	}
	
	bool operator ==(const GMatrix<T, Xs, Ys> &mat)
	{
		for (int y=0; y<Ys; y++)
		{
			for (int x=0; x<Xs; x++)
			{
				if (!Equal(m[y][x], mat.m[y][x]))
					return false;
			}
		}
		return true;
	}

	bool operator !=(const GMatrix<T, Xs, Ys> &mat)
	{
		bool eq = *this == mat;
		return !eq;
	}
	
	void SetIdentity()
	{
		for (int y=0; y<Ys; y++)
		{
			for (int x=0; x<Xs; x++)
			{
				m[y][x] = y == x ? 1 : 0;
			}
		}
	}
	
	GAutoString GetStr(const char *format)
	{
		GStringPipe p(256);
		for (int y=0; y<Ys; y++)
		{
			for (int x=0; x<Xs; x++)
			{
				if (x) p.Print(", ");
				p.Print(format, m[y][x]);
			}
			p.Print("\n");
		}
		return GAutoString(p.NewStr());
	}
	
	inline bool IsNumeric(char s)
	{
		return IsDigit(s) || s == '-' || s == 'e';
	}
	
	bool SetStr(const char *s)
	{
		if (!s)
			return false;
		
		int x = 0, y = 0;
		while (*s)
		{
			if (IsNumeric(*s))
			{
				T i;
				Convert(i, s);
				if (x < Xs && y < Ys)
				{
					m[y][x++] = i;					
				}
				while (IsNumeric(s[1]))
					s++;
			}
			else if (*s == '\n')
			{
				if (y < Ys)
				{
					while (x < Xs)
						m[y][x++] = 0;
				}

				y++;
				x = 0;
			}
			
			s++;
		}
		
		return x == Xs && y == Ys;
	}
	
	void Trace(const char *format)
	{
		GAutoString a = GetStr(format);
		LgiTrace("%s", a.Get());
	}

	// Type specific methods. If you use more types, add methods here.
	void Convert(double &i, const char *s) { i = atof(s); }	
	void Convert(int &i, const char *s) { i = atoi(s); }
	bool Equal(double a, double b) { double c = a - b; if (c < 0.0) c = -c; return c < MATRIX_DOUBLE_EPSILON; }
	bool Equal(int a, int b) { return a == b; }
};

#endif