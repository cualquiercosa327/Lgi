#ifndef __GUNDO_H__
#define __GUNDO_H__

class GUndoEvent
{
public:
    virtual ~GUndoEvent() {}
    virtual void ApplyChange() {}
    virtual void RemoveChange() {}
};

class GUndo
{
	int Pos;
	List<GUndoEvent> Events;

public:
	GUndo() { Pos = 0; }
	~GUndo() { Empty(); }

	bool CanUndo() { return (Pos > 0) && (Events.Length() > 0); }
	bool CanRedo() { return (Pos < Events.Length()) && (Events.Length() > 0); }
	void Empty() { Events.DeleteObjects(); Pos = 0; }

	GUndo &operator +=(GUndoEvent *e)
	{
		while (Events.Length() > Pos)
		{
			GUndoEvent *u = Events.Last();
			Events.Delete(u);
			DeleteObj(u);
		}

		Events.Insert(e);
		Pos++;
		return *this;
	}

	void Undo()
	{
		if (CanUndo())
		{
			GUndoEvent *e = Events.ItemAt(Pos-1);
			if (e)
			{
				e->RemoveChange();
				Pos--;
			}
		}
	}

	void Redo()
	{
		if (CanRedo())
		{
			GUndoEvent *e = Events.ItemAt(Pos);
			if (e)
			{
				e->ApplyChange();
				Pos++;
			}
		}
	}
};

#endif
