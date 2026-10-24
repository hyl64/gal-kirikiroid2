/*
 * This file is part of the macOS port of Kirikiroid2 (zeas2/Kirikiroid2).
 *
 * Texture2D_Soft.cpp - see the header; a texture is a block of the engine's
 * 32-bit pixels, and the accessors are the ones the layers and the blitters use.
 */

#include "Texture2D_Soft.h"

#include <cstring>

// iTVPTexture2D::Release lives in RenderManager.cpp upstream, which belongs to
// the cocos2d renderer; a software texture refcounts the same way.
void iTVPTexture2D::Release()
{
	--RefCount;
	if(RefCount <= 0)
		delete this;
}

tTVPTexture2D_Soft::tTVPTexture2D_Soft(tjs_int w, tjs_int h, bool opaque)
	: iTVPTexture2D(w, h), Stride(w * 4), Opaque(opaque)
{
	Storage.assign((size_t)Stride * (size_t)(h > 0 ? h : 0), 0);
}

void tTVPTexture2D_Soft::Release()
{
	--RefCount;
	if(RefCount <= 0)
		delete this;
}

TVPTextureFormat::e tTVPTexture2D_Soft::GetFormat() const
{
	return TVPTextureFormat::RGBA;
}

const void* tTVPTexture2D_Soft::GetScanLineForRead(tjs_uint l)
{
	if(l >= (tjs_uint)Height)
		return nullptr;
	return Storage.data() + (size_t)l * Stride;
}

void* tTVPTexture2D_Soft::GetScanLineForWrite(tjs_uint l)
{
	if(l >= (tjs_uint)Height)
		return nullptr;
	return Storage.data() + (size_t)l * Stride;
}

tjs_int tTVPTexture2D_Soft::GetPitch() const
{
	return Stride;
}

void tTVPTexture2D_Soft::SetSize(unsigned int w, unsigned int h)
{
	Width = (tjs_int)w;
	Height = (tjs_int)h;
	Stride = (int)w * 4;
	Storage.assign((size_t)Stride * h, 0);
}

// copy the given rectangle of the caller's pixels into the texture; the format
// is RGBA (the engine's own layout) and a grayscale source expands to it
void tTVPTexture2D_Soft::Update(const void* pixel, TVPTextureFormat::e format, int pitch,
	const tTVPRect& rc)
{
	if(pixel == nullptr)
		return;
	const tjs_uint8* src = (const tjs_uint8*)pixel;
	int x0 = rc.left, y0 = rc.top;
	int w = rc.right - rc.left + 1;
	int h = rc.bottom - rc.top + 1;
	if(x0 < 0 || y0 < 0 || w <= 0 || h <= 0)
		return;
	if(x0 + w > Width)
		w = Width - x0;
	if(y0 + h > Height)
		h = Height - y0;
	for(int y = 0; y < h; y++)
	{
		tjs_uint8* dst = Storage.data() + (size_t)(y0 + y) * Stride + (size_t)x0 * 4;
		const tjs_uint8* s = src + (size_t)y * pitch;
		if(format == TVPTextureFormat::Gray)
		{
			for(int x = 0; x < w; x++)
			{
				tjs_uint8 v = s[x];
				dst[x * 4 + 0] = v;
				dst[x * 4 + 1] = v;
				dst[x * 4 + 2] = v;
				dst[x * 4 + 3] = 0xff;
			}
		}
		else if(format == TVPTextureFormat::RGB)
		{
			for(int x = 0; x < w; x++)
			{
				dst[x * 4 + 0] = s[x * 3 + 0];
				dst[x * 4 + 1] = s[x * 3 + 1];
				dst[x * 4 + 2] = s[x * 3 + 2];
				dst[x * 4 + 3] = 0xff;
			}
		}
		else
			memcpy(dst, s, (size_t)w * 4);
	}
}

uint32_t tTVPTexture2D_Soft::GetPoint(int x, int y)
{
	if(x < 0 || y < 0 || x >= Width || y >= Height)
		return 0;
	return *(const uint32_t*)(Storage.data() + (size_t)y * Stride + (size_t)x * 4);
}

void tTVPTexture2D_Soft::SetPoint(int x, int y, uint32_t clr)
{
	if(x < 0 || y < 0 || x >= Width || y >= Height)
		return;
	*(uint32_t*)(Storage.data() + (size_t)y * Stride + (size_t)x * 4) = clr;
}

bool tTVPTexture2D_Soft::IsStatic()
{
	return false;
}

bool tTVPTexture2D_Soft::IsOpaque()
{
	return Opaque;
}

cocos2d::Texture2D* tTVPTexture2D_Soft::GetAdapterTexture(cocos2d::Texture2D* origTex)
{
	(void)origTex;
	return nullptr;
}
