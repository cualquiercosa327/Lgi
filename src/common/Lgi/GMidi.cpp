#include "Lgi.h"
#include "GMidi.h"
#if defined(WIN32)
#include "mmsystem.h"
#endif
#if defined(MAC)
#include <MIDIServices.h>
#endif

#define MIDI_MIRROR_IN_TO_OUT		0

struct GMidiPriv
{
	#if defined(MAC)
	MIDIClientRef Client;
	MIDIPortRef InPort, OutPort;
	GArray<MIDIDeviceRef> Devs;
	GArray<MIDIEndpointRef> Srcs, Dsts;
	MIDIEndpointRef Dst;
	#elif defined(WIN32)
	HMIDIIN hIn;
	HMIDIOUT hOut;
	MIDIHDR InHdr;
	char InBuf[2 << 10];
	#endif
};

#if defined(MAC)
GAutoString PrintMIDIObjectInfo(MIDIObjectRef o)
{
	CFStringRef pName, pManuf, pModel;
	SInt32 Id;
	MIDIObjectGetStringProperty(o, kMIDIPropertyName, &pName);
	MIDIObjectGetStringProperty(o, kMIDIPropertyManufacturer, &pManuf);
	MIDIObjectGetStringProperty(o, kMIDIPropertyModel, &pModel);
	MIDIObjectGetIntegerProperty(o, kMIDIPropertyUniqueID, &Id);
	GAutoString Name(CFStringToUtf8(pName));
	GAutoString Man(CFStringToUtf8(pManuf));
	GAutoString Mod(CFStringToUtf8(pModel));

	char s[256];
	sprintf(s, "%s, %s, %s, %x", Name.Get(), Man.Get(), Mod.Get(), (uint32)Id);
	printf("%s\n", s);

	CFRelease(pName);
	CFRelease(pManuf);
	CFRelease(pModel);
	
	return GAutoString(NewStr(s));
}
#endif

#ifdef MAC
void MidiNotify(const MIDINotification *message, void *refCon)
{
	App *a = (App*)refCon;
	a->OnMidiNotify(message);
}

void MidiRead(const MIDIPacketList *pktlist, void *readProcRefCon, void *srcConnRefCon)
{
	App *a = (App*)readProcRefCon;
	if (a)
	{
		MIDIPacket *packet = (MIDIPacket*)pktlist->packet;
		for (unsigned int j = 0; j < pktlist->numPackets; j++)
		{
			a->OnMidiIn(packet->data, packet->length);
			packet = MIDIPacketNext(packet);
		}
	}
}

void App::OnMidiNotify(const MIDINotification *message)
{
	printf("MIDI notify: %li\n", message->messageID);
}

#elif defined(WIN32)

void CALLBACK MidiOutProc(HMIDIOUT hmo, UINT wMsg, MIDI_TYPE dwInstance, MIDI_TYPE wParam, MIDI_TYPE lParam)
{
	GMidi *a = (GMidi*)dwInstance;
	switch (wMsg)
	{
		case MM_MOM_OPEN:
			// ya ya... we're open for business.
			break;
		case MM_MOM_DONE:
		{
			MIDIHDR *Hdr = (MIDIHDR*)wParam;
			if (!Hdr->dwUser)
				a->OnMidiOut((uchar*)Hdr->lpData, Hdr->dwBufferLength);
			// a->GetLog()->Print("MidiOutProc MM_MOM_DONE\n");
			break;
		}
		default:
		{
		    if (a->GetLog())
			    a->GetLog()->Print("MidiOutProc 0x%x\n", wMsg);
			break;
		}
	}
}

