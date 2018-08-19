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

extern OsView DefaultOsView(class GView *v);

#endif
