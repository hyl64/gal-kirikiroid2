// krkrrun - run a TJS2 script with the engine's classes registered
//
// This is the closest thing to the engine's own start-up a tool can do: a TJS2
// instance with the native classes the engine registers (Window, Layer, Bitmap,
// System, Storages, Scripts and the rest), so a script can create a window, put
// layers in it and draw.  What it is for is checking that path without a game:
// everything the script touches is the engine's own object.
//
// Usage: krkrrun <script.tjs>

#include "tjsCommHead.h"
#include "tjs.h"
#include "tjsString.h"
#include "tjsError.h"
#include "tjsNative.h"
#include "tjsVariant.h"

#include "WindowIntf.h"
#include "LayerIntf.h"
#include "BitmapIntf.h"
#include "LayerBitmapIntf.h"
#include "SystemIntf.h"
#include "StorageIntf.h"
#include "ScriptMgnIntf.h"
#include "RectItf.h"
#include "FontImpl.h"
#include "DebugIntf.h"
#include "PluginImpl.h"
#include "tvpgl.h"
#include "StorageIntf.h"
#include "DrawDevice.h"
#include "LayerIntf.h"
#include "LayerBitmapIntf.h"

#include <SDL.h>

#include <cstdio>
#include <cstring>
#include <string>
#include <vector>
#include <unistd.h>
#include <execinfo.h>
#include <cstdlib>

// who called exit(): the engine's terminate path leads somewhere we cannot see
// otherwise, and the answer is in this stack
static void OnExitTrace()
{
	if(getenv("KRKR_TRACE_EXIT") == nullptr)
		return;
	void* frames[32];
	const int n = backtrace(frames, 32);
	fprintf(stderr, "[exit trace] %d frame(s)\n", n);
	backtrace_symbols_fd(frames, n, 2);
}

// the console the script prints through
class ConsoleOutput : public iTJSConsoleOutput
{
public:
	void ExceptionPrint(const tjs_char* msg) override
	{
		fprintf(stderr, "%ls\n", (const wchar_t*)msg);
	}
	void Print(const tjs_char* msg) override
	{
		fprintf(stdout, "%ls", (const wchar_t*)msg);
		fflush(stdout);
	}
};

static bool ReadScript(const char* path, std::vector<tjs_char>& out)
{
	FILE* f = fopen(path, "rb");
	if(f == nullptr)
		return false;
	fseek(f, 0, SEEK_END);
	long size = ftell(f);
	fseek(f, 0, SEEK_SET);
	std::string bytes((size_t)(size > 0 ? size : 0), '\0');
	if(size > 0)
		bytes.resize((size_t)fread(&bytes[0], 1, (size_t)size, f));
	fclose(f);
	out.assign(bytes.size() + 1, 0);
	TJS_mbstowcs(&out[0], bytes.c_str(), out.size());
	return true;
}

static void Register(iTJSDispatch2* global, const tjs_char* name, iTJSDispatch2* object)
{
	if(object == nullptr)
	{
		fprintf(stderr, "krkrrun: %ls is not available on this back end\n", (const wchar_t*)name);
		return;
	}
	tTJSVariant val(object, NULL);
	object->Release();
	global->PropSet(TJS_MEMBERENSURE | TJS_IGNOREPROP, name, NULL, &val, global);
}

// what the window composited, written out so the frame can be looked at
static bool SaveComposed(const char* path)
{
	// what the device presented is the frame; the primary layer is only a
	// fallback for a run that never reached Show()
	extern const uint32_t* TVPDrawDeviceDarwinGetFrame(int* width, int* height);
	extern iTVPDrawDevice* TVPGetLastDrawDeviceDarwin();
	int fw = 0, fh = 0;
	if(const uint32_t* frame = TVPDrawDeviceDarwinGetFrame(&fw, &fh))
	{
		if(fw > 0 && fh > 0)
		{
			SDL_Surface* s2 = SDL_CreateRGBSurfaceWithFormatFrom((void*)frame, fw, fh, 32, fw * 4,
				SDL_PIXELFORMAT_ARGB8888);
			const bool ok2 = s2 != nullptr && SDL_SaveBMP(s2, path) == 0;
			if(s2 != nullptr)
				SDL_FreeSurface(s2);
			if(ok2)
			{
				printf("wrote %s (%dx%d, presented frame)\n", path, fw, fh);
				return true;
			}
		}
	}
	iTVPDrawDevice* device = TVPGetLastDrawDeviceDarwin();
	if(device == nullptr)
	{
		fprintf(stderr, "krkrrun: no draw device was constructed\n");
		return false;
	}
	tTJSNI_BaseLayer* primary = device->GetPrimaryLayer();
	if(primary == nullptr)
	{
		fprintf(stderr, "krkrrun: the device has no primary layer\n");
		return false;
	}
	tTVPBaseTexture* screen = primary->GetMainImage();
	const int w = (int)screen->GetWidth();
	const int h = (int)screen->GetHeight();
	if(w <= 0 || h <= 0)
	{
		fprintf(stderr, "krkrrun: the primary layer is %dx%d\n", w, h);
		return false;
	}
	SDL_Surface* surf = SDL_CreateRGBSurfaceWithFormatFrom((void*)screen->GetScanLine(0),
		w, h, 32, screen->GetPitchBytes(), SDL_PIXELFORMAT_ABGR8888);
	const bool ok = surf != nullptr && SDL_SaveBMP(surf, path) == 0;
	if(surf != nullptr)
		SDL_FreeSurface(surf);
	printf(ok ? "wrote %s (%dx%d)\n" : "cannot write %s\n", path, w, h);
	return ok;
}

