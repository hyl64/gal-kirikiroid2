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
#include "tjsUtils.h" // tTJSCriticalSection, which the allocator holds
#include "win32/BitmapBitsAlloc.h"
#include "tjsError.h"
#include "MsgIntf.h"
#include "PrerenderedFont.h"
#include "FontRasterizer.h"

// the rasterizer and the font list, as the Windows back end declares them
extern FontRasterizer* GetCurrentRasterizer();
extern tjs_int TVPGlobalFontStateMagic; // bumped when the rasterizer changes
extern tTVPPrerenderedFont* TVPGetPrerenderedMappedFont(const tTVPFont& font);
extern void TVPGetAllFontList(std::vector<ttstr>& list);
extern void TVPMapPrerenderedFont(const tTVPFont& font, const ttstr& storage);
extern void TVPUnmapPrerenderedFont();

// the window options the engine keeps (bitmap alignment and the like) are
// initialised on first use; the desktop defaults need nothing set up
void TVPInitWindowOptions()
{
}


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
	// the render manager owns every texture; the software one it hands out is
	// the engine's own (visual/RenderManager.cpp)
	Bitmap = TVPGetRenderManager()->CreateTexture2D(nullptr, 0, w, h, TVPTextureFormat::RGBA);
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
	iTVPTexture2D* old = keepimage ? Bitmap : nullptr;
	iTVPTexture2D* fresh = TVPGetRenderManager()->CreateTexture2D(nullptr, 0, w, h, TVPTextureFormat::RGBA);
	if(old != nullptr && fresh != nullptr)
	{
		// keep the overlapping corner, as the engine expects of SetSize
		tjs_uint cw = old->GetInternalWidth() < w ? old->GetInternalWidth() : w;
		tjs_uint ch = old->GetInternalHeight() < h ? old->GetInternalHeight() : h;
		for(tjs_uint y = 0; y < ch; y++)
			memcpy(fresh->GetScanLineForWrite(y), old->GetScanLineForRead(y), (size_t)cw * 4);
	}
	if(old != nullptr)
		old->Release();
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

// ---------------------------------------------------------------------------
// tTVPBitmap : the pixel buffer
// ---------------------------------------------------------------------------
//
// No platform call is in these: they allocate through BitmapInfomation and
// tTVPBitmapBitsAlloc, which are portable, so the Windows back end's code for
// this class is the code here too (moved, not rewritten).  The textures and the
// image loaders hand these buffers around.
// tTVPBitmap : internal bitmap object
//---------------------------------------------------------------------------
/*
	important:
	Note that each lines must be started at tjs_uint32 ( 4bytes ) aligned address.
	This is the default Windows bitmap allocate behavior.
*/
tTVPBitmap::tTVPBitmap(tjs_uint width, tjs_uint height, tjs_uint bpp)
{
	// tTVPBitmap constructor

	TVPInitWindowOptions(); // ensure window/bitmap usage options are initialized

	RefCount = 1;

	Allocate(width, height, bpp); // allocate initial bitmap
}
//---------------------------------------------------------------------------
tTVPBitmap::tTVPBitmap(tjs_uint width, tjs_uint height, tjs_uint bpp, void* bits)
{
	// tTVPBitmap constructor
	TVPInitWindowOptions(); // ensure window/bitmap usage options are initialized

	RefCount = 1;

	BitmapInfo = new BitmapInfomation( width, height, bpp );
	Width = width;
	Height = height;
	PitchBytes = BitmapInfo->GetPitchBytes();
	PitchStep = PitchBytes;

	// set bitmap bits
	try
	{
		Bits = bits;
		if( bpp == 8 ) {
			Palette = new tjs_uint[DEFAULT_PALETTE_COUNT];
			ActualPalCount = 0;
		} else {
			Palette = NULL;
			ActualPalCount = 0;
		}
	}
	catch(...)
	{
		delete BitmapInfo;
		BitmapInfo = NULL;
		throw;
	}
}
//---------------------------------------------------------------------------
tTVPBitmap::~tTVPBitmap()
{
	tTVPBitmapBitsAlloc::Free(Bits);
	delete BitmapInfo;
	if( Palette ) delete Palette;
}
//---------------------------------------------------------------------------
tTVPBitmap::tTVPBitmap(const tTVPBitmap & r)
{
	// constructor for cloning bitmap
	TVPInitWindowOptions(); // ensure window/bitmap usage options are initialized

	RefCount = 1;

	// allocate bitmap which has the same metrics to r
	Allocate(r.GetWidth(), r.GetHeight(), r.GetBPP());

	// copy BitmapInfo
	*BitmapInfo = *r.BitmapInfo;

	// copy Bits
	if(r.Bits) memcpy(Bits, r.Bits, r.BitmapInfo->GetImageSize() );
	if(r.Palette) {
		memcpy(Palette, r.Palette, sizeof(tjs_uint)*DEFAULT_PALETTE_COUNT );
		ActualPalCount = r.ActualPalCount;
	}

	// copy pitch
	PitchBytes = r.PitchBytes;
	PitchStep = r.PitchStep;
}
//---------------------------------------------------------------------------
void tTVPBitmap::Allocate(tjs_uint width, tjs_uint height, tjs_uint bpp)
{
	// allocate bitmap bits
	// bpp must be 8 or 32

	// create BITMAPINFO
	BitmapInfo = new BitmapInfomation( width, height, bpp );

	Width = width;
	Height = height;
	PitchBytes = BitmapInfo->GetPitchBytes();
	PitchStep = PitchBytes;

	// allocate bitmap bits
	try
	{
		Bits = tTVPBitmapBitsAlloc::Alloc(BitmapInfo->GetImageSize(), width, height);
		if( bpp == 8 ) {
			Palette = new tjs_uint[DEFAULT_PALETTE_COUNT];
			ActualPalCount = 0;
		} else {
			Palette = NULL;
			ActualPalCount = 0;
		}
	}
	catch(...)
	{
		delete BitmapInfo;
		BitmapInfo = NULL;
		throw;
	}
}
//---------------------------------------------------------------------------
void * tTVPBitmap::GetScanLine(tjs_uint l) const
{
	if((tjs_int)l>=BitmapInfo->GetHeight() )
	{
		TVPThrowExceptionMessage(TVPScanLineRangeOver, ttstr((tjs_int)l),
			ttstr((tjs_int)BitmapInfo->GetHeight()-1));
	}

	return l * PitchBytes + (tjs_uint8*)Bits;
}
//---------------------------------------------------------------------------
void tTVPBitmap::SetPaletteCount( tjs_uint count ) {
	if( !Is8bit() ) TVPThrowExceptionMessage(TVPInvalidOperationFor32BPP);
	if( count >= DEFAULT_PALETTE_COUNT )
		TVPThrowExceptionMessage(TJSRangeError);

	ActualPalCount = count;
}
//---------------------------------------------------------------------------

