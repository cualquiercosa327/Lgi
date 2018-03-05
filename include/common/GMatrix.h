#ifndef _GMATRIX_H_
#define _GMATRIX_H_

#define MATRIX_DOUBLE_EPSILON 1e-10
#define Abs(x) ((x) < 0 ? -(x) : (x))

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
	
	bool IsIdentity()
	{
		for (int y=0; y<Ys; y++)
		{
			for (int x=0; x<Xs; x++)
			{
				T i = x == y ? 1 : 0;
				if (!Equal(i, m[y][x]))
					return false;
			}
		}
		
		return true;
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
		return IsDigit(s) || strchr("-.e", s) != NULL;
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
	
	void SwapRow(int row_a, int row_b)
	{
		if (row_a == row_b)
			return;

		for (int i=0; i<Xs; i++)
		{
			T t = m[row_a][i];
			m[row_a][i] = m[row_b][i];
			m[row_b][i] = t;
		}
	}

	bool Inverse()
	{
		int x, y, maxrow, y2;

		if (Xs != Ys)
			return false;

		GMatrix<T, Xs * 2, Ys> b;
		for (y=0; y<Ys; y++)
		{
			for (x=0; x<Xs; x++)
			{
				b.m[y][x] = m[y][x];
				b.m[y][x + Xs] = x == y ? 1 : 0;
			}
		}
		
		int w = Xs * 2;
		T zero = 0;

		for (y=0; y<Ys; y++)
		{
			maxrow = y;
			for (y2 = y+1; y2<Ys; y2++)
			{
				if ( Abs(b.m[y2][y]) > Abs(b.m[maxrow][y]) )
				{
					maxrow = y2;
				}
			}
			b.SwapRow(y, maxrow);

			if (Equal(zero, b.m[y][y]))
			{
				return false;
			}

			for (y2 = y+1; y2<Ys; y2++) // Eliminate column y
			{
				T c = b.m[y2][y] / b.m[y][y];
				for (x=y; x<w; x++)
				{
					b.m[y2][x] -= b.m[y][x] * c;
				}
			}
		}

		for (y = Ys-1; y>=0; y--) // Backsubstitute
		{
			T c = b.m[y][y];

			for (y2=0; y2<y; y2++)
			{
				for (x=w-1; x>=y; x--)
				{
					b.m[y2][x] -=  b.m[y][x] * b.m[y2][y] / c;
				}
			}
			
			b.m[y][y] /= c;
			
			for (x=Ys; x<w; x++) // Normalize row y
			{
				b.m[y][x] /= c;
			}
		}
		
		for (y=0; y<Ys; y++)
		{
			for (x=0; x<Xs; x++)
			{
				m[y][x] = b.m[y][x+Xs];
			}
		}

		return true;
	}

	// Type specific methods. If you use more types, add methods here.
	void Convert(double &i, const char *s) { i = atof(s); }	
	void Convert(int &i, const char *s) { i = atoi(s); }
	bool Equal(double a, double b) { double c = a - b; if (c < 0.0) c = -c; return c < MATRIX_DOUBLE_EPSILON; }
	bool Equal(int a, int b) { return a == b; }
};

#endif