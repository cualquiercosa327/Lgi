/*hdr
**	FILE:			Text.cpp
**	AUTHOR:			Matthew Allen
**	DATE:			2/7/97
**	DESCRIPTION:	Generic text document handler
**
**	Copyright (C) 1997, Matthew Allen
**		fret@memecode.com
*/

/****************************** Includes ************************************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "Gdc2.h"
#include "Text.h"

/****************************** Defines *************************************************************************************/

// Cursor type
// if the cursor goes between charaters (like in graphics mode)
#define CURSOR_BETWEEN
// else the cursor is at or under a charater (like in text mode)
//#define CURSOR_UNDER

/****************************** Locals **************************************************************************************/

char Delimiters[] = "!@#$%^&*()'\":;,.<>/?[]{}-=+\\|`~";

bool bAutoIndent = TRUE;

ushort CodePage_ISO_8859_2[] = {
	/* 0x80-8F */	0x20AC,      0, 0x201A,    402,
					0x201E, 0x2026, 0x2020, 0x2021, 
					  0x5e, 0x2030,  0x160, 0x2039,
					   338,      0,      0,      0,
	/* 0x90-9F */	     0, 0x2018, 0x2019, 0x201C,
					0x201D, 0x2022, 0x2013, 0x2014,
					  0x7E, 0x2122,  0x161, 0x203A,
					   339,      0,      0,    376,
	/* 0xA0-AF */	  0xA0,    161,    162,    163,
					   164,    165,    166,    167,
					   168,	   169,    170,    171,
					   172,    173,    174,    175,
	/* 0xB0-BF */	   176,    177,    178,    179,
					   180,    318,    347,    728,
					   184,    353,    351,    357,
					   378,    732,    382,    380,
	/* 0xC0-CF */      340,    195,    194
	/* 0xD0-DF */
	/* 0xE0-EF */
	/* 0xF0-FF */

};

ushort CodePage_WIN_1250[] = {
	/* 0x80-8F */	0x20AC, 0x0000, 0x201A, 0x0000,
					0x201E, 0x2026, 0x2020, 0x2021,
					0x0000, 0x2030, 0x0160, 0x2039,
					0x015A, 0x0164, 0x017D, 0x0179,
	/* 0x90-9F */	0x0000, 0x2018, 0x2019, 0x201C,
					0x201D, 0x2022, 0x2013, 0x2014,
					0x0000, 0x2122, 0x0161, 0x203A,
					0x015B, 0x0165, 0x017E, 0x017A,
	/* 0xA0-AF */	0x00A0, 0x02C7, 0x02D8, 0x0141,
					0x00A4, 0x0104, 0x00A6, 0x00A7,
					0x00A8, 0x00A9, 0x015E, 0x00AB,
					0x00AC, 0x00AD, 0x00AE, 0x017B,
	/* 0xB0-BF */	0x00B0, 0x00B1, 0x02DB, 0x0142,
					0x00B4, 0x00B5, 0x00B6, 0x00B7,
					0x00B8, 0x0105, 0x015F, 0x00BB,
					0x013D, 0x02DD, 0x013E, 0x017C,
	/* 0xC0-CF */	0x0154, 0x00C1, 0x00C2, 0x0102,
					0x00C4, 0x0139, 0x0106, 0x00C7,
					0x010C, 0x00C9, 0x0118, 0x00CB,
					0x011A, 0x00CD, 0x00CE, 0x010E,
	/* 0xD0-DF */	0x0110, 0x0143, 0x0147, 0x00D3,
					0x00D4, 0x0150, 0x00D6, 0x00D7,
					0x0158, 0x016E, 0x00DA, 0x0170,
					0x00DC, 0x00DD, 0x0162, 0x00DF,
	/* 0xE0-EF */	0x0155, 0x00E1, 0x00E2, 0x0103,
					0x00E4, 0x013A, 0x0107, 0x00E7,
					0x010D, 0x00E9, 0x0119, 0x00EB,
					0x011B, 0x00ED, 0x00EE, 0x010F,
	/* 0xF0-FF */	0x0111, 0x0144, 0x0148, 0x00F3,
					0x00F4, 0x0151, 0x00F6, 0x00F7,
					0x0159, 0x016F, 0x00FA, 0x0171,
					0x00FC, 0x00FD, 0x0163, 0x02D9
};

