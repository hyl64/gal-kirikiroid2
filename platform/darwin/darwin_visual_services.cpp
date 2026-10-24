/*
 * This file is part of the macOS port of Kirikiroid2 (zeas2/Kirikiroid2).
 *
 * darwin_visual_services.cpp - the visual services this back end does not have
 *
 * The engine registers an image decoder per format and asks the platform for a
 * cursor; the graphics loader (visual/GraphicsLoaderIntf.cpp) names all of them
 * at link time.  The desktop back end decodes what KiriKiri games actually
 * carry - PNG and TLG, in visual/LoadPNG.cpp and visual/LoadTLG.cpp - so the
 * other formats answer as unsupported instead of pretending: a game that needs
 * JPEG, JXR, WEBP, BPG or PVR loses those pictures and says so, and nothing
 * else changes.
 *
 * The cursor is the same story: games set a cursor shape by name, and a back
 * end with no cursor theme leaves the system arrow alone.
 */

#include "tjsCommHead.h"
#include "tjsString.h"
#include "tjsError.h"

#include <functional>
#include <unistd.h>
#include <dirent.h>
#include <sys/stat.h>
#include <cstring>
#include <string>

#include <SDL.h>
#include "GraphicsLoaderIntf.h"
#include "MsgIntf.h"
#include "ClipboardIntf.h"
#include "MenuItemIntf.h"
#include "WindowIntf.h"

// ---------------------------------------------------------------------------
// the formats this back end does not decode
// ---------------------------------------------------------------------------

static void UnsupportedFormat(const char* name)
{
	TVPThrowExceptionMessage(TVPCannotOpenStorage,
		ttstr(TJS_W("the macOS back end has no decoder for ")) + ttstr(name));
}

void TVPLoadJPEG(void*, void*, tTVPGraphicSizeCallback, tTVPGraphicScanLineCallback,
	tTVPMetaInfoPushCallback, tTJSBinaryStream*, tjs_int, tTVPGraphicLoadMode)
{
	UnsupportedFormat("JPEG");
}

void TVPLoadJXR(void*, void*, tTVPGraphicSizeCallback, tTVPGraphicScanLineCallback,
	tTVPMetaInfoPushCallback, tTJSBinaryStream*, tjs_int, tTVPGraphicLoadMode)
{
	UnsupportedFormat("JPEG XR");
}

void TVPLoadWEBP(void*, void*, tTVPGraphicSizeCallback, tTVPGraphicScanLineCallback,
	tTVPMetaInfoPushCallback, tTJSBinaryStream*, tjs_int, tTVPGraphicLoadMode)
{
	UnsupportedFormat("WebP");
}

void TVPLoadBPG(void*, void*, tTVPGraphicSizeCallback, tTVPGraphicScanLineCallback,
	tTVPMetaInfoPushCallback, tTJSBinaryStream*, tjs_int, tTVPGraphicLoadMode)
{
	UnsupportedFormat("BPG");
}

void TVPLoadPVRv3(tTJSBinaryStream*, const std::function<void(const ttstr&, const tTJSVariant&)>&)
{
	UnsupportedFormat("PVR");
}

void TVPLoadPVRv3(void*, void*, tTVPGraphicSizeCallback, tTVPGraphicScanLineCallback,
	tTVPMetaInfoPushCallback, tTJSBinaryStream*, tjs_int, tTVPGraphicLoadMode)
{
	UnsupportedFormat("PVR");
}

void TVPLoadHeaderBPG(void*, tTJSBinaryStream*, iTJSDispatch2**)
{
	UnsupportedFormat("BPG");
}

void TVPSaveAsJPG(void*, tTJSBinaryStream*, const iTVPBaseBitmap*, const ttstr&, iTJSDispatch2*)
{
	UnsupportedFormat("JPEG");
}

void TVPSaveAsJXR(void*, tTJSBinaryStream*, const iTVPBaseBitmap*, const ttstr&, iTJSDispatch2*)
{
	UnsupportedFormat("JPEG XR");
}

void TVPSaveAsTLG(void*, tTJSBinaryStream*, const iTVPBaseBitmap*, const ttstr&, iTJSDispatch2*)
{
	UnsupportedFormat("TLG");
}

void TVPLoadHeaderJPG(void*, tTJSBinaryStream*, iTJSDispatch2**)
{
	UnsupportedFormat("JPEG");
}

void TVPLoadHeaderJXR(void*, tTJSBinaryStream*, iTJSDispatch2**)
{
	UnsupportedFormat("JPEG XR");
}

void TVPLoadHeaderWEBP(void*, tTJSBinaryStream*, iTJSDispatch2**)
{
	UnsupportedFormat("WebP");
}

bool TVPAcceptSaveAsJPG(void*, const ttstr&, iTJSDispatch2**)
{
	return false; // no save support: the engine then offers nothing
}

bool TVPAcceptSaveAsJXR(void*, const ttstr&, iTJSDispatch2**)
{
	return false;
}

// ---------------------------------------------------------------------------
// the event loop
// ---------------------------------------------------------------------------

// The engine posts events to scripts and drains them with TVPInvokeEvents; its
// own event thread is the platform's (base/win32/EventImpl.cpp on Windows).  A
// host that renders a frame and leaves has nothing queued, so this runs nothing
// and the queue stays empty - the window layer will drain it once there is a
// message loop.
void TVPInvokeEvents()
{
}

