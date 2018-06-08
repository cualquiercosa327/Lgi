#ifndef _IdeDocPrivate_h_
#define _IdeDocPrivate_h_

#define EDIT_CTRL_WIDTH		200

class EditTray : public GLayout
{
	GRect FileBtn;
	GEdit *FileSearch;

	GRect FuncBtn;
	GEdit *FuncSearch;

	GRect SymBtn;
	GEdit *SymSearch;

	GCheckBox *AllPlatforms;

	GRect TextMsg;

	GTextView3 *Ctrl;
	IdeDoc *Doc;

public:
	int Line, Col;

	EditTray(GTextView3 *ctrl, IdeDoc *doc);
	~EditTray();
	
	void GotoSearch(int CtrlId, char *InitialText = NULL);	
	void OnCreate();
	void OnPosChange();
	void OnPaint(GSurface *pDC);
	bool Pour(GRegion &r);
	void OnMouseClick(GMouse &m);
	void OnHeaderList(GMouse &m);
	void OnFunctionList(GMouse &m);
	void OnSymbolList(GMouse &m);
};

class IdeDocPrivate : public NodeView, public LMutex
{
	GString FileName;
	GString Buffer;

public:
	IdeDoc *Doc;
	AppWnd *App;
	IdeProject *Project;
	bool IsDirty;
	LDateTime ModTs;
	class DocEdit *Edit;
	EditTray *Tray;
	GHashTbl<int, bool> BreakPoints;
	class ProjFilePopup *FilePopup;
	class ProjMethodPopup *MethodPopup;
	class ProjSymPopup *SymPopup;
	GString::Array WriteBuf;
	GAutoPtr<LThread> Build;
	
	IdeDocPrivate(IdeDoc *d, AppWnd *a, NodeSource *src, const char *file);
	void OnDelete();
	void UpdateName();
	GString GetDisplayName();
	bool IsFile(const char *File);
	char *GetLocalFile();
	void SetFileName(const char *f);
	bool Load();
	bool Save();
	void OnSaveComplete(bool Status);

	LDateTime GetModTime()
	{
		LDateTime Ts;

		GString Full = nSrc ? nSrc->GetFullPath() : FileName;
		if (Full)
		{
			GDirectory Dir;
			if (Dir.First(Full, NULL))
				Ts.Set(Dir.GetLastWriteTime());
		}

		return Ts;
	}

	void CheckModTime();
};

#endif