ushort *CodePages[] = {0, 0, CodePage_WIN_1250};

/****************************** Helper Functions ****************************************************************************/

int StrLen(char *c)
{
	int i = 0;
	if (c) for (i=0; *c AND *c != '\n'; i++, c++);
	return i;
}

int RevStrLen(char *c)
{
	int i = 0;
	if (c) for (i=0; *c AND *c != '\n'; i++, c--);
	return i;	
}

void CountLinesAndLen(char *c, int Len, int &EndLen, int &Lines)
{
	EndLen = 0;
	Lines = 0;
	if (c)
	{
		for (; Len>0; Len--, c++)
		{
			if (*c == '\n')
			{
				EndLen = 0;
				Lines++;
			}
			else
			{
				EndLen++;
			}
		}
	}
}

/****************************** Document Class ******************************************************************************/

Document::Document()
{
	File = 0;
}

Document::~Document()
{
	DeleteArray(File);
}

bool Document::Open(char *FileName, int Attrib)
{
	bool Status = FALSE;
	if (FileName)
	{
		char *FName = NewStr(FileName);
		DeleteObj(File);
		File = FName;
		Status = F.Open(File, Attrib);
	}
	return Status;
}

/****************************** UserAction Class ****************************************************************************/
UserAction::UserAction()
{
	Text = 0;
	x = y = 0;
	Insert = false;
}

UserAction::~UserAction()
{
	DeleteArray(Text);
}

/****************************** Cursor Class ********************************************************************************/
GCursor::GCursor()
{
	x = 0;
	y = 0;
	Parent = 0;
	Offset = 0;
}

GCursor::~GCursor()
{
}

GCursor::operator char*()
{
	return Parent->Data + (Offset + x);
}

char *GCursor::GetStr()
{
	if (Parent)
	{
		return Parent->Data + Offset;
	}
	return NULL;
}

void GCursor::GoUpLine()
{
	if (y > 0)
	{
		char *c = Parent->Data + Offset;

		if (y > 1)
		{
			y--;
			c -= 2;
			x = Length = RevStrLen(c);
			Offset = ((long) c - (long) Parent->Data) - x + 1;
		}
		else
		{
			Offset = 0;
			x = Length = StrLen(Parent->Data);
			y = 0;
		}
	}
	else
	{
		x = 0;
	}
}

void GCursor::GoDownLine()
{
	if (y < Parent->Lines-1)
	{
		char *c = Parent->Data + (Offset + Length);

		y++;
		x = 0;
		c++;
		Length = StrLen(c);
		Offset = (long) c - (long) Parent->Data;
	}
	else
	{
		x = Length;
	}
}

bool GCursor::AtEndOfLine()
{
	return x == Length;
}

bool GCursor::AtBeginningOfLine()
{
	return x == 0;
}

void GCursor::SetX(int X)
{
	x = limit(X, 0, Length);
}

void GCursor::SetY(int Y)
{
	Y = limit(Y, 0, Parent->Lines-1);
	char *c = Parent->FindLine(Y);
	if (c)
	{
		Offset = (long) c - (long) Parent->Data;
		Length = StrLen(c);
		y = Y;
	}
}

void GCursor::MoveX(int Dx)
{
	char *c = Parent->Data + Offset;
	while (Dx)
	{
		if (Dx < 0)
		{
			if (x)
			{
				x--;
			}
			else
			{
				GoUpLine();
			}
		}
		else
		{
			if (c[x])
			{
				if (c[x] != '\n')
				{
					x++;
				}
				else
				{
					GoDownLine();
				}
			}
			else
			{
				return;
			}
		}
		
		Dx += (Dx > 0) ? -1 : 1;
	}
}

void GCursor::MoveY(int Dy)
{
	while (Dy)
	{
		if (Dy < 0)
		{
			GoUpLine();
			Dy++;
		}
		else
		{
			GoDownLine();
			Dy--;
		}
	}
}

int GCursor::CursorX(int TabSize)
{
	char *c = Parent->Data + Offset;
	int cx = 0;

	for (int i=0; i<x; i++, c++)
	{
		if (*c == 9)
		{
			cx += TabSize - (cx % TabSize);
		}
		else
		{
			cx++;
		}
	}

	return cx;
}

