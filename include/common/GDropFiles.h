#ifndef __DROP_FILES__
#define __DROP_FILES__

#if WINNATIVE
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
		#if WINNATIVE
		DROPFILES *Df = v.IsBinary() && v.Value.Binary.Length >= sizeof(DROPFILES) ? (DROPFILES*)v.Value.Binary.Data : 0;
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
		#elif defined __GTK_H__
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
				#ifdef WIN32
				while (*File == '/')
					File++;
				#endif

				// Decode URI
				char *in = File, *out = File;
				while (*in)
				{
					if (*in == '%')
					{
						char h[3] = { in[1], in[2], 0 };
						*out++ = htoi(h);
						in += 3;
					}
					else
					{
						*out++ = *in++;
					}
				}
				*out++ = 0;

				// Check file exists..
				if (FileExists(File))
				{
					Add(NewStr(File));
				}
			}
		}
		else printf("GDropFiles: Type not binary.\n");
		#elif defined MAC
		GArray<GVariant*> a;
		if (v.Type == GV_LIST)
		{
			for (GVariant *f = v.Value.Lst->First(); f; f = v.Value.Lst->Next())
			{
				a.Add(f);
			}
		}
		else
		{
			a.Add(&v);
		}
		
		for (int i=0; i<a.Length(); i++)
		{
			char *s = a[i]->Str();
			if (s)
			{
				GUri u(s);
				if (u.Protocol &&
					stricmp(u.Protocol, "file") == 0 &&
					u.Host &&
					stricmp(u.Host, "localhost") == 0)
				{
					Add(NewStr(u.Path));
				}
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