void TVPEventReceived()
{
}

bool TVPAcceptSaveAsTLG(void*, const ttstr&, iTJSDispatch2**)
{
	return false; // no TLG writer in this back end
}

void TVPLoadHeaderPVRv3(void*, tTJSBinaryStream*, iTJSDispatch2**)
{
	UnsupportedFormat("PVR");
}

// ---------------------------------------------------------------------------
// the application's odds and ends
// ---------------------------------------------------------------------------

// the engine asks where it runs from (the original lives in the game folder)
ttstr TVPGetAppPath()
{
	char cwd[1024];
	if(getcwd(cwd, sizeof cwd) == nullptr)
		return ttstr("file://./");
	std::string path("file://");
	path += cwd;
	path += "/";
	return ttstr(path.c_str());
}

// no window is up in a tool, and no asynchronous work is running to stop
void TVPTerminateSync(int code)
{
	(void)code;
}

tjs_int TVPGetWindowCount()
{
	return 0;
}

void TVPDumpHWException()
{
}

void TVPOpenPatchLibUrl()
{
}

// the clipboard is the system's; SDL owns the pasteboard here
bool TVPClipboardGetText(ttstr& text)
{
	char* clip = SDL_GetClipboardText();
	if(clip == nullptr)
		return false;
	text = ttstr(clip);
	SDL_free(clip);
	return true;
}

void TVPClipboardSetText(const ttstr& text)
{
	tTJSNarrowStringHolder narrow(text.c_str());
	SDL_SetClipboardText((const char*)narrow);
}

// ---------------------------------------------------------------------------
// the last odds and ends the runtime asks for
// ---------------------------------------------------------------------------

// the engine's shutdown flag and the "quit when no window opened" switch
bool TVPSystemUninitCalled = false;
bool TVPTerminateOnNoWindowStartup = false;

// continuous events are the engine's idle callbacks; a tool has no idle loop
void TVPBeginContinuousEvent()
{
}

void TVPEndContinuousEvent()
{
}

// the clipboard holds text here, so whatever the engine asks for is there
bool TVPClipboardHasFormat(tTVPClipboardFormat format)
{
	(void)format;
	return true;
}

ttstr TVPGetErrorDialogTitle()
{
	return ttstr("Error");
}

// saving the per-game settings writes a file the desktop host does not keep yet
void TVPDoSaveSystemVariables()
{
}

// ---------------------------------------------------------------------------
// colour and cursor
// ---------------------------------------------------------------------------

// 0xRRGGBB into the 0x00BBGGRR the Windows GDI colour values use
unsigned long ColorToRGB(unsigned int col)
{
	return ((col & 0xff) << 16) | (col & 0xff00) | ((col >> 16) & 0xff);
}

// games name a cursor; a back end without a cursor theme keeps the system one,
// so the name is recorded and no shape is set
static tjs_int gCursorShape = 0;

tjs_int TVPGetCursor(const ttstr& name)
{
	(void)name;
	tjs_int shape = gCursorShape;
	gCursorShape = (gCursorShape + 1) & 0x0f;
	return shape;
}

// ---------------------------------------------------------------------------
// what the Windows system layer would supply
// ---------------------------------------------------------------------------
//
// Some of the engine's files were written against the Windows back end's system
// layer (base/win32/SysInitImpl.cpp, environ/win32/SystemControl.cpp,
// base/win32/StorageImpl.cpp) and name a few globals and helpers from it.  The
// desktop host does not have that layer yet; these are the pieces it names,
// answered the way a host with no plug-ins, no popup menu and no SIMD tables
// would answer them.

ttstr TVPDataPath;          // where the game's data lives; the hosts sets it
ttstr TVPNativeProjectDir;  // the project folder of the original tool

void TVPGL_ASM_Init()
{
	// the SIMD blitters are never initialised here: TVPInitTVPGL points the
	// function pointers at the C implementations
}

tTVPJPEGLoadPrecision TVPJPEGLoadPrecision = jlpMedium;

void TVPDetectCPU()
{
}

void TVPShowPopMenu(tTJSNI_MenuItem* item)
{
	(void)item; // no popup menus without a window to pop them up in
}

tTJSNI_Window* TVPGetWindowListAt(tjs_int idx)
{
	(void)idx;
	return nullptr;
}

std::string ExtractFileDir(const std::string& FileName)
{
	size_t pos = FileName.find_last_of("/\\");
	return pos == std::string::npos ? std::string(".") : FileName.substr(0, pos);
}

// list a folder's entries, the way the engine's own storage layer does: the
// callback gets the name and whether it is a folder
void TVPListDir(const std::string& folder, std::function<void(const std::string&, int)> cb)
{
	DIR* d = opendir(folder.c_str());
	if(d == nullptr)
		return;
	struct dirent* e;
	while((e = readdir(d)) != nullptr)
	{
		if(strcmp(e->d_name, ".") == 0 || strcmp(e->d_name, "..") == 0)
			continue;
		struct stat st;
		std::string path = folder + "/" + e->d_name;
		int isDir = (stat(path.c_str(), &st) == 0 && S_ISDIR(st.st_mode)) ? 1 : 0;
		cb(e->d_name, isDir);
	}
	closedir(d);
}