/****************************** TextLock Class ******************************************************************************/

TextLock::TextLock()
{
	StartLine = 0;
	Lines = 0;
	Line = 0;
	LineW = 0;
}

TextLock::~TextLock()
{
	DeleteArray(Line);
	if (LineW)
	{
		for (int i=0; i<Lines; i++)
		{
			DeleteArray(LineW[i]);
		}
		DeleteArray(LineW);
	}
}

ushort *TextLock::GetLineW(int i)
{
	if (LineW AND
		i >= StartLine AND
		i < StartLine + Lines)
	{
		return LineW[i - StartLine];
	}
	return 0;
}

char *TextLock::operator [](int i)
{
	if (Line AND
		i >= StartLine AND
		i < StartLine + Lines)
	{
		return Line[i - StartLine];
	}
	return 0;
}

/****************************** TextDocument Class **************************************************************************/
TextDocument::TextDocument()
{
	Flags = 0;
	LockCount = 0;
	Dirty = false;
	Editable = true;
	Lines = 1;
	Length = 0;
	Alloc = 1;
	Data = new char[1];
	if (Data) *Data = 0;

	IgnoreUndo = 0;
	UndoPos = 0;
}

TextDocument::~TextDocument()
{
	DeleteArray(Data);
	ClearUndoQueue();
}

int TextDocument::CountLines(char *c, int Len)
{
	int L = 0;
	if (Length > 0)
	{
		if (Len >= 0)
		{
			while (*c AND Len)
			{
				L += (*c++ == '\n');
				Len--;
			}
			L += (Len > 0 AND *c == 0) ? 1 : 0;
		}
		else
		{
			while (*c)
			{
				L += (*c++ == '\n');
			}
			L++;
		}
	}
	return L;
}

char *TextDocument::FindLine(int i)
{
	char *c = 0;
	if (Length > 0)
	{
		c = Data;
		while (i)
		{
			for (; *c AND *c != '\n'; c++);
			if (*c) c++;
			else return 0;
			i--;
		}
	}
	return c;
}

bool TextDocument::SetLength(int Len)
{
	bool Status = TRUE;

	if (Len + 1 > Alloc)
	{
		int NewAlloc = (Len + TEXT_BLOCK) & (~TEXT_MASK);
		char *Temp = new char[NewAlloc];
		if (Temp)
		{
			memset(Temp + Length, 0, NewAlloc - Length);
			memcpy(Temp, Data, Length);
			DeleteArray(Data);
			Data = Temp;
			Length = Len;
			Alloc = NewAlloc;
		}
		else
		{
			Status = FALSE;
		}
	}
	else
	{
		// TODO: resize Data to be smaller if theres a
		// considerable size change
		Length = Len;
	}

	return Status;
}

bool TextDocument::Load(char *File)
{
	bool Status = FALSE;

	if (Open(File, O_READ))
	{
		int FileSize = F.GetSize();
		if (SetLength(FileSize))
		{
			F.Read(Data, FileSize);
			
			CrLf = FALSE;

			char *In = Data, *Out = Data;
			int Size = 0;
			for (int i=0; i<FileSize; i++, In++)
			{
				if (*In != '\r')
				{
					*Out++ = *In;
					Size++;
				}
				else
				{
					if (In[1] != '\n')
					{
						// Macintrash file
						*Out++ = '\n';
						Size++;
					}
					else
					{
						CrLf = TRUE;
					}
				}
			}

			Status = SetLength(Size);
			Data[Length] = 0;
			Lines = max(CountLines(Data), 1);
		}

		F.Close();
		Dirty = FALSE;
	}

	return Status;
}

bool TextDocument::Save(char *FileName)
{
	bool Status = FALSE;
	
	if (FileName)
	{
		if (Open(FileName, O_WRITE))
		{
			if (CrLf)
			{
				char pCrLf[2] = { 0x0D, 0x0A };

				for (int i=0; i<Length;)
				{
					int Start = i;
					char *Wr;
					for (Wr=Data+i; *Wr AND *Wr != '\n' AND i<Length; i++, Wr++);
					F.Write(Data+Start, i-Start);
					if (*Wr == '\n')
					{
						F.Write(pCrLf, 2);
						i++;
					}
				}

				Status = NOT F.GetStatus();
			}
			else
			{
				// no '\r's so just write the whole thing
				Status = F.Write(Data, Length);
			}

			F.SetSize(F.GetPosition());
			F.Close();
			Dirty = FALSE;
		}
	}

	return Status;
}

