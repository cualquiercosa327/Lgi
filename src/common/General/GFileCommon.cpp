//
//  GFileCommon.cpp
//  LgiCarbon
//
//  Created by Matthew Allen on 4/05/14.
//  Copyright (c) 2014 Memecode. All rights reserved.
//

#include "Lgi.h"
#include "GVariant.h"

bool GFile::GetVariant(const char *Name, GVariant &Value, char *Array)
{
	GDomProperty p = GStringToProp(Name);
	switch (p)
	{
		case ObjType:
			Value = "File";
			break;
		case ObjName:
			Value = GetName();
			break;
		case ObjLength:
			Value = GetSize();
			break;
		case FilePos:
			Value = GetPos();
			break;
		default:
			return false;
	}
	
	return true;
}

bool GFile::SetVariant(const char *Name, GVariant &Value, char *Array)
{
	GDomProperty p = GStringToProp(Name);
	switch (p)
	{
		case ObjLength:
			SetSize(Value.CastInt64());
			break;
		case FilePos:
			SetPos(Value.CastInt64());
			break;
		default:
			return false;
	}
	
	return true;
}

bool GFile::CallMethod(const char *Name, GVariant *Dst, GArray<GVariant*> &Arg)
{
	GDomProperty p = GStringToProp(Name);
	switch (p)
	{
		case ObjLength:
		{
			if (Arg.Length() == 1)
				*Dst = SetSize(Arg[0]->CastInt64());
			else
				*Dst = GetSize();
			break;
		}
		case FilePos:
		{
			if (Arg.Length() == 1)
				*Dst = SetPos(Arg[0]->CastInt64());
			else
				*Dst = GetPos();
			break;
		}
		case ObjType:
		{
			*Dst = "File";
			break;
		}
		case FileOpen:
		{
			if (Arg.Length() >= 1)
			{
				int Mode = O_READ;
				if (Arg.Length() == 2)
				{
					char *m = Arg[1]->CastString();
					if (m)
					{
						bool Rd = strchr(m, 'r');
						bool Wr = strchr(m, 'w');
						if (Rd && Wr)
							Mode = O_READWRITE;
						else if (Wr)
							Mode = O_WRITE;
						else
							Mode = O_READ;
					}
				}
				
				*Dst = Open(Arg[0]->CastString(), Mode);
			}
			break;
		}
		case FileClose:
		{
			*Dst = Close();
			break;
		}
		case FileRead:
		{
			int64 RdLen = 0;
			int RdType = 0; // 0 - string, 1 - int
			
			Dst->Empty();
			
			switch (Arg.Length())
			{
				default:
				case 0:
					RdLen = GetSize() - GetPos();
					break;
				case 2:
					RdType = Arg[1]->CastInt32();
					// fall thru
				case 1:
					RdLen = Arg[0]->CastInt64();
					break;
			}
			
			if (RdType)
			{
				// Int type
				switch (RdLen)
				{
					case 1:
					{
						uint8 i;
						if (Read(&i, sizeof(i)) == sizeof(i))
							*Dst = i;
						break;
					}
					case 2:
					{
						uint16 i;
						if (Read(&i, sizeof(i)) == sizeof(i))
							*Dst = i;
						break;
					}
					case 4:
					{
						uint32 i;
						if (Read(&i, sizeof(i)) == sizeof(i))
							*Dst = (int)i;
						break;
					}
					case 8:
					{
						int64 i;
						if (Read(&i, sizeof(i)) == sizeof(i))
							*Dst = i;
						break;
					}
				}
			}
			else if (RdLen > 0)
			{
				// String type
				if ((Dst->Value.String = new char[RdLen + 1]))
				{
					int r = Read(Dst->Value.String, (int)RdLen);
					if (r > 0)
					{
						Dst->Type = GV_STRING;
						Dst->Value.String[r] = 0;
					}
					else
					{
						DeleteArray(Dst->Value.String);
					}
				}
			}
			else *Dst = -1;
			break;
		}
		case FileWrite:
		{
			GVariant *v;
			if (Arg.Length() < 1 ||
				Arg.Length() > 2 ||
				!(v = Arg[1]))
			{
				*Dst = 0;
				return true;
			}

			switch (Arg.Length())
			{
				case 1:
				{
					// Auto-size write length to the variable.
					switch (v->Type)
					{
						case GV_INT32:
							*Dst = Write(&v->Value.Int, sizeof(v->Value.Int));
							break;
						case GV_INT64:
							*Dst = Write(&v->Value.Int64, sizeof(v->Value.Int64));
							break;
						case GV_STRING:
							*Dst = Write(&v->Value.String, strlen(v->Value.String));
							break;
						case GV_WSTRING:
							*Dst = Write(&v->Value.WString, StrlenW(v->Value.WString) * sizeof(char16));
							break;
						default:
							*Dst = 0;
							return true;
					}
					break;
				}
				case 2:
				{
					int64 WrLen = Arg[1]->CastInt64();
					switch (v->Type)
					{
						case GV_INT32:
						{
							if (WrLen == 1)
							{
								uint8 i = v->Value.Int;
								*Dst = Write(&i, sizeof(i));
							}
							else if (WrLen == 2)
							{
								uint16 i = v->Value.Int;
								*Dst = Write(&i, sizeof(i));
							}
							else
							{
								*Dst = Write(&v->Value.Int, sizeof(v->Value.Int));
							}
							break;
						}
						case GV_INT64:
						{
							if (WrLen == 1)
							{
								uint8 i = v->Value.Int64;
								*Dst = Write(&i, sizeof(i));
							}
							else if (WrLen == 2)
							{
								uint16 i = v->Value.Int64;
								*Dst = Write(&i, sizeof(i));
							}
							else if (WrLen == 4)
							{
								uint32 i = v->Value.Int64;
								*Dst = Write(&i, sizeof(i));
							}
							else
							{
								*Dst = Write(&v->Value.Int64, sizeof(v->Value.Int64));
							}
							break;
						}
						case GV_STRING:
						{
							int Max = strlen(v->Value.String) + 1;
							*Dst = Write(&v->Value.String, min(Max, WrLen));
							break;
						}
						case GV_WSTRING:
						{
							int Max = (StrlenW(v->Value.WString) + 1) * sizeof(char16);
							*Dst = Write(&v->Value.WString, min(Max, WrLen));
							break;
						}
						default:
						{
							*Dst = 0;
							return true;
						}
					}
					break;
				}
				default:
				{
					*Dst = 0;
					return true;
				}
			}
			break;
		}
		default:
			return false;
	}
	return true;
}