void CALLBACK MidiInProc(HMIDIIN hmi, UINT wMsg, MIDI_TYPE dwInstance, MIDI_TYPE dwParam1, MIDI_TYPE dwParam2)
{
	GMidi *a = (GMidi*)dwInstance;
	uint8 *b = 0;
	int len = 0;
	uint8 buf[3];

	// LgiTrace("MidiInProc wMsg=%i\n", wMsg);
	switch (wMsg)
	{
		case MM_MIM_OPEN:
			// ya ya... we're open for business.
			break;
		case MM_MIM_CLOSE:
			break;
		case MM_MIM_DATA:
		{
			buf[0] = (uint8) (dwParam2 & 0xff);
			buf[1] = (uint8) ((dwParam2 >> 8) & 0xff);
			buf[2] = (uint8) ((dwParam2 >> 16) & 0xff);
			b = buf;
			
			switch (b[0] >> 4)
			{
				case 0x8:
				case 0x9:
				case 0xa:
				case 0xb:
				case 0xe:
					len = 3;
					break;
				case 0xc:
				case 0xd:
					len = 2;
					break;
				default:
					b = NULL;
					break;
			}
			break;
		}
		case MM_MIM_LONGDATA:
		{
			MIDIHDR *Hdr = (MIDIHDR*)dwParam1;
			b = (uint8*) Hdr->lpData;
			len = Hdr->dwBytesRecorded;
			
			if (len)
				midiInAddBuffer(hmi, Hdr, sizeof(*Hdr));
			break;
		}
		default:
		{
			if (a->GetLog())
				a->GetLog()->Print("MidiInProc 0x%x\n", wMsg);
			break;
		}
	}

	if (b && len > 0)
		a->StoreMidi(b, len);
}

#endif

GMidi::GMidi() : GMutex("GMidi")
{
	#if defined(MAC)
	Client = 0;
	Dst = 0;

	int Devs = MIDIGetNumberOfDevices();
	for (int i = 0; i < Devs; i++)
	{
		MIDIDeviceRef dev = MIDIGetDevice(i);
		Devs[i] = dev;
		GAutoString a = PrintMIDIObjectInfo(dev);
		In->Insert(a);
		Out->Insert(a);

		int nEnt = MIDIDeviceGetNumberOfEntities(dev);
		// printf("Device %d has %d entities:\n", i, nEnt);
		for (int n = 0; n < nEnt; ++n)
		{
			MIDIEntityRef ent = MIDIDeviceGetEntity(dev, n);
			// printf("  ");
			// PrintMIDIObjectInfo(ent);
			int nSources = MIDIEntityGetNumberOfSources(ent);
			// printf("  Entity %d has %d source endpoints:\n", n, nSources);
			for (int k = 0; k < nSources; ++k)
			{
				MIDIEndpointRef sep = MIDIEntityGetSource(ent, k);
				Srcs[i] = sep;
				// printf("    ");
				// PrintMIDIObjectInfo(sep);
			}

			int nDestinations = MIDIEntityGetNumberOfDestinations(ent);
			// printf("  Entity %d has %d destination endpoints:\n", i, nDestinations);
			for (int l = 0; l < nDestinations; ++l)
			{
				MIDIEndpointRef dep = MIDIEntityGetDestination(ent, l);
				Dsts[i] = dep;
				// printf("    ");
				// PrintMIDIObjectInfo(dep);
			}
		}
	}
	
	if (In && Out)
	{
		In->Value(5);
		Out->Value(5);
	}

	#elif defined(WIN32)

	d = new GMidiPriv;
	d->hIn = 0;
	d->hOut = 0;

	UINT InDevs = midiInGetNumDevs();
	UINT OutDevs = midiOutGetNumDevs();
	UINT n;

	MIDIINCAPS InCaps;
	for (n=0; n<InDevs; n++)
	{
		MMRESULT r = midiInGetDevCaps(n, &InCaps, sizeof(InCaps));
		if (r == MMSYSERR_NOERROR)
			In.New().Reset(LgiNewUtf16To8(InCaps.szPname));
	}

	MIDIOUTCAPS OutCaps;
	for (n=0; n<OutDevs; n++)
	{
		MMRESULT r = midiOutGetDevCaps(n, &OutCaps, sizeof(OutCaps));
		if (r == MMSYSERR_NOERROR)
			Out.New().Reset(LgiNewUtf16To8(OutCaps.szPname));
	}
	
	#endif
}