bool TextDocument::Import(char *s, int Length)
{
	bool Status = FALSE;
	if (s)
	{
		if (Length < 0)
		{
			Length = strlen(s);
		}

		if (SetLength(Length))
		{
			CrLf = FALSE;

			char *In = s, *Out = Data;
			int Size = 0;
			for (int i=0; i<Length; i++, In++)
			{
				if (*In != '\r')
				{
					*Out++ = *In;
					Size++;
				}
				else
				{
					CrLf = TRUE;
				}
			}

			Status = SetLength(Size + 1);
			Data[Size] = 0;
			Lines = max(CountLines(Data), 1);
		}

		Dirty = FALSE;
	}
	return Status;	
}

bool TextDocument::Export(char *&s, int &size)
{
	return FALSE;
}

bool TextDocument::Lock(TextLock *Lock, int StartLine, int Ls, int CodePageId)
{
	bool Status = FALSE;
	if (Lock AND StartLine < Lines)
	{
		Lock->StartLine = limit(StartLine, 0, Lines);
		Lock->Lines = min(Ls, Lines - StartLine);
		
		ushort *CodePage = (CodePageId < TVCP_MAX) ? CodePages[CodePageId] : 0;
		if (CodePage)
		{
			Lock->LineW = new ushort*[Lock->Lines];
		}
		else
		{
			Lock->Line = new char*[Lock->Lines];
		}

		char *c = FindLine(StartLine);
		if (Lock->Line)
		{
			for (int i=0; i<Lock->Lines; i++)
			{
				if (c)
				{
					if (CodePage)
					{
						char *Eol = c + StrLen(c);
						int Len = (int)Eol-(int)c;
						Lock->LineW[i] = new ushort[Len + 1];
						if (Lock->LineW[i])
						{
							for (int n=0; n<Len; n++)
							{
								Lock->LineW[i][n] = (c[n] & 0x80) ? CodePage[n-0x80] : c[n];
							}
							Lock->LineW[i][Len] = 0;
						}
					}
					else
					{
						Lock->Line[i] = (*c) ? c : 0;
					}
					for (; *c AND *c != '\n'; c++);
					if (*c) c++;
				}
				else
				{
					if (CodePage)
					{
						Lock->LineW[i] = 0;
					}
					else
					{
						Lock->Line[i] = 0;
					}
				}
			}

			LockCount++;
			Status = TRUE;
		}
	}
	return Status;
}

void TextDocument::UnLock(TextLock *Lock)
{
	LockCount = max(LockCount-1, 0);
}

bool TextDocument::MoveLock(TextLock *Lock, int Dy)
{
	bool Status = FALSE;
	if (Lock)
	{
	}
	return Status;
}

bool TextDocument::CursorCreate(GCursor *c, int X, int Y)
{
	bool Status = FALSE;
	if (c)
	{
		char *StartOfLine = FindLine(Y);

		c->Parent = this;
		c->y = Y;
		c->Length = StrLen(StartOfLine);
		c->Offset = (StartOfLine) ? ((long) StartOfLine - (long) Data) : 0;
		c->x = min(X, c->Length);

		Status = TRUE;
	}
	return Status;
}

bool TextDocument::Insert(GCursor *At, char *Text, int Len)
{
	bool Status = FALSE;

	if (Text AND Editable)
	{
		if (Len < 0)
		{
			Len = strlen(Text);
		}

		int OldLength = Length;
		if (SetLength(Length + Len))
		{
			int Offset = (At) ? At->Offset + At->x : 0;

			// save undo info
			if (NOT IgnoreUndo)
			{
				TruncateQueue();

				// add new undo info in
				if (UserAction *a = new UserAction)
				{
					a->Text = NewStr(Text, Len);
					a->x = At->x;
					a->y = At->y;
					a->Insert = true;
					Queue.Insert(a);
					UndoPos = Queue.GetItems();
				}
			}

			memmove(Data + Offset + Len, Data + Offset, OldLength - Offset + 1);
			memcpy(Data + Offset, Text, Len);
			if (At)
			{
				At->Length = StrLen(Data + At->Offset);
			}
			Lines += CountLines(Text, Len);

			Status = true;
			Dirty = true;
		}
	}

	return Status;
}

