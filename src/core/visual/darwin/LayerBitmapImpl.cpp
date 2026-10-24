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
#include "Texture2D_Soft.h"

// ---------------------------------------------------------------------------
// lifetime and allocation
// ---------------------------------------------------------------------------

// The bitmap owns one reference on its texture; the layers hold their own.
tTVPNativeBaseBitmap::tTVPNativeBaseBitmap()
{
	Bitmap = nullptr;
	Font.Height = -12;
	Font.Flags = 0;
	Font.Angle = 0;
	FontChanged = true;
	GlobalFontState = 0;
	PrerenderedFont = nullptr;
	AscentOfsX = AscentOfsY = 0;
	RadianAngle = 0;
	FontHash = 0;
	TextWidth = TextHeight = 0;
}

tTVPNativeBaseBitmap::tTVPNativeBaseBitmap(const tTVPNativeBaseBitmap& r)
	: tTVPNativeBaseBitmap()
{
	Assign(r);
}

tTVPNativeBaseBitmap::~tTVPNativeBaseBitmap()
{
	if(Bitmap != nullptr)
		Bitmap->Release();
	Bitmap = nullptr;
}

// give the bitmap a texture of its own: the engine renders into 32-bit pixels
// here, so the depth the caller asks for is what a caller gets
void tTVPNativeBaseBitmap::Recreate(tjs_uint w, tjs_uint h, tjs_uint bpp)
{
	(void)bpp;
	if(Bitmap != nullptr)
		Bitmap->Release();
	Bitmap = new tTVPTexture2D_Soft((tjs_int)w, (tjs_int)h, false);
}

void tTVPNativeBaseBitmap::Recreate()
{
	tjs_uint w = Bitmap != nullptr ? Bitmap->GetInternalWidth() : 0;
	tjs_uint h = Bitmap != nullptr ? Bitmap->GetInternalHeight() : 0;
	Recreate(w, h, 32);
}

void tTVPNativeBaseBitmap::SetSize(tjs_uint w, tjs_uint h, bool keepimage)
{
	if(Bitmap != nullptr && Bitmap->GetInternalWidth() == w && Bitmap->GetInternalHeight() == h)
		return;
	tTVPTexture2D_Soft* old = keepimage ? dynamic_cast<tTVPTexture2D_Soft*>(Bitmap) : nullptr;
	tTVPTexture2D_Soft* fresh = new tTVPTexture2D_Soft((tjs_int)w, (tjs_int)h, false);
	if(old != nullptr && Bitmap != nullptr)
	{
		// keep the overlapping corner, as the engine expects of SetSize
		tjs_uint cw = old->GetInternalWidth() < w ? old->GetInternalWidth() : w;
		tjs_uint ch = old->GetInternalHeight() < h ? old->GetInternalHeight() : h;
		for(tjs_uint y = 0; y < ch; y++)
			memcpy(fresh->GetScanLineForWrite(y), old->GetScanLineForRead(y), (size_t)cw * 4);
	}
	if(Bitmap != nullptr)
		Bitmap->Release();
	Bitmap = fresh;
}

void tTVPNativeBaseBitmap::SetWidth(tjs_uint w)
{
	SetSize(w, GetHeight(), true);
}

void tTVPNativeBaseBitmap::SetHeight(tjs_uint h)
{
	SetSize(GetWidth(), h, true);
}

// ---------------------------------------------------------------------------
// depth and ownership
// ---------------------------------------------------------------------------

tjs_uint tTVPNativeBaseBitmap::GetBPP() const
{
	return 32;
}

bool tTVPNativeBaseBitmap::Is32BPP() const
{
	return true;
}

bool tTVPNativeBaseBitmap::Is8BPP() const
{
	return false;
}

bool tTVPNativeBaseBitmap::IsOpaque() const
{
	return Bitmap != nullptr ? Bitmap->IsOpaque() : false;
}

bool tTVPNativeBaseBitmap::Assign(const tTVPNativeBaseBitmap& rhs)
{
	if(&rhs == this)
		return true;
	if(rhs.Bitmap != nullptr)
	{
		rhs.Bitmap->AddRef();
		if(Bitmap != nullptr)
			Bitmap->Release();
		Bitmap = rhs.Bitmap;
	}
	return true;
}

bool tTVPNativeBaseBitmap::AssignBitmap(const tTVPNativeBaseBitmap& rhs)
{
	return Assign(rhs);
}

bool tTVPNativeBaseBitmap::AssignTexture(iTVPTexture2D* tex)
{
	if(tex != nullptr)
		tex->AddRef();
	if(Bitmap != nullptr)
		Bitmap->Release();
	Bitmap = tex;
	return true;
}

bool tTVPNativeBaseBitmap::IsIndependent() const
{
	return Bitmap != nullptr;
}

void tTVPNativeBaseBitmap::Independ()
{
	// a soft texture is already a buffer of its own
	if(Bitmap == nullptr)
		return;
}

void tTVPNativeBaseBitmap::IndependNoCopy()
{
	Independ();
}

iTVPTexture2D* tTVPNativeBaseBitmap::GetTextureForRender(bool isBlendTarget, const tTVPRect* rc)
{
	(void)isBlendTarget;
	(void)rc;
	return Bitmap;
}

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
