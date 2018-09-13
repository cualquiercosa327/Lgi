#ifndef _LPARSE_PART_H_
#define _LPARSE_PART_H_

// Very simple variable parser
// See fn 'UnitTests' for examples
struct LParsePart
{
	// The character that started the variable or 0 for a literal string
	char Start;

	// if (Start != 0) -> The name of the parsed variable
	// else -> The string literal
	GString Str;

	LParsePart()
	{
		Start = 0;
	}

	static GArray<LParsePart> Split
	(
		// String to parse
		const char *In,
		// Pairs of characters to match. The space ' ' char
		// can be used as a special end marker that just means
		// parse till the end of the word.
		//
		// e.g. "%%" would parse things like "An environment %variable% to match"
		//		"[]" would parse an array marker: "some_var[index]"
		//		"$ " would parse this: "here is my $variable to match" 
		const char *SeparatorPairs
	)
	{
		GArray<LParsePart> a;
		const char *c = In;
		const char *prev = c;
		char Lut[256] = {0};
		for (auto p = SeparatorPairs; p[0] && p[1]; p += 2)
			Lut[p[0]] = p[1];
		#ifdef _DEBUG
		const char *End = c + strlen(c);
		#endif

		while (c && *c)
		{
			#ifdef _DEBUG
			if (c >= End) LgiAssert(!"Off the end");
			#endif

			char End = Lut[(uint8)*c];
			if (End)
			{
				if (c > prev)
					a.New().Str.Set(prev, c - prev);
				auto &v = a.New();
				v.Start = *c++;
				prev = c;
				if (End == ' ')
				{
					// Word match
					while (*c && (IsAlpha(*c) || IsDigit(*c) || *c == '_'))
						c++;
					v.Str.Set(prev, c - prev);
					prev = c;
				}
				else
				{
					// End char match
					while (*c && *c != End)
						c++;
					v.Str.Set(prev, c - prev);
					if (*c) c++;
					prev = c;
				}
			}
			else c++;
		}

		if (c > prev)
			a.New().Str.Set(prev, c - prev);
	
		return a;
	}

	#undef FAILTEST
	#define FAILTEST(test, str) if (!(test)) { LgiAssert(!str); return false; }
	bool Check(char start, const char *str) { return Start == start && Str.Equals(str); }

	static bool UnitTests()
	{
		// Non word based
		auto a = Split("arraytest[123];", "[]");
		FAILTEST(a.Length() == 3, "arr len")
		FAILTEST(a[0].Check(0, "arraytest"), "arr[0]")
		FAILTEST(a[1].Check('[', "123"), "arr[1]")
		FAILTEST(a[2].Check(0, ";"), "arr[1]")

		// Unterminated non-word
		a = Split("arraytest[12", "[]");
		FAILTEST(a.Length() == 2, "arr len")
		FAILTEST(a[0].Check(0, "arraytest"), "arr[0]")
		FAILTEST(a[1].Check('[', "12"), "arr[1]")

		// Word based
		a = Split("some $value to parse", "$ ");
		FAILTEST(a.Length() == 3, "arr len")
		FAILTEST(a[0].Check(0, "some "), "arr[0]")
		FAILTEST(a[1].Check('$', "value"), "arr[1]")
		FAILTEST(a[2].Check(0, " to parse"), "arr[2]")

		// Unterminated word based
		a = Split("some $valu", "$ ");
		FAILTEST(a.Length() == 2, "arr len")
		FAILTEST(a[0].Check(0, "some "), "arr[0]")
		FAILTEST(a[1].Check('$', "valu"), "arr[1]")

		return true;
	}
};

#endif