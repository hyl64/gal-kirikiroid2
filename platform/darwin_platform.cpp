// darwin_platform.cpp - the handful of hooks the TJS2 engine expects from its
// host on macOS
//
// Upstream these live in the application layer: environ/cocos2d/MainScene.cpp
// for the Android build, environ/Application.cpp for the Windows one.  The
// desktop port has its own application layer (this file for now, the SDL
// window later), so the three symbols TJS2 links against are provided here:
//
//   TVPGetRoughTickCount32  - the millisecond counter used for seeding and timing
//   TVPConsoleLog           - the engine's log line
//   TVPGetMessageByLocale   - a message by key, from the engine's own catalogue

#include "darwin_platform.h"

#include "tjsMessage.h"

#include <time.h>
#include <stdio.h>

// ---------------------------------------------------------------------------
// timing
// ---------------------------------------------------------------------------

// milliseconds since an unspecified point, monotonic; the engine only ever
// compares two readings
tjs_uint32 TVPGetRoughTickCount32()
{
	struct timespec ts;
	clock_gettime(CLOCK_MONOTONIC, &ts);
	return (tjs_uint32)((tjs_uint64)ts.tv_sec * 1000u + (tjs_uint64)(ts.tv_nsec / 1000000));
}

// ---------------------------------------------------------------------------
// logging
// ---------------------------------------------------------------------------

// Write UTF-16 as UTF-8; the engine speaks UTF-16 everywhere and a terminal
// wants bytes.  Three-byte sequences cover the BMP, which is all TJS2 text is.
static void WriteUtf8(const tjs_char* s, FILE* f)
{
	if(s == nullptr)
		return;
	for(; *s; s++)
	{
		tjs_uint32 c = (tjs_uint32)*s;
		if(c < 0x80)
			fputc((int)c, f);
		else if(c < 0x800)
		{
			fputc((int)(0xC0 | (c >> 6)), f);
			fputc((int)(0x80 | (c & 0x3F)), f);
		}
		else
		{
			fputc((int)(0xE0 | (c >> 12)), f);
			fputc((int)(0x80 | ((c >> 6) & 0x3F)), f);
			fputc((int)(0x80 | (c & 0x3F)), f);
		}
	}
}

// the script-facing console (darwin_system.cpp) and the engine's log
void KrkrOut(const tjs_char* s)
{
	WriteUtf8(s, stdout);
}

void KrkrErr(const tjs_char* s)
{
	WriteUtf8(s, stderr);
}

namespace TJS
{
	void TVPConsoleLog(const tjs_char* l)
	{
		KrkrErr(l);
		fputc('\n', stderr);
	}
}

// ---------------------------------------------------------------------------
// messages
// ---------------------------------------------------------------------------

// The locale XML of the shipped games is loaded by the cocos2d application
// layer; without it the engine's compiled-in catalogue answers, and a key
// with no entry reads as itself (which is what a missing translation means).
ttstr TVPGetMessageByLocale(const std::string& key)
{
	tjs_char wkey[256];
	size_t n = key.size() < 255 ? key.size() : 255;
	for(size_t i = 0; i < n; i++)
		wkey[i] = (tjs_char)(unsigned char)key[i];
	wkey[n] = 0;
	ttstr msg = TJSGetMessageMapMessage(wkey);
	if(msg.IsEmpty())
		return ttstr(key.c_str());
	return msg;
}
