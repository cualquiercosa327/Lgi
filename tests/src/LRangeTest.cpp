#include "Lgi.h"
#include "UnitTests.h"
#include "LUnrolledList.h"
#include "LHashTable.h"

bool LRangeTest::Run()
{
	GRange a, b, c;

	// Overlap tests:

	b.Set(20, 10);
	a.Set(10, 5); 
	if (a.Overlap(b).Valid()) return FAIL(_FL, "overlap");

	a.Set(10, 10);
	if (a.Overlap(b).Valid()) return FAIL(_FL, "overlap");

	a.Set(10, 11);
	if (!a.Overlap(b).Valid()) return FAIL(_FL, "overlap");

	a.Set(22, 3);
	if (!a.Overlap(b).Valid()) return FAIL(_FL, "overlap");

	a.Set(30, 5);
	if (a.Overlap(b).Valid()) return FAIL(_FL, "overlap");

	a.Set(28, 5);
	if (!a.Overlap(b).Valid()) return FAIL(_FL, "overlap");

	a.Set(33, 5);
	if (a.Overlap(b).Valid()) return FAIL(_FL, "overlap");


	/* The different -= cases we need to test:
		
		1) a is entirely before b
		2) a's end is b's start
		3) a overlaps the start of b
		4) a overlaps the whole of b
		5) a overlaps the middle of b
		6) a overlaps part of b but matching the end
		7) a overlaps the end and past the end of b
		8) a's start adjoins b's end
		9) a is entirely after b 
	*/

	// 1) a is entirely before b
	a.Set(10, 5); b.Set(20, 10);
	c = b;
	b -= a;
	if (b != c) return FAIL(_FL, "range test 1");

	// 2) a's end is b's start
	a.Set(10, 10); b.Set(20, 10);
	c = b;
	b -= a;
	if (b != c) return FAIL(_FL, "range test 2");

	// 3) a overlaps the start of b
	a.Set(12, 10); b.Set(20, 10);
	c = b;
	b -= a;
	if (b != GRange(12,8)) return FAIL(_FL, "range test 3");

	// 4) a overlaps the whole of b
	a.Set(15, 30); b.Set(20, 10);
	c = b;
	b -= a;
	if (b.Valid()) return FAIL(_FL, "range test 3");

	// 5) a overlaps the middle of b
	a.Set(22, 6); b.Set(20, 10);
	c = b;
	b -= a;
	if (b != GRange(20, 4)) return FAIL(_FL, "range test 3");

	// 6) a overlaps part of b but matching the end
	a.Set(25, 5); b.Set(20, 10);
	c = b;
	b -= a;
	if (b != GRange(20, 5)) return FAIL(_FL, "range test 3");

	// 7) a overlaps the end and past the end of b
	a.Set(25, 10); b.Set(20, 10);
	c = b;
	b -= a;
	if (b != GRange(20, 5)) return FAIL(_FL, "range test 3");

	// 8) a's start adjoins b's end
	a.Set(30, 10); b.Set(20, 10);
	c = b;
	b -= a;
	if (b != c) return FAIL(_FL, "range test 3");

	// 8) a is entirely after b 
	a.Set(35, 10); b.Set(20, 10);
	c = b;
	b -= a;
	if (b != c) return FAIL(_FL, "range test 3");

	return true;
}

