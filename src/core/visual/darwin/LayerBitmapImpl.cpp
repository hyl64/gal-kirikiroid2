/*
 * This file is part of the macOS port of Kirikiroid2 (zeas2/Kirikiroid2).
 *
 * LayerBitmapImpl.cpp - the macOS (SDL2) bitmap layer, first slice
 *
 * The engine's pictures are tTVPNativeBaseBitmap objects, and upstream keeps
 * their pixels in an iTVPTexture2D: the Windows back end (visual/win32/
 * LayerBitmapImpl.cpp, 1600 lines of GDI) and the Android one both delegate to
 * a texture, so the class itself carries no pixel storage - only the metrics
 * and the scan lines, which is exactly what the image decoders ask it for.
 *
 * What is here is that delegation (width, height, scan lines) plus the one
 * cache hook the PNG decoder calls.  The rest of the class - the text and font
 * calls, the blits, the DIB-style allocation - is the platform bitmap layer
 * that still has to be written for SDL, and it will join these functions in
 * this file rather than replace them.
 */

#include "tjsCommHead.h"
#include "RenderManager.h" // iTVPTexture2D
#include "LayerBitmapIntf.h"

// ---------------------------------------------------------------------------
// metrics and scan lines, delegated to the texture that holds the pixels
// ---------------------------------------------------------------------------

tjs_uint tTVPNativeBaseBitmap::GetWidth() const
{
	return Bitmap != nullptr ? Bitmap->GetInternalWidth() : 0;
}

tjs_uint tTVPNativeBaseBitmap::GetHeight() const
{
	return Bitmap != nullptr ? Bitmap->GetInternalHeight() : 0;
}

const void* tTVPNativeBaseBitmap::GetScanLine(tjs_uint l) const
{
	return Bitmap != nullptr ? Bitmap->GetScanLineForRead(l) : nullptr;
}

void* tTVPNativeBaseBitmap::GetScanLineForWrite(tjs_uint l)
{
	return Bitmap != nullptr ? Bitmap->GetScanLineForWrite(l) : nullptr;
}

tjs_int tTVPNativeBaseBitmap::GetPitchBytes() const
{
	return Bitmap != nullptr ? Bitmap->GetPitch() : 0;
}

// ---------------------------------------------------------------------------
// the graphics cache the decoders clear after loading a picture
// ---------------------------------------------------------------------------

// The Windows back end keeps decoded header information per storage name to
// avoid asking the disk twice.  This slice of the port has no such cache yet,
// so there is nothing to clear; the call exists because the decoders make it.
void TVPClearGraphicCache(void)
{
}
