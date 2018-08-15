#include "Lgi.h"
#include "Store.h"
#include "Store2.h"
#include "StoreConvert1To2.h"
#include "GProgressDlg.h"

class ReaderItem : public StorageObj
{
	int Type() { LgiAssert(0); return 0; }
	int Sizeof() { LgiAssert(0); return 0; }
	bool Serialize(GFile &f, bool Write) { LgiAssert(0); return 0; }

public:
	int Len;
	uchar *Data;

	ReaderItem(StorageItem *Item)
	{
		Len = 0;
		Data = 0;
		Store = Item;
		Store->Object = this;

		GFile *f = Store->GotoObject(__FILE__, __LINE__);
		if (f)
		{
			Len = ((Storage1::StorageItemImpl*)Store)->StoreSize;
			if (Len <= 0)
			{
				Len = ((Storage1::StorageItemImpl*)Store)->StoreAllocatedSize;
			}

			LgiAssert(Len > 0);

			Data = new uchar[Len];
			if (Data)
			{
				f->Read(Data, Len);
			}
			
			DeleteObj(f);
		}
	}

	~ReaderItem()
	{
		DeleteArray(Data);
	}
};

class WriterItem : public StorageObj
{
public:
	ReaderItem *Data;

	WriterItem(ReaderItem *data)
	{
		Data = data;
	}

	int Type()
	{
		return Data ? Data->Store->GetType() : 0;
	}

	int Sizeof()
	{
		if (Data)
		{
			return Data->Len;
		}

		return 0;
	}

	bool Serialize(GFile &f, bool Write)
	{
		if (Data && Write)
		{
			f.Write(Data->Data, Data->Len);
			return true;
		}

		return false;
	}

	bool Write()
	{
		return Store->Save();
	}
};

class StoreConverter1To2
{
	Storage1::StorageKitImpl *Store1;
	Storage2::StorageKitImpl *Store2;

	Progress *Prog;
	int Failed;
	int Converted;

public:
	StoreConverter1To2(char *in, char *out)
	{
		Failed = 0;
		Converted = 0;
		Prog = 0;

		Store1 = new Storage1::StorageKitImpl(in);
		Store2 = new Storage2::StorageKitImpl(out);
	}

	bool ConvertItem(ReaderItem *Reader, WriterItem *Writer)
	{
		bool Status = false;

		if (Reader && Writer)
		{
			// convert object across
			if (Prog)
			{
				Prog->Value(Prog->Value() + Reader->Len);
				LgiYield();
			}

			Converted++;
			Status = true;

			// do children
			for (StorageItem *ri = Reader->Store->GetChild(); ri;
				ri = ri->GetNext())
			{
				ReaderItem *Reader1 = new ReaderItem(ri);
				if (Reader1 &&
					Reader1->Len > 0)
				{
					WriterItem *Writer2 = new WriterItem(Reader1);
					StorageItem *wi = Writer->Store->CreateSub(Writer2);
					if (Writer2 && wi)
					{
						wi->Object = Writer2;
						Writer2->Store = wi;

						Status &= ConvertItem(Reader1, Writer2);

						Writer2->Store->Object = 0;
						Writer2->Store = 0;
						DeleteObj(Writer2);
					}

					Reader1->Store->Object = 0;
					Reader1->Store = 0;
					DeleteObj(Reader1);
				}
			}
		}

		return Status;
	}

	bool Convert(GView *Parent)
	{
		bool Status = false;
		GProgressDlg Dlg(Parent);

		if (Store1 &&
			Store2)
		{
			if (Store1->GetStatus())
			{
				Prog = Dlg.ItemAt(0);
				Dlg.SetDescription("Converting items...");
				Dlg.SetLimits(0, Store1->GetFileSize());
				Dlg.SetScale(1.0 / 1024.0);
				Dlg.SetType("K");

				StorageItem *Item1 = Store1->GetRoot();
				ReaderItem *Reader1 = new ReaderItem(Item1);
				if (Reader1)
				{
					// The storage1 code didn't set the root object's type
					// correctly, leaving it as -1. So on the way over lets
					// correct this little mistake and make it into a folder.
					Reader1->Store->SetType(0xAAFF0003); // magic # for a folder
				}

				WriterItem *Writer2 = new WriterItem(Reader1);
				StorageItem *Item2 = Store2->CreateRoot(Writer2);
				if (Writer2 && Item2)
				{
					Writer2->Store = Item2;
					Item2->Object = Writer2;
				}

				if (Reader1 && Writer2)
				{
					Status = ConvertItem(Reader1, Writer2);

					Reader1->Store->Object = 0;
					Reader1->Store = 0;
					Writer2->Store->Object = 0;
					Writer2->Store = 0;
				}
			}
			else
			{
				GStatusPane *Wnd = dynamic_cast<GStatusPane*>(Prog);
				LgiMsg(	Wnd,
						"The input folders failed to load correctly.\n"
						"Most likely because they are not v1 folders or\n"
						"are corrupt.",
						"Scribe: Folder Compact",
						MB_OK);
			}

			DeleteObj(Store1);
			DeleteObj(Store2);
		}

		return Status;
	}
};

bool ConvertStorage1To2(GView *Parent, char *InFile, char *OutFile)
{
	GFileSelect In;
	In.Parent(Parent);
	In.Type("Mail folders", "*.mail");
	if (!InFile &&
		In.Open())
	{
		InFile = In.Name();
	}

	if (InFile)
	{
		GFileSelect Out;
		Out.Parent(Parent);
		Out.Type("Mail folders", "*.mail2");
		if ((!OutFile || strlen(OutFile) == 0) &&
			Out.Save())
		{
			if (OutFile)
			{
				strcpy(OutFile, Out.Name());
			}
			else
			{
				OutFile = Out.Name();
			}
		}

		if (OutFile)
		{
			StoreConverter1To2 Converter(InFile, OutFile);
			return Converter.Convert(Parent);
		}
	}

	return false;
}