bool TextDocument::Delete(GCursor *From, int Len, char *Buffer)
{
	bool Status = FALSE;

	if (From AND Editable)
	{
		int Offset = From->Offset + From->x;

		Len = min(Length - Offset, Len); // -1 is for NULL terminator
		if (Len > 0)
		{
			char *c = Data + Offset;
			if (NOT IgnoreUndo)
			{
				TruncateQueue();

				// add new undo info in
				if (UserAction *a = new UserAction)
				{
					a->Text = NewStr(c, Len);
					a->x = From->x;
					a->y = From->y;
					a->Insert = false;
					Queue.Insert(a);
					UndoPos = Queue.GetItems();
				}
			}

			if (Buffer)
			{
				memcpy(Buffer, c, Len);
			}
			
			Lines -= CountLines(c, Len);
			memmove(c, c + Len, Length - Offset - Len + 1);
	
			Status = SetLength(Length - Len);
			From->Length = StrLen(Data + From->Offset);
			Dirty = TRUE;
		}
	}

	return Status;
}

void TextDocument::TruncateQueue()
{
	// delete undo information after current pos
	int Num = Queue.GetItems() - UndoPos;
	for (int i=0; i<Num; i++)
	{
		UserAction *a = Queue.Last();
		if (a)
		{
			Queue.Delete(a);
			DeleteObj(a);
		}
	}
}

void TextDocument::ApplyAction(UserAction *a, GCursor &c, bool Reverse)
{
	if (a)
	{
		// reverse action
		IgnoreUndo++;

		if (CursorCreate(&c, a->x, a->y))
		{
			if (a->Insert ^ Reverse)
			{
				// delete text
				Delete(&c, strlen(a->Text));
			}
			else
			{
				// insert text
				Insert(&c, a->Text);
			}
		}

		IgnoreUndo--;
	}
}

void TextDocument::Undo(GCursor &c)
{
	if (UndoPos > 0)
	{
		ApplyAction(Queue.ItemAt(UndoPos - 1), c, false);
		UndoPos--;
	}
}

void TextDocument::Redo(GCursor &c)
{
	if (UndoPos < Queue.GetItems())
	{
		ApplyAction(Queue.ItemAt(UndoPos), c, true);
		UndoPos++;
	}
}

void TextDocument::ClearUndoQueue()
{
	for (UserAction *a = Queue.First(); a; a = Queue.Next())
	{
		DeleteObj(a);
	}

	Queue.Empty();
	UndoPos = 0;
}

bool TextDocument::UndoAvailable(bool Redo)
{
	return false;
}

/****************************** TextView Class ******************************************************************************/
TextView::TextView()
{
	Flags = 0;
	HiddenLines = 0;
	DisplayLines = 1;
	ClipData = NULL;
	StatusMsg = NULL;
	WrapType = TEXTED_WRAP_NONE;
	CodePage = TVCP_US_ASCII;

	Doc.CursorCreate(&User, 0, 0);
}

TextView::~TextView()
{
	DeleteArray(ClipData);
	DeleteArray(StatusMsg);
}

void TextView::SetStatus(char *Msg)
{
	if (Msg)
	{
		DeleteArray(StatusMsg);
		StatusMsg = new char[strlen(Msg)+1];
		if (StatusMsg)
		{
			strcpy(StatusMsg, Msg);
		}
	}
}

bool TextView::ClipText(char *Str, int Len)
{
	bool Status = FALSE;
	if (Str)
	{
		if (Len < 0)
		{
			Len = strlen(Str);
		}

		DeleteArray(ClipData);
		ClipData = new char[Len+1];
		if (ClipData)
		{
			memcpy(ClipData, Str, Len);
			ClipData[Len] = 0;
			Status = TRUE;
		}
	}

	return Status;
}

char *TextView::ClipText()
{
	return ClipData;
}

