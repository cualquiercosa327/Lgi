#include "Lgi.h"
#include "GTnef.h"
#include "GVariant.h"

#define TNEF_SIGNATURE  ((uint32) 0x223E9F78)

#define atpTriples      ((uint16) 0x0000)
#define atpString       ((uint16) 0x0001)
#define atpText         ((uint16) 0x0002)
#define atpDate         ((uint16) 0x0003)
#define atpShort        ((uint16) 0x0004)
#define atpLong         ((uint16) 0x0005)
#define atpByte         ((uint16) 0x0006)
#define atpWord         ((uint16) 0x0007)
#define atpDword        ((uint16) 0x0008)
#define atpMax          ((uint16) 0x0009)

#define LVL_MESSAGE     ((uint8) 0x01)
#define LVL_ATTACHMENT  ((uint8) 0x02)

#define ATT_ID(_att)                ((uint16) ((_att) & 0x0000FFFF))
#define ATT_TYPE(_att)              ((uint16) (((_att) >> 16) & 0x0000FFFF))
#define ATT(_atp, _id)              ((((uint32) (_atp)) << 16) | ((uint16) (_id)))

#define attNull                     ATT( 0,             0x0000)
#define attFrom                     ATT( atpTriples,    0x8000) /* PR_ORIGINATOR_RETURN_ADDRESS */
#define attSubject                  ATT( atpString,     0x8004) /* PR_SUBJECT */
#define attDateSent                 ATT( atpDate,       0x8005) /* PR_CLIENT_SUBMIT_TIME */
#define attDateRecd                 ATT( atpDate,       0x8006) /* PR_MESSAGE_DELIVERY_TIME */
#define attMessageStatus            ATT( atpByte,       0x8007) /* PR_MESSAGE_FLAGS */
#define attMessageClass             ATT( atpWord,       0x8008) /* PR_MESSAGE_CLASS */
#define attMessageID                ATT( atpString,     0x8009) /* PR_MESSAGE_ID */
#define attParentID                 ATT( atpString,     0x800A) /* PR_PARENT_ID */
#define attConversationID           ATT( atpString,     0x800B) /* PR_CONVERSATION_ID */
#define attBody                     ATT( atpText,       0x800C) /* PR_BODY */
#define attPriority                 ATT( atpShort,      0x800D) /* PR_IMPORTANCE */
#define attAttachData               ATT( atpByte,       0x800F) /* PR_ATTACH_DATA_xxx */
#define attAttachTitle              ATT( atpString,     0x8010) /* PR_ATTACH_FILENAME */
#define attAttachMetaFile           ATT( atpByte,       0x8011) /* PR_ATTACH_RENDERING */
#define attAttachCreateDate         ATT( atpDate,       0x8012) /* PR_CREATION_TIME */
#define attAttachModifyDate         ATT( atpDate,       0x8013) /* PR_LAST_MODIFICATION_TIME */
#define attDateModified             ATT( atpDate,       0x8020) /* PR_LAST_MODIFICATION_TIME */
#define attAttachTransportFilename  ATT( atpByte,       0x9001) /* PR_ATTACH_TRANSPORT_NAME */
#define attAttachRenddata           ATT( atpByte,       0x9002)
#define attMAPIProps                ATT( atpByte,       0x9003)
#define attRecipTable               ATT( atpByte,       0x9004) /* PR_MESSAGE_RECIPIENTS */
#define attAttachment               ATT( atpByte,       0x9005)
#define attTnefVersion              ATT( atpDword,      0x9006)
#define attOemCodepage              ATT( atpByte,       0x9007)
#define attOriginalMessageClass     ATT( atpWord,       0x0006) /* PR_ORIG_MESSAGE_CLASS */

#define attOwner                    ATT( atpByte,       0x0000) /* PR_RCVD_REPRESENTING_xxx  or
                                                                   PR_SENT_REPRESENTING_xxx */
#define attSentFor                  ATT( atpByte,       0x0001) /* PR_SENT_REPRESENTING_xxx */
#define attDelegate                 ATT( atpByte,       0x0002) /* PR_RCVD_REPRESENTING_xxx */
#define attDateStart                ATT( atpDate,       0x0006) /* PR_DATE_START */
#define attDateEnd                  ATT( atpDate,       0x0007) /* PR_DATE_END */
#define attAidOwner                 ATT( atpLong,       0x0008) /* PR_OWNER_APPT_ID */
#define attRequestRes               ATT( atpShort,      0x0009) /* PR_RESPONSE_REQUESTED */

struct DTR
{
    uint16    wYear;
    uint16    wMonth;
    uint16    wDay;
    uint16    wHour;
    uint16    wMinute;
    uint16    wSecond;
    uint16    wDayOfWeek;
};

class TnefAttr
{
public:
	int64 StreamPos;
	uint16 Tag;
	uint16 Type;
	uint32 Size;
	GVariant Value;

	uint32 Prop()
	{
		return ATT(Type, Tag);
	}

	int64 DataStart()
	{
		return StreamPos + 8;
	}

