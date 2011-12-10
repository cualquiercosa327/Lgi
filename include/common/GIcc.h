#ifndef _ICC_H_
#define _ICC_H_

#include "GDom.h"
#include "GVariant.h"

class GIccProfile : public GDom
{
	class GIccProfilePrivate *d;

public:
	GIccProfile(char *file = 0);
	~GIccProfile();

	// I/O
	bool CreateNamed(const char *name);
	bool Open(char *file);
	bool Open(GStream *stream);
	bool Save(char *file);
	bool Save(GStream *stream);
	
	// Props
	char *GetName();
	char *GetError();

	// Conversion
	bool Convert(COLOUR *Out32, COLOUR In32, GIccProfile *Profile = 0);
	bool Convert(GSurface *Dest, GSurface *Src, GIccProfile *Profile = 0);

	// Dom
	bool GetVariant(const char *Name, GVariant &Value, char *Array = 0);
};

#endif