GMidi::~GMidi()
{
	CloseMidi();
	delete d;
}

bool GMidi::IsMidiOpen()
{
	#if defined(MAC)
	return Client != 0;
	#elif defined(WIN32)
	return d->hIn != 0;
	#endif
}

void GMidi::StoreMidi(uint8 *ptr, int len)
{
	if (Lock(_FL))
	{
		MidiIn.Add(ptr, len);
		Unlock();
	}
}

bool GMidi::Connect(int InIdx, int OutIdx, GAutoString *ErrorMsg)
{
	if (IsMidiOpen())
	{
		if (ErrorMsg)
			ErrorMsg->Reset(NewStr("Already connected."));
		return false;
	}

	bool Status = false;
	
	#ifdef MAC
	
	CFStringRef Name = CFSTR("AxeFoot");
	OSStatus e = MIDIClientCreate(Name, MidiNotify, this, &Client);
	if (e)
	{
	    if (GetLog())
		    GetLog()->Print("%s:%i - MIDIClientCreate failed with %i\n", _FL, e);
	}
	else
	{
		e = MIDIInputPortCreate(Client, CFSTR("AxeFootInput"), MidiRead, this, &InPort);
		if (e)
		{
		    if (GetLog())
			    GetLog()->Print("%s:%i - MIDIInputPortCreate failed with %i\n", _FL, e);
		}
		else
		{
			e = MIDIOutputPortCreate(Client, CFSTR("AxeFootOutput"), &OutPort);
			if (e)
			{
			    if (GetLog())
				    GetLog()->Print("%s:%i - MIDIOutputPortCreate failed with %i\n", _FL, e);
			}
			else
			{
				int InIdx = In->Value();
				int OutIdx = Out->Value();

				MIDIEndpointRef Src = Srcs[InIdx];
				Dst = Dsts[OutIdx];

				e = MIDIPortConnectSource(InPort, Src, this);
				if (e)
				{
				    if (GetLog())
					    GetLog()->Print("%s:%i - MIDIPortConnectSource failed with %i\n", _FL, e);
				}
				else
				{
				    if (GetLog())
					    GetLog()->Print("Created midi client ok.\n");
				}
			}
		}
	}

	#elif defined(WIN32)

	MMRESULT r = midiOutOpen(&d->hOut, OutIdx, (MIDI_TYPE)MidiOutProc, (MIDI_TYPE)this, CALLBACK_FUNCTION);
	if (r != MMSYSERR_NOERROR)
	{
		OnError("midiOutOpen", ErrorMsg, r, _FL);
	}
	else
	{
		r = midiInOpen(&d->hIn, InIdx, (MIDI_TYPE)MidiInProc, (MIDI_TYPE)this, CALLBACK_FUNCTION);
		if (r != MMSYSERR_NOERROR)
		{
			OnError("midiInOpen", ErrorMsg, r, _FL);
		}
		else
		{
			ZeroObj(d->InHdr);
			d->InHdr.lpData = d->InBuf;
			d->InHdr.dwBufferLength = sizeof(d->InBuf);

			r = midiInPrepareHeader(d->hIn, &d->InHdr, sizeof(d->InHdr));
			if (r != MMSYSERR_NOERROR)
			{
				OnError("midiInPrepareHeader", ErrorMsg, r, _FL);
			}
			else
			{
				r = midiInAddBuffer(d->hIn, &d->InHdr, sizeof(d->InHdr));
				if (r != MMSYSERR_NOERROR && GetLog())
					GetLog()->Print("%s:%i - midiInAddBuffer failed with %i\n", _FL, r);
			}

			r = midiInStart(d->hIn);
			if (r != MMSYSERR_NOERROR)
			{
				OnError("midiInStart", ErrorMsg, r, _FL);
			}
			else
			{
				Status = true;
				if (GetLog())
					GetLog()->Print("Created midi connections ok.\n");
			}
		}
	}

	#endif
	
	return Status;
}

