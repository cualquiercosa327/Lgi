#include "GLexCpp.h"
#include <ctype.h>
#include <string.h>

#define iswhite(s)		(s && strchr(White, s) != 0)
#define isword(s)		(s && (IsDigit(s) || IsAlpha(s) || (s) == '_') )
#define skipws(s)		while (*s) \
						{ \
							if (*s == '\n') \
							{ \
								if (LineCount) *LineCount = *LineCount + 1; \
								s++; \
							} \
							else if (*s == ' ' || *s == '\t' || *s == '\r') \
								s++; \
							else \
								break; \
						}

char16 *LexStrdup(void *context, const char16 *start, ssize_t len)
{
	if (len < 0)
		len = StrlenW(start);
		
	char16 *n = new char16[len + 1];
	if (n)
	{
		memcpy(n, start, sizeof(*n) * len);
		n[len] = 0;
	}
	
	return n;
}

char16 *LexNoReturn(void *context, const char16 *start, ssize_t len)
{
	return NULL;
}

// Returns the next C++ token in 's'
//
// If 'ReturnString' is false then the next token is skipped and
// not returned. i.e. the return is always NULL but 's' is moved.
char16 *LexCpp
(
	char16 *&s,
	LexCppStrdup Strdup,
	void *Context,
	int *LineCount
)
{
    LexAgain:
    skipws(s);
    if (*s == '#')
    {
        // Pre-processor command
        char16 *Start = s++;
        
        // Skip any whitespace
        while (*s == ' ' || *s == '\t')
        {
			Start = s;
			*s++ = '#';
		}

        // Non whitespace
        while (*s && IsAlpha(*s))
        {
            s++;
        }

        return Strdup(Context, Start, s-Start);
    }
    else if (*s == '_' || IsAlpha(*s))
    {
        // Identifier
        char16 *Start = s++;
        while
        (
            *s
            &&
            (
                *s == '_'
                ||
                *s == ':'
                ||
                IsAlpha(*s)
                ||
                IsDigit(*s)
            )
        )
        {
            s++;
        }
        
        return Strdup(Context, Start, s-Start);
    }
    else if (s[0] == '/' && s[1] == '/')
    {
        // C++ Comment
        s += 2;
        while (*s)
        {
			if (*s == '\n')
			{
				if (LineCount) *LineCount = *LineCount+1;
				s++;
				goto LexAgain;
			}
			
			s++;
		}
		return NULL;
    }
    else if (s[0] == '/' && s[1] == '*')
    {
        // C comment
        s += 2;
        while (*s)
        {
			if (*s == '\n')
			{
				if (LineCount) *LineCount = *LineCount+1;
			}
			else if (s[0] == '*' && s[1] == '/')
			{
				s += 2;
				goto LexAgain;
			}
			
			s++;
		}
		return NULL;
    }
    else if
    (
        (s[0] == '-' && s[1] == '>')
        ||
        (s[0] == '|' && s[1] == '|')
        ||
        (s[0] == '&' && s[1] == '&')
        ||
        (s[0] == '+' && s[1] == '+')
        ||
        (s[0] == '-' && s[1] == '-')
        ||
        (s[0] == '/' && s[1] == '=')
        ||
        (s[0] == '-' && s[1] == '=')
        ||
        (s[0] == '*' && s[1] == '=')
        ||
        (s[0] == '+' && s[1] == '=')
        ||
        (s[0] == '^' && s[1] == '=')
        ||
        (s[0] == '>' && s[1] == '=')
        ||
        (s[0] == '<' && s[1] == '=')
        ||
        (s[0] == '-' && s[1] == '>')
        ||
        (s[0] == '=' && s[1] == '=')
        ||
        (s[0] == '!' && s[1] == '=')
    )
    {
        // 2 char delimiter
        char16 *Status = Strdup(Context, s, 2);
        s += 2;
        return Status;
    }
    else if (IsDigit(*s) || (*s == '-' && IsDigit(s[1]) ) )
    {
        // Constant
        char16 *Start = s;
		bool IsHex = false;

		// Skip hex prefix...
		if (s[0] == '0' && tolower(s[1]) == 'x')
		{
			s += 2;
			IsHex = true;
		}

		if (*s == '-')
			s++;

        // Seek to end of number
		while
        (
            *s
            &&
            (
                *s == '.'
                ||
                *s == 'e'
                ||
                IsDigit(*s)
				||
				(
					IsHex
					&&
					strchr("abcdef", tolower(*s))
				)
            )
        )
        {
            s++;
        }
        
        return Strdup(Context, Start, s-Start);
    }
    else if (*s && strchr("-()*[]&,{};:=!<>?.\\+/%^|~@", *s))
    {
        // Delimiter
        return Strdup(Context, s++, 1);
    }
    else if (*s && strchr("\"\'", *s))
    {
        // String
        char16 Delim = *s;
        char16 *Start = s++;
        while (*s)
        {
            if (*s == '\\')
            {
                s += 2;
            }
            else if (*s == Delim)
            {
                s++;
                break;
            }
            else
            {
                s++;
            }
        }
        
        return Strdup(Context, Start, s-Start);
    }

    return NULL;
};

