#include "Lgi.h"
#include "GRichTextEdit.h"
#include "GRichTextEditPriv.h"

GRichTextPriv::BlockCursor::BlockCursor(const BlockCursor &c)
{
	Blk = NULL;
	Blink = true;
	*this = c;
}
		
GRichTextPriv::BlockCursor::BlockCursor(Block *b, ssize_t off, int line)
{
	Blk = NULL;
	Offset = -1;
	Pos.ZOff(-1, -1);
	Line.ZOff(-1, -1);
	LineHint = 0;
	Blink = true;

	if (b)
		Set(b, off, line);
}
		
GRichTextPriv::BlockCursor::~BlockCursor()
{
	Set(NULL, 0, 0);
}
		
GRichTextPriv::BlockCursor &GRichTextPriv::BlockCursor::operator =(const BlockCursor &c)
{
	Set(c.Blk, c.Offset, c.LineHint);
	Pos = c.Pos;
	Line = c.Line;
			
	LgiAssert(Offset >= 0);
			
	return *this;
}
		
void GRichTextPriv::BlockCursor::Set(ssize_t off)
{
	GArray<int> Ln;
	if (!Blk->OffsetToLine(off, NULL, &Ln))
		Ln.Add(-1);

	Set(Blk, off, Ln.First());
}

void GRichTextPriv::BlockCursor::Set(GRichTextPriv::Block *b, ssize_t off, int line)
{
	if (Blk != b)
	{
		if (Blk)
		{
			LgiAssert(Blk->Cursors > 0);
			Blk->Cursors--;

			#if DEBUG_LOG_CURSOR_COUNT
			{
				GArray<char16> Text;
				Blk->CopyAt(0, 20, &Text);
				Text.Add(0);
				LgiTrace("%s:%i - %i del cursor %p (%.20S)\n", _FL, Blk->Cursors, Blk, &Text.First());
			}
			#endif

			Blk = NULL;
		}

		if (b)
		{
			Blk = b;
			LgiAssert(Blk->Cursors < 0x7f);
			Blk->Cursors++;

			#if DEBUG_LOG_CURSOR_COUNT
			{
				GArray<char16> Text;
				Blk->CopyAt(0, 20, &Text);
				Text.Add(0);
				LgiTrace("%s:%i - %i add cursor %p (%.20S)\n", _FL, Blk->Cursors, Blk, &Text.First());
			}
			#endif
		}
	}

	Offset = off;
	LineHint = line;
	if (Blk)
	{
		GRect Line;
		ssize_t BlkLen = Blk->Length();
		LgiAssert(off >= 0 && off <= BlkLen);
		Blk->GetPosFromIndex(this);
	}

	// LOG_FN("%s:%i - Cursor.Set: %i, Line: %i\n", _FL, Offset, LineHint);
			
	LgiAssert(Offset >= 0);
}