	bool Read(GStreamI *s)
	{
		StreamPos = s->GetPos();

		if (s->Read(&Tag, sizeof(Tag)) == sizeof(Tag) &&
			s->Read(&Type, sizeof(Type)) == sizeof(Tag) &&
			s->Read(&Size, sizeof(Size)) == sizeof(Size))
		{
			uint16 EffectiveType = Type;
			if (Prop() == attMessageClass)
			{
				EffectiveType = atpString;
			}

			switch (EffectiveType)
			{
				case atpDword:
				{
					uint32 d;
					if (Size == sizeof(d) &&
						s->Read(&d, sizeof(d)) == sizeof(d))
					{
						Value = (int)d;
					}
					else
					{
						LgiTrace("Tnef: dword size error %i\n", Size);
						return false;
					}
					break;
				}
				case atpByte:
				{
					char *b = new char[Size];
					if (b)
					{
						s->Read(b, Size);
						Value.SetBinary(Size, b);
						DeleteArray(b);
					}
					break;
				}
				case atpString:
				{
					char *b = new char[Size+1];
					if (b)
					{
						s->Read(b, Size);
						b[Size] = 0;
						Value = b;
						DeleteArray(b);
					}
					break;
				}
				case atpDate:
				{
					DTR d;
					if (sizeof(d) == Size &&
						s->Read(&d, sizeof(d)))
					{
						GDateTime dt;
						dt.Year(d.wYear);
						dt.Month(d.wMonth);
						dt.Day(d.wDay);
						dt.Hours(d.wHour);
						dt.Minutes(d.wMinute);
						dt.Seconds(d.wSecond);

						Value = &dt;
					}
					break;
				}
				default:
				case atpText:
				case atpShort:
				case atpLong:
				case atpWord:
				{
					LgiTrace("Tnef: unsupported type %i\n", EffectiveType);
					break;
				}
			}

			uint16 Checksum;
			s->Read(&Checksum, sizeof(Checksum));
		}
		else return false;

		return true;
	}
};

bool TnefReadIndex(GStreamI *Tnef, GArray<TnefFileInfo*> &Index)
{
	bool Status = false;

	if (Tnef)
	{
		uint32 Sig;
		uint16 Key;
		TnefFileInfo *Cur = 0;

		if (Tnef->Read(&Sig, sizeof(Sig)) &&
			Tnef->Read(&Key, sizeof(Key)) &&
			Sig == TNEF_SIGNATURE &&
			Key > 0)
		{
			uint8 b;
			bool Done = false;
			GArray<uint32> Tags;
			while (!Done && Tnef->Read(&b, sizeof(b)) == sizeof(b))
			{
				switch (b)
				{
					case LVL_MESSAGE:
					{
						TnefAttr a;
						if (a.Read(Tnef))
						{
							switch (a.Prop())
							{
								case attTnefVersion:
								{
									break;
								}
								case attOemCodepage:
								{
									break;
								}
								case attMessageClass:
								{
									break;
								}
								case attMAPIProps:
								{
									break;
								}
								default:
								{
									break;
								}
							}
						}
						else
						{
							LgiTrace("Tnef: failed to read msg attribute\n");
						}
						break;
					}
					case LVL_ATTACHMENT:
					{
						TnefAttr a;
						if (a.Read(Tnef))
						{
							if (!Cur || Tags.HasItem(a.Prop()))
							{
								Index.Add(Cur = new TnefFileInfo);
								Tags.Length(0);
							}

							Tags.Add(a.Prop());

							switch (a.Prop())
							{
								case attAttachRenddata:
								{
									break;
								}
								case attAttachModifyDate:
								{
									break;
								}
								case attAttachData:
								{
									if (Cur)
									{
										Cur->Start = a.DataStart();
										Cur->Size = a.Size;
									}
									break;
								}
								case attAttachTitle:
								{
									if (Cur)
									{
										Cur->Name = NewStr(a.Value.Str());
									}
									break;
								}
								case attAttachMetaFile:
								{
									break;
								}
								case attAttachment:
								{
									if (Cur)
									{
										Cur->Start = a.DataStart();
										Cur->Size = a.Size;
									}
									break;
								}
								default:
								{
									int asd=0;
									break;
								}
							}
						}
						else
						{
							LgiTrace("Tnef: failed to read attachment attribute\n");
						}
						break;
					}
					default:
					{
						Done = true;
						LgiTrace("Tnef: got strange header byte %i\n", b);
						break;
					}
				}
			}

			for (int i=0; i<Index.Length(); i++)
			{
				TnefFileInfo *fi = Index[i];
				if (!fi->Name ||
					!fi->Size ||
					!fi->Start)
				{
					DeleteObj(Index[i]);
					Index.DeleteAt(i--);
				}
				else
				{
					Status = true;
				}
			}
		}
	}

	return Status;
}

bool TnefExtract(GStreamI *Tnef, GStream *Out, TnefFileInfo *File)
{
	bool Status = false;

	if (Tnef && Out && File)
	{
		if (Tnef->SetPos(File->Start) == File->Start)
		{
			char Buf[512];
			int64 i;
			for (i=0; i<File->Size; )
			{
				int Block = min(sizeof(Buf), File->Size - i);
				int r = Tnef->Read(Buf, Block);
				if (r > 0)
				{
					i += r;
					Out->Write(Buf, r);
				}
				else break;
			}
			Status = i == File->Size;
		}
	}

	return Status;
}
