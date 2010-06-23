#ifndef __DROP_FILES__
#define __DROP_FILES__

#if defined WIN32
#include <Shlobj.h>
#elif defined LINUX
#include "GToken.h"
#elif defined MAC
#include "INet.h"
#endif
#include "GVariant.h"

class GDropFiles : public GArray<char*>
{
public:
    GDropFiles(GVariant &v)
    {
		#if defined WIN32
		DROPFILES *Df = v.IsBinary() AND v.Value.Binary.Length >= sizeof(DROPFILES) ? (DROPFILES*)v.Value.Binary.Data : 0;
		if (Df)
		{
			void *FilesPtr = ((char*)Df) + Df->pFiles;
			if (Df->fWide)
			{
				char16 *f = (char16*)FilesPtr;
				while (*f)
				{
					char *Utf8 = LgiNewUtf16To8(f);
					if (Utf8) Add(Utf8);
					f += StrlenW(f) + 1;
				}
			}
			else
			{
				char *f = (char*)FilesPtr;
				while (*f)
				{
					Add((char*)LgiNewConvertCp("utf-8", f, LgiAnsiToLgiCp()));
					f += strlen(f) + 1;
				}
			}
		}
		#elif defined LINUX
		if (v.IsBinary())
		{
			GToken Uri(	(char*)v.Value.Binary.Data,
						"\r\n,",
						true,
						v.Value.Binary.Length);

			for (int i=0; i<Uri.Length(); i++)
			{
				char *File = Uri[i];

				// Remove leading 'File:'
				if (strnicmp(File, "file:", 5) == 0) File += 5;

				// Decode URI
				char *i = File, *o = File;
				while (*i)
				{
					if (*i == '%')
					{
						char h[3] = { i[1], i[2], 0 };
						*o++ = htoi(h);
						i += 3;
					}
					else
					{
						*o++ = *i++;
					}
				}
				*o++ = 0;

				// Check file exists..
				if (FileExists(File))
				{
					Add(NewStr(File));
				}
			}
		}
		else printf("GDropFiles: Type not binary.\n");
		#elif defined MAC
		char *s = v.Str();
		if (s)
		{
			GUri u(s);
			if (u.Protocol AND
				stricmp(u.Protocol, "file") == 0 AND
				u.Host AND
				stricmp(u.Host, "localhost") == 0)
			{
				Add(NewStr(u.Path));
			}
		}
		#endif
    }

    ~GDropFiles()
    {
		DeleteArrays();
    }
};

#endif
