/*
 * This file is part of the macOS port of Kirikiroid2 (zeas2/Kirikiroid2).
 *
 * DrawDevice_Darwin.cpp - the desktop back end's screen
 *
 * tTVPDrawDevice (visual/win32/DrawDevice.cpp, which carries no Windows call at
 * all) is the engine's compositor: layers combine into one screen texture, and
 * the platform's job is the last four steps it leaves open - Show(), which puts
 * that screen on the display, and the three bitmap-completion hooks, which exist
 * so a GPU back end can hand a copy back asynchronously.
 *
 * Here Show() uploads the composited screen to an SDL texture and presents it;
 * the completion hooks run their work inline, which is what a CPU compositor
 * does anyway.
 */

#include "tjsCommHead.h"
#include "DrawDevice.h"
#include "RenderManager.h"

#include <SDL.h>

#include <cstdio>

// declared by the tools (tools/krkrdevice.cpp) rather than in a header, since
// this back end's factory is the only caller
class tTVPDrawDevice_Darwin : public tTVPDrawDevice
{
public:
	SDL_Window* Window;
	SDL_Renderer* Renderer;
	SDL_Texture* Screen;
	int ScreenW;
	int ScreenH;
	bool TriedInit;

	void EnsureWindow(int w, int h);

public:
	tTVPDrawDevice_Darwin();
	~tTVPDrawDevice_Darwin() override;

	void TJS_INTF_METHOD Show() override;
	void TJS_INTF_METHOD StartBitmapCompletion(iTVPLayerManager* manager) override;
	void TJS_INTF_METHOD NotifyBitmapCompleted(iTVPLayerManager* manager, tjs_int x, tjs_int y,
		tTVPBaseTexture* bmp, const tTVPRect& cliprect, tTVPLayerType type, tjs_int opacity) override;
	void TJS_INTF_METHOD EndBitmapCompletion(iTVPLayerManager* manager) override;

	// candidate for a screenshot: the pixels the last Show() put on the screen
	bool SaveScreen(const char* path);
};

tTVPDrawDevice_Darwin::tTVPDrawDevice_Darwin()
	: Window(nullptr), Renderer(nullptr), Screen(nullptr), ScreenW(0), ScreenH(0), TriedInit(false)
{
}

tTVPDrawDevice_Darwin::~tTVPDrawDevice_Darwin()
{
	if(Screen != nullptr)
		SDL_DestroyTexture(Screen);
	if(Renderer != nullptr)
		SDL_DestroyRenderer(Renderer);
	if(Window != nullptr)
		SDL_DestroyWindow(Window);
}

void tTVPDrawDevice_Darwin::EnsureWindow(int w, int h)
{
	if(Window != nullptr && w == ScreenW && h == ScreenH)
		return;
	if(Screen != nullptr)
	{
		SDL_DestroyTexture(Screen);
		Screen = nullptr;
	}
	if(Renderer != nullptr)
	{
		SDL_DestroyRenderer(Renderer);
		Renderer = nullptr;
	}
	if(Window != nullptr)
	{
		SDL_DestroyWindow(Window);
		Window = nullptr;
	}
	if(!TriedInit)
	{
		TriedInit = true;
		if(SDL_Init(SDL_INIT_VIDEO) != 0)
			fprintf(stderr, "tTVPDrawDevice_Darwin: SDL_Init: %s\n", SDL_GetError());
	}
	Window = SDL_CreateWindow("Kirikiroid2", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
		w, h, SDL_WINDOW_SHOWN);
	if(Window == nullptr)
		return;
	Renderer = SDL_CreateRenderer(Window, -1, SDL_RENDERER_ACCELERATED);
	if(Renderer == nullptr)
		Renderer = SDL_CreateRenderer(Window, -1, 0);
	Screen = SDL_CreateTexture(Renderer, SDL_PIXELFORMAT_ABGR8888,
		SDL_TEXTUREACCESS_STREAMING, w, h);
	ScreenW = w;
	ScreenH = h;
}

// the compositor's output, onto the display
void TJS_INTF_METHOD tTVPDrawDevice_Darwin::Show()
{
	tTJSNI_BaseLayer* primary = GetPrimaryLayer();
	if(primary == nullptr)
		return;
	tTVPBaseTexture* screen = primary->GetMainImage();
	if(screen == nullptr)
		return;
	int w = (int)screen->GetWidth();
	int h = (int)screen->GetHeight();
	if(w <= 0 || h <= 0)
		return;
	EnsureWindow(w, h);
	if(Screen == nullptr)
		return;
	SDL_UpdateTexture(Screen, nullptr, screen->GetScanLine(0), screen->GetPitchBytes());
	SDL_RenderClear(Renderer);
	SDL_RenderCopy(Renderer, Screen, nullptr, nullptr);
	SDL_RenderPresent(Renderer);

	// let the window's events be pumped: closing it ends the process, as the
	// original's close box does once the engine stops asking for frames
	SDL_Event e;
	while(SDL_PollEvent(&e))
		if(e.type == SDL_QUIT)
			SDL_Quit();
}

// a CPU compositor finishes its copies before it returns, so these hooks have
// nothing to wait for
void TJS_INTF_METHOD tTVPDrawDevice_Darwin::StartBitmapCompletion(iTVPLayerManager* manager)
{
	(void)manager;
}

// The compositor already drew the layer into the screen; a GPU back end would
// copy the bitmap here asynchronously, and a CPU one has nothing to add.
void TJS_INTF_METHOD tTVPDrawDevice_Darwin::NotifyBitmapCompleted(iTVPLayerManager* manager,
	tjs_int x, tjs_int y, tTVPBaseTexture* bmp, const tTVPRect& cliprect, tTVPLayerType type,
	tjs_int opacity)
{
	(void)manager;
	(void)x;
	(void)y;
	(void)bmp;
	(void)cliprect;
	(void)type;
	(void)opacity;
}

void TJS_INTF_METHOD tTVPDrawDevice_Darwin::EndBitmapCompletion(iTVPLayerManager* manager)
{
	(void)manager;
}

bool tTVPDrawDevice_Darwin::SaveScreen(const char* path)
{
	if(Screen == nullptr || Renderer == nullptr)
		return false;
	SDL_Surface* shot = SDL_CreateRGBSurfaceWithFormat(0, ScreenW, ScreenH, 32,
		SDL_PIXELFORMAT_ABGR8888);
	if(shot == nullptr)
		return false;
	bool ok = SDL_RenderReadPixels(Renderer, nullptr, SDL_PIXELFORMAT_ABGR8888,
		shot->pixels, shot->pitch) == 0;
	if(ok)
		ok = SDL_SaveBMP(shot, path) == 0;
	SDL_FreeSurface(shot);
	return ok;
}

// the factory the engine's application layer would call
tTVPDrawDevice* TVPCreateDrawDeviceDarwin()
{
	return new tTVPDrawDevice_Darwin();
}

// what the screen holds, for the tools that check a frame
bool TVPDrawDeviceDarwinSaveScreen(tTVPDrawDevice* device, const char* path)
{
	tTVPDrawDevice_Darwin* darwin = dynamic_cast<tTVPDrawDevice_Darwin*>(device);
	return darwin != nullptr ? darwin->SaveScreen(path) : false;
}