int TextView::ProcessKey(GKey &K)
{
	Flags &= ~(TVF_GOTO_START | TVF_GOTO_END | TVF_EAT_MOVE);

	if (NOT K.IsChar)
	{
		if (K.Shift())
		{
			Flags |= TVF_SHIFT;
		}
		else
		{
			Flags &= ~(TVF_SHIFT);
		}

		if (K.Down())
		{
			switch (K.c)
			{
				case VK_ESCAPE:
				{
					OnEscape(K);
					break;
				}
				case VK_RETURN:
				{
					OnEnter(K);
					break;
				}
				case VK_BACK:
				{
					if (K.Alt())
					{
						if (K.Ctrl())
						{
							Doc.Redo(User);
						}
						else
						{
							Doc.Undo(User);
						}

						Dirty(TVF_DIRTY_ALL);
					}
					else
					{
						if (Flags & TVF_SELECTION)
						{
							OnDeleteSelection(FALSE);
							UpdateHiddenCheck();
						}
						else if (User.Y() > 0 OR User.X() > 0)
						{
							Flags &= ~(TVF_SHIFT);
							OnMoveCursor(-1, 0);
							OnDeleteText(&User, 1, FALSE);
						}
					}
					break;
				}
				case VK_RIGHT:
				case VK_LEFT:
				{
					bool bLeft = K.c == VK_LEFT;
					int Inc = (bLeft) ? -1 : 1;

					if (bLeft)
					{
						Flags |= TVF_GOTO_START;
					}
					else
					{
						Flags |= TVF_GOTO_END;
					}

					Flags |= TVF_EAT_MOVE;

					if (K.Ctrl())
					{
						char *c = User;

						if (bLeft)
						{
							OnMoveCursor(Inc, 0);
							c = User;
							while (	strchr(WhiteSpace, *c) AND
									OnMoveCursor(Inc, 0))
							{
								c = User;
							}
						}

						if (strchr(Delimiters, *c))
						{
							while (	strchr(Delimiters, *c) AND
									OnMoveCursor(Inc, 0))
							{
								c = User;
							}
						}
						else
						{
							// IsText(*c)
							while (	NOT strchr(WhiteSpace, *c) AND
									OnMoveCursor(Inc, 0))
							{
								c = User;
							}
						}

						if (bLeft)
						{
							if (User.Y() > 0 OR User.X() > 0)
							{
								OnMoveCursor(-Inc, 0);
							}
						}
						else
						{
							while (	strchr(WhiteSpace, *c) AND
									OnMoveCursor(Inc, 0))
							{
								c = User;
							}
						}
					}
					else
					{
						OnMoveCursor(Inc, 0);
					}
					break;
				}
				case VK_UP:
				{
					Flags |= TVF_GOTO_START;
					OnMoveCursor(0, -1);
					break;
				}
				case VK_DOWN:
				{
					Flags |= TVF_GOTO_END;
					OnMoveCursor(0, 1);
					break;
				}
				case VK_PRIOR:
				{
					int Move = 1-DisplayLines;
					OnSetHidden(HiddenLines+Move);
					OnMoveCursor(0, Move);
					break;
				}
				case VK_NEXT:
				{
					int Move = DisplayLines-1;
					OnSetHidden(HiddenLines+Move);
					OnMoveCursor(0, Move);
					break;
				}
				case VK_HOME:
				{
					if (K.Ctrl())
					{
						OnSetCursor(0, 0);
					}
					else
					{
						OnMoveCursor(-User.X(), 0);
					}
					break;
				}
				case VK_END:
				{
					if (K.Ctrl())
					{
						OnSetCursor(1024, Doc.GetLines());
					}
					else
					{
						OnMoveCursor(User.LineLength()-User.X(), 0);
					}
					break;
				}
				case VK_DELETE:
				{
					if (	K.Shift() AND
							(Flags & TVF_SELECTION))
					{
						Cut();
					}
					else
					{
						if (Flags & TVF_SELECTION)
						{
							OnDeleteSelection(FALSE);
							UpdateHiddenCheck();
						}
						else if (	User.Y() < Doc.GetLines() OR
									User.X() < User.LineLength())
						{
							OnDeleteText(&User, 1, FALSE);
						}
					}
					break;
				}
				case VK_INSERT:
				{
					if (Flags & TVF_SELECTION)
					{
						if (K.Ctrl())
						{
							Copy();
						}
					}

					if (K.Shift())
					{
						Paste();
					}
					break;
				}
			}
		}
	}
	else
	{
		// IsChar
		#define EXTENDED (1<<24)

		// if (	NOT (K.Data & EXTENDED))
		{
			bool InsertChar = K.c >= ' '; //  AND K.c < 128

			if (K.c == 9)
			{
				if ((Flags & TVF_SELECTION) AND Start.Y() != End.Y())
				{
					if (Flags & TVF_SHIFT)
					{
						InsertChar = NOT OnMultiLineTab(false);
					}
					else
					{
						InsertChar = NOT OnMultiLineTab(true);
					}
				}
				else
				{
					InsertChar = true;
				}
			}
			
			if (InsertChar)
			{
				char Char[2] = " ";
				Char[0] = K.c;
				if (OnInsertText(Char))
				{
					OnMoveCursor(1, 0, true);
				}
			}
		}
	}

	return TRUE;
}