int main(int argc, char** argv)
{
	atexit(OnExitTrace);
	if(argc < 2)
	{
		fprintf(stderr, "usage: %s <script.tjs>\n", argv[0]);
		return 2;
	}
	std::vector<tjs_char> script;
	if(!ReadScript(argv[1], script))
	{
		fprintf(stderr, "krkrrun: cannot read %s\n", argv[1]);
		return 1;
	}

	// the pixel pipeline's function table: without it the first blitter call
	// jumps through a null pointer, which is how the layer's first fill died
	TVPInitTVPGL();

	// The engine's own start-up makes the game's folder searchable, so a script's
	// relative storage name ("system/Initialize.tjs") resolves.  KAG depends on it.
	{
		std::string root = "file://";
		char cwd[1024];
		if(getcwd(cwd, sizeof cwd) != nullptr)
			root += cwd;
		root += "/";
		// KAG's own scripts live in system/ and name each other without the
		// prefix, so that folder goes in first (the first match wins)
		TVPAddAutoPath(ttstr((root + "system/").c_str()));
		TVPAddAutoPath(ttstr(root.c_str()));

		TVPSetCurrentDirectory(ttstr(root.c_str()));
	}

	ConsoleOutput console;
	// Scripts.execStorage and friends reach the engine through this global; the
	// application's start-up assigns it, and a tool creates the engine itself
	extern tTJS* TVPScriptEngine;
	tTJS* tjs = new tTJS();
	TVPScriptEngine = tjs;
	// the engine's own TJS-level definitions (gcsAuto and the rest); KAG needs them
	extern void TVPRunEngineInitScript(tTJS*);
	TVPRunEngineInitScript(tjs);
	tjs->SetConsoleOutput(&console);
	iTJSDispatch2* global = tjs->GetGlobal();

	// the classes the engine registers at start-up, as far as this back end has
	// them; each factory answers nothing where the platform half is missing
	Register(global, TJS_W("System"), TVPCreateNativeClass_System());
	Register(global, TJS_W("Window"), TVPCreateNativeClass_Window());
	Register(global, TJS_W("Layer"), TVPCreateNativeClass_Layer());
	Register(global, TJS_W("Bitmap"), TVPCreateNativeClass_Bitmap());
	Register(global, TJS_W("Storages"), TVPCreateNativeClass_Storages());
	Register(global, TJS_W("Scripts"), TVPCreateNativeClass_Scripts());
	Register(global, TJS_W("Rect"), TVPCreateNativeClass_Rect());
	Register(global, TJS_W("Font"), TVPCreateNativeClass_Font());
	Register(global, TJS_W("Debug"), TVPCreateNativeClass_Debug());
	Register(global, TJS_W("Plugins"), TVPCreateNativeClass_Plugins());

	int status = 0;
	try
	{
		ttstr name = ttstr(argv[1]);
		tjs->ExecScript(&script[0], nullptr, nullptr, name.c_str());
		// an optional frame, taken after the script has run
		if(argc >= 4 && strcmp(argv[2], "--screenshot") == 0)
		{
			fflush(stdout);
			SaveComposed(argv[3]);
		}
	}
	catch(const eTJS& e)
	{
		fprintf(stderr, "krkrrun: %ls\n", (const wchar_t*)e.GetMessage().c_str());
		status = 1;
	}
	catch(...)
	{
		fprintf(stderr, "krkrrun: unhandled exception\n");
		status = 1;
	}
	fflush(stdout);
	fflush(stderr);
	_exit(status);
}
