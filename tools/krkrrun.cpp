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

#include <cstdio>
#include <string>
#include <vector>
#include <unistd.h>

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

int main(int argc, char** argv)
{
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

	ConsoleOutput console;
	tTJS* tjs = new tTJS();
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