bool TextView::IsDirty()
{
	return Flags & TVF_DIRTY_MASK;
}

void TextView::Dirty(int Type)
{
	Flags |= Type;
}

void TextView::Clean()
{
	Flags &= ~TVF_DIRTY_MASK;
}

bool TextView::OnInsertText(char *Text, int Len)
{
	bool Status = false;

	OnDeleteSelection(FALSE);

	int OldLines = Doc.GetLines();
	Status = Doc.Insert(&User, Text, Len);
	if (Status)
	{
		if (OldLines != Doc.GetLines())
		{
			Dirty(TVF_DIRTY_TO_EOP);
		}
		else
		{
			Dirty(TVF_DIRTY_TO_EOL);
		}

		AfterInsertText(Text, Len);
	}

	return Status;
}

bool TextView::OnDeleteText(GCursor *c, int Len, bool Clip)
{
	bool Status = false;

	if (Clip)
	{
		ClipText((char*) *c, Len);
	}
	
	int OldLines = Doc.GetLines();
	Status = Doc.Delete(c, Len, (Clip) ? ClipText() : 0);
	if (Status)
	{
		if (Clip)
		{
			Dirty(TVF_DIRTY_ALL);
		}
		else
		{
			if (OldLines != Doc.GetLines())
			{
				Dirty(TVF_DIRTY_TO_EOP);
			}
			else
			{
				Dirty(TVF_DIRTY_TO_EOL);
			}
		}

		AfterDeleteText();
	}

	return Status;
}

bool TextView::UpdateHiddenCheck()
{
	bool Status = FALSE;

	if (User.Y() < HiddenLines)
	{
		HiddenLines = User.Y();
		Dirty(TVF_DIRTY_ALL);
		Status = TRUE;
	}

	if (User.Y() >= HiddenLines + DisplayLines)
	{
		HiddenLines = User.Y() - DisplayLines + 1;
		Dirty(TVF_DIRTY_ALL);
		Status = TRUE;
	}

	return Status;
}

bool TextView::OnMoveCursor(int Dx, int Dy, bool NoSelect)
{
	bool Status = FALSE;
	bool StartSelect = (Flags & TVF_SHIFT) AND NOT (Flags & TVF_SELECTION);
	bool EndSelect = NOT (Flags & TVF_SHIFT) AND (Flags & TVF_SELECTION);
	GCursor Old = User;

	if (EndSelect)
	{
		GCursor S, E;
		if (Start < End)
		{
			S = Start;
			E = End;
		}
		else
		{
			S = End;
			E = Start;
		}

		if (Flags & TVF_GOTO_START)
		{
			User = S;
		}
		
		if (Flags & TVF_GOTO_END)
		{
			User = E;
		}

		if (Flags & TVF_EAT_MOVE)
		{
			goto AfterMove;
		}
	}

	if (Dy)
	{
		int OldX = User.X();
		User.MoveY(Dy);
		User.SetX(OldX);
	}
	else
	{
		User.MoveX(Dx);
	}

	if (Old != User OR EndSelect)
	{
		AfterMove:
		if (NOT NoSelect)
		{
			if (StartSelect)
			{
				OnStartSelection(&Old);
			}

			if (EndSelect)
			{
				OnEndSelection();
			}
		}

		Dirty(TVF_DIRTY_CURSOR);
		UpdateHiddenCheck();

		if (Flags & TVF_SELECTION)
		{
			Dirty(TVF_DIRTY_SELECTION);
			End = User;
		}

		Status = TRUE;
	}

	return Status;
}