//---------------------------------------------------------------------------

// ---------------------------------------------------------------------------
// font list and text metrics
// ---------------------------------------------------------------------------
//
// No platform call in these: they ask the rasterizer and the prerendered font
// for advances and heights, the same arithmetic on every back end (moved from
// visual/win32/LayerBitmapImpl.cpp).
void tTVPNativeBaseBitmap::GetFontList(tjs_uint32 flags, std::vector<ttstr> &list)
{
	ApplyFont();
	std::vector<ttstr> ansilist;
	TVPGetAllFontList(ansilist);
	for(std::vector<ttstr>::iterator i = ansilist.begin(); i != ansilist.end(); i++)
		list.push_back(*i);
}

void tTVPNativeBaseBitmap::GetTextSize(const ttstr & text)
{
	ApplyFont();

	if(text != CachedText)
	{
		CachedText = text;

		if(PrerenderedFont)
		{
			tjs_uint width = 0;
			const tjs_char *buf = text.c_str();
			while(*buf)
			{
				const tTVPPrerenderedCharacterItem * item =
					PrerenderedFont->Find(*buf);
				if(item != NULL)
				{
					width += item->Inc;
				}
				else
				{
					tjs_int w, h;
					GetCurrentRasterizer()->GetTextExtent( *buf, w, h );
					width += w;
				}
				buf++;
			}
			TextWidth = width;
			TextHeight = std::abs(Font.Height);
		}
		else
		{
			tjs_uint width = 0;
			const tjs_char *buf = text.c_str();

			while(*buf)
			{
				tjs_int w, h;
				GetCurrentRasterizer()->GetTextExtent( *buf, w, h );
				width += w;
				buf++;
			}
			TextWidth = width;
			TextHeight = std::abs(Font.Height);
		}
	}
}

tjs_int tTVPNativeBaseBitmap::GetTextWidth(const ttstr & text)
{
	GetTextSize(text);
	return TextWidth;
}

tjs_int tTVPNativeBaseBitmap::GetTextHeight(const ttstr & text)
{
	GetTextSize(text);
	return TextHeight;
}

double tTVPNativeBaseBitmap::GetEscWidthX(const ttstr & text)
{
	GetTextSize(text);
	return cos(RadianAngle) * TextWidth;
}

double tTVPNativeBaseBitmap::GetEscWidthY(const ttstr & text)
{
	GetTextSize(text);
	return sin(RadianAngle) * (-TextWidth);
}

double tTVPNativeBaseBitmap::GetEscHeightX(const ttstr & text)
{
	GetTextSize(text);
	return sin(RadianAngle) * TextHeight;
}

double tTVPNativeBaseBitmap::GetEscHeightY(const ttstr & text)
{
	GetTextSize(text);
	return cos(RadianAngle) * TextHeight;
}

void tTVPNativeBaseBitmap::GetFontGlyphDrawRect( const ttstr & text, struct tTVPRect& area )
{
	ApplyFont();
	GetCurrentRasterizer()->GetGlyphDrawRect( text, area );
}

// ---------------------------------------------------------------------------
// the font the bitmap draws with
// ---------------------------------------------------------------------------
//
// Portable: it resolves the face through the rasterizer and asks the prerendered
// font map for a stand-in when the game shipped bitmap glyphs.
void tTVPNativeBaseBitmap::ApplyFont()
{
	// apply font
	if(FontChanged || GlobalFontState != TVPGlobalFontStateMagic)
	{
		Independ();

		FontChanged = false;
		GlobalFontState = TVPGlobalFontStateMagic;
		CachedText.Clear();
		TextWidth = TextHeight = 0;

		if(PrerenderedFont) PrerenderedFont->Release();
		PrerenderedFont = TVPGetPrerenderedMappedFont(Font);

		// compute ascent offset
		GetCurrentRasterizer()->ApplyFont( this, true );
		tjs_int ascent = GetCurrentRasterizer()->GetAscentHeight();
		RadianAngle = Font.Angle * (M_PI/1800);
		double angle90 = RadianAngle + M_PI_2;
		AscentOfsX = static_cast<tjs_int>(-cos(angle90) * ascent);
		AscentOfsY = static_cast<tjs_int>(sin(angle90) * ascent);

		// compute font hash
		FontHash = tTJSHashFunc<ttstr>::Make(Font.Face);
		FontHash ^= Font.Height ^ Font.Flags ^ Font.Angle;
	}
	else
	{
		GetCurrentRasterizer()->ApplyFont( this, false );
	}
}
