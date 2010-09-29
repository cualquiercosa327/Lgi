#include "Lgi.h"
#include "GTextView.h"

char *GTextView::WhiteSpace		= " \t\r\n";
char *GTextView::Delimiters		= "!@#$%^&*()'\":;,.<>/?[]{}-=+\\|`~";
char *GTextView::UrlDelim		= "!~/:%+-?@&$#._=,";

bool GTextView::AlphaOrDigit(char c)
{
	return IsDigit(c) OR IsAlpha(c);
}

char16 *ConvertToCrLf(char16 *Text)
{
	if (Text)
	{
		// add '\r's
		int Lfs = 0;
		int Len = 0;
		char16 *s=Text;
		for (; *s; s++)
		{
			if (*s == '\n') Lfs++;
			Len++;
		}

		char16 *Temp = new char16[Len+Lfs+1];
		if (Temp)
		{
			char16 *d=Temp;
			s = Text;
			for (; *s; s++)
			{
				if (*s == '\n')
				{
					*d++ = 0x0d;
					*d++ = 0x0a;
				}
				else if (*s == '\r')
				{
					// ignore
				}
				else
				{
					*d++ = *s;
				}
			}
			*d++ = 0;

			DeleteObj(Text);
			return Temp;
		}
	}

	return Text;
}