void GMidi::CloseMidi()
{
	#if defined(MAC)
	if (Client)
	{
		MIDIClientDispose(Client);
		Client = 0;
	}
	#elif defined(WIN32)
	if (d->hIn)
	{
		midiInReset(d->hIn);
		midiInUnprepareHeader(d->hIn, &d->InHdr, sizeof(d->InHdr));
		midiInClose(d->hIn);
		d->hIn = 0;
	}
	if (d->hOut)
	{
		midiOutReset(d->hOut);
		midiOutClose(d->hOut);
		d->hOut = 0;
	}
	#endif
}

void GMidi::OnMidiIn(uint8 *midi, int midi_len)
{
	#if MIDI_MIRROR_IN_TO_OUT
	SendMidi(p, len);
	#endif
}

void GMidi::SendMidi(uint8 *ptr, int len, bool quiet)
{
	LgiAssert(ptr != NULL);
	if (!ptr)
		return;
	#if defined(WIN32)
	if (d->hOut)
	{
		if (len <= 3)
		{
			DWORD dwMsg = 0;
			uint8 *b = (uint8*) &dwMsg;
			memcpy(b, ptr, len);
			MMRESULT r = midiOutShortMsg(d->hOut, dwMsg);
			if (r != 0)
			{
			    if (GetLog())
				    GetLog()->Print("midiOutShortMsg failed with %i\n", r);
			}

			if (!quiet)
				OnMidiOut(ptr, len);
		}
		else
		{
			MIDIHDR Hdr;
			ZeroObj(Hdr);
			Hdr.dwUser = quiet;
			Hdr.lpData = (LPSTR) ptr;
			Hdr.dwBufferLength = len;
			midiOutPrepareHeader(d->hOut, &Hdr, sizeof(Hdr));

			MMRESULT r = midiOutLongMsg(d->hOut, &Hdr, sizeof(Hdr));
			if (r != MMSYSERR_NOERROR)
			{
			    if (GetLog())
				    GetLog()->Print("midiOutLongMsg failed with %i\n", r);
			}

			midiOutUnprepareHeader(d->hOut, &Hdr, sizeof(Hdr));
		}
	}
	#else
	if (OutPort)
	{
		struct MIDIPacketList p;
		p.numPackets = 1;
		p.packet[0].timeStamp = 0;
		p.packet[0].length = len;
		memcpy(p.packet[0].data, ptr, len);
		OSStatus e = MIDISend(OutPort, Dst, &p);
		if (e)
		{
		    if (GetLog())
			    GetLog()->Print("%s:%i - MIDISend failed %e\n", _FL, e);
		}
		else if (!quiet)
		{
		    if (GetLog())
			    OnMidiOut(ptr, len);
		}
	}
	#endif
}

void GMidi::OnMidiOut(uint8 *p, int len)
{
    if (GetLog())
    {
	    GetLog()->Print("Out: ");
	    for (int i=0; i<len; i++)
		    GetLog()->Print("%02.2x ", p[i]);
	    GetLog()->Print("\n");
	}

	#if MIDI_MIRROR_IN_TO_OUT
	SendMidi(p, len);
	#endif
}

void GMidi::OnError(char *Func, GAutoString *ErrorMsg, uint32 Code, char *File, int Line)
{
	char m[256];
	sprintf_s(m, sizeof(m), "%s failed with %i", Func, Code);
	if (ErrorMsg)
		ErrorMsg->Reset(NewStr(m));
    if (GetLog())
	    GetLog()->Print("%s:%i - %s\n", File, Line, m);
}
