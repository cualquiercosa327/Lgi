/// \file
/// \author Matthew Allen

#ifndef __OS_CLASS_H
#define __OS_CLASS_H

class LgiClass OsApplication
{
	int Args;
	const char **Arg;
	
public:
	OsApplication(int i, const char **a)
	{
		Args = i;
		Arg = a;
	}
	
	int GetArgs() { return Args; }
	const char **GetArg() { return Arg; }
};

#ifdef __OBJC__

extern GdcPt2 LFlip(NSView *v, GdcPt2 p);
extern GRect LFlip(NSView *v, GRect p);

#endif

extern OsView DefaultOsView(class GView *v);

#endif