void TextView::OnSetHidden(int Hidden)
{
	int NewHidden = limit(Hidden, 0, Doc.GetLines()-1);
	if (NewHidden != HiddenLines)
	{
		HiddenLines = NewHidden;
		Dirty(TVF_DIRTY_ALL);
	}
}

void TextView::OnSetCursor(int X, int Y)
{
	GCursor Old = User;

	User.SetY(Y);
	User.SetX(X);

	if (Old != User)
	{
		Dirty(TVF_DIRTY_CURSOR);
		UpdateHiddenCheck();

		if (Flags & TVF_SELECTION)
		{
			Dirty(TVF_DIRTY_SELECTION);
			End = User;
		}
	}
}

void TextView::OnStartSelection(GCursor *c)
{
	Start = *c;
	End = *c;
	Flags |= TVF_SELECTION;
	Dirty(TVF_DIRTY_SELECTION);
}

void TextView::OnEndSelection()
{
	Flags &= ~TVF_SELECTION;
	Dirty(TVF_DIRTY_ALL);
}

void TextView::OnDeleteSelection(bool Clip)
{
	if (Flags & TVF_SELECTION)
	{
		GCursor S;
		int Len;
		
		if (Start < End)
		{
			S = Start;
			Len = End - Start;
		}
		else
		{
			S = End;
			#ifdef CURSOR_UNDER
			S.MoveX(1);
			#endif
			Len = Start - End;
		}

		if (Len > 0)
		{
			User = S;
			OnDeleteText(&User, Len, Clip);
		}

		OnEndSelection();
	}
}

bool TextView::Open(char *Name)
{
	Doc.Load(Name);
	return Doc.CursorCreate(&User, 0, 0);
}

bool TextView::Save(char *Name)
{
	return Doc.Save(Name);
}

bool TextView::Cut()
{
	if (Flags & TVF_SELECTION)
	{
		OnDeleteSelection(TRUE);
		UpdateHiddenCheck();
		return TRUE;
	}
	return FALSE;
}

bool TextView::Copy()
{
	if (Flags & TVF_SELECTION)
	{
		GCursor S;
		int Len;

		if (Start < End)
		{
			S = Start;
			Len = End - Start;
		}
		else
		{
			S = End;
			Len = Start - End;
		}
		
		ClipText((char*) S, Len);
		return TRUE;
	}
	return FALSE;
}

bool TextView::Paste()
{
	if (ClipText())
	{
		char *Text = ClipText();
		
		// Insert text into document
		OnInsertText(Text);

		// Calculate the end point of the inserted text
		int X, Y;
		CountLinesAndLen(Text, strlen(Text), X, Y);

		// Make the cursor point to the end of the inserted text
		if (Y > 0)
		{
			User.MoveY(Y);
			User.SetX(X);
		}
		else
		{
			User.MoveX(X);
		}
	}

	OnEndSelection();
	UpdateHiddenCheck();
	return TRUE;
}

bool TextView::ClearDirty(bool Ask)
{
	return FALSE;
}

void TextView::OnSave()
{
	ClearDirty(FALSE);
}

void TextView::OnGotoLine()
{
}

void TextView::OnEscape(GKey &K)
{
	LgiCloseApp();
}

void TextView::OnEnter(GKey &K)
{
	OnInsertText("\n");
	OnMoveCursor(1, 0, TRUE);
	
	if (bAutoIndent)
	{
		GCursor S = User;
		S.MoveY(-1);
		S.SetX(0);

		char *c = S;
		int Len = 0;
		while (IsWhite(*c) AND Len < S.LineLength())
		{
			Len++;
			c++;
		}

		if (Len > 0)
		{
			OnInsertText(S, Len);
			OnMoveCursor(Len, 0, TRUE);
		}
	}
}

ushort *TextView::GetCodePageMap(int Page)
{
	if (Page < 0) Page = CodePage;
	if (Page >= 0 AND Page < TVCP_MAX)
	{
		return CodePages[Page];
	}

	return 0;
}
