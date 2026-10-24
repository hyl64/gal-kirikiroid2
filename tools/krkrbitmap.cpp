// krkrbitmap - a bitmap through the engine's own bitmap API
//
// iTVPBaseBitmap is the engine's picture object: layers draw into it, the
// blitters read it, the image loaders fill it.  Its portable half
// (visual/LayerBitmapIntf.cpp: Fill, FillColor, GetPoint, SetPoint) sits on the
// platform half, which for this back end is a software texture in engine pixel
// order.  This tool exercises that seam and writes the result out.
//
// Usage: krkrbitmap <out.bmp>

#include "tjsCommHead.h"
#include "LayerBitmapIntf.h"
#include "RenderManager.h"
#include "tvpgl.h"

#include <SDL.h>

#include <cstdio>
#include <cstring>
#include <unistd.h>

// the engine's bitmap classes are abstract only because the platform supplies
// the render manager; a software bitmap has none
class tTVPBitmap_Soft : public iTVPBaseBitmap
{
public:
	iTVPRenderManager* GetRenderManager() override { return nullptr; }
};

int main(int argc, char** argv)
{
	const char* out = argc > 1 ? argv[1] : "bitmap.bmp";
	const tjs_uint w = 320, h = 200;

	// the engine's blitters are function pointers that something must point at
	// the C implementations before any drawing happens
	TVPInitTVPGL();

	tTVPBitmap_Soft bmp;
	bmp.Recreate(w, h, 32);
	printf("bitmap: %ux%u, %u bpp, pitch %d\n", (unsigned)bmp.GetWidth(),
		(unsigned)bmp.GetHeight(), (unsigned)bmp.GetBPP(), (int)bmp.GetPitchBytes());

	// A gradient written through the scan lines.  The engine's own Fill and
	// FillColor go through a render method ("FillARGB"), which a software back
	// end has to provide before they can be used - that is the next component,
	// and until it lands the scan lines are the honest way in.
	for(tjs_uint y = 0; y < h; y++)
	{
		unsigned g = 0x20 + (unsigned)(y * 0xa0 / h);
		unsigned colour = 0xff000000u | (g << 16) | (g << 8) | g;
		unsigned* line = (unsigned*)bmp.GetScanLineForWrite(y);
		for(tjs_uint x = 0; x < w; x++)
			line[x] = colour;
	}

	// a block written over it, to show the buffer is addressable per pixel
	for(tjs_uint y = 40; y < 120; y++)
	{
		unsigned* line = (unsigned*)bmp.GetScanLineForWrite(y);
		for(tjs_uint x = 40; x < 280; x++)
			line[x] = 0xff0080ffu;
	}

	// the point accessors must agree with the scan lines
	bmp.SetPoint(10, 10, 0x00ff00ffu);
	printf("point (10,10) = 0x%08x, (11,10) = 0x%08x\n",
		bmp.GetPoint(10, 10), bmp.GetPoint(11, 10));

	const void* line = bmp.GetScanLine(10);
	printf("scan line 10 at %p, first pixel 0x%08x\n", line,
		line ? *(const unsigned*)line : 0u);

	SDL_Surface* surf = SDL_CreateRGBSurfaceWithFormatFrom(
		(void*)bmp.GetScanLine(0), (int)w, (int)h, 32, bmp.GetPitchBytes(),
		SDL_PIXELFORMAT_ABGR8888);
	if(surf == nullptr)
	{
		fprintf(stderr, "krkrbitmap: %s\n", SDL_GetError());
		return 1;
	}
	int rc = SDL_SaveBMP(surf, out) == 0 ? 0 : 1;
	SDL_FreeSurface(surf);
	printf(rc == 0 ? "wrote %s\n" : "cannot write %s\n", out);
	fflush(stdout);
	_exit(rc);
}
