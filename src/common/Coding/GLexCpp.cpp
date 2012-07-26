#include "GLexCpp.h"
#include <ctype.h>
#include <string.h>

static const char *White = " \r\t\n";
#define iswhite(s)		(s && strchr(White, s) != 0)
#define isword(s)		(s && (isdigit(s) || isalpha(s) || (s) == '_') )
#define skipws(s)		while (iswhite(*s)) s++;

// Returns the next C++ token in 's'
//
// If 'ReturnString' is false then the next token is skipped and
// not returned. i.e. the return is always NULL but 's' is moved.
char16 *LexCpp(char16 *&s, bool ReturnString)
{
    char16 *Status = 0;

    LexAgain:
    skipws(s);
    if (*s == '#')
    {
        // Pre-processor command
        char16 *Start = s++;

        // Non whitespace
        char16 Nwsp = 0;
        while (*s)
        {
            s++;
            if (*s == '\n' && Nwsp != '\\')
            {
            	break;
            }
            if (!strchr(White, *s))
            {
            	Nwsp = *s;
            }
        }
        Status = ReturnString ? NewStrW(Start, s-Start) : 0;
    }
    else if (*s == '_' ||
        	 isalpha(*s))
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
                isalpha(*s)
                ||
                isdigit(*s)
            )
        )
        {
            s++;
        }
        Status = ReturnString ? NewStrW(Start, s-Start) : 0;
    }
    else if (s[0] == '/' && s[1] == '/')
    {
        // C++ Comment
        s = StrchrW(s, '\n');
        if (s)
        {
            s++;
            goto LexAgain;
        }
    }
    else if (s[0] == '/' && s[1] == '*')
    {
        // C comment
        s += 2;
        
        char16 Eoc[] = {'*','/',0};
        s = StrstrW(s, Eoc);
        if (s)
        {
            s += 2;
            goto LexAgain;
        }
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
        Status = ReturnString ? NewStrW(s, 2) : 0;
        s += 2;
    }
    else if (isdigit(*s) || (*s == '-' && isdigit(s[1]) ) )
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
                isdigit(*s)
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
        Status = ReturnString ? NewStrW(Start, s-Start) : 0;
    }
    else if (*s && strchr("-()*[]&,{};:=!<>?.\\+/%^|~", *s))
    {
        // Delimiter
        Status = ReturnString ? NewStrW(s++, 1) : 0;
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
        Status = ReturnString ? NewStrW(Start, s-Start) : 0;
    }
    else
    {
        return 0;
    }
    
    return Status;
};

