/*
 * This file is part of the macOS port of Kirikiroid2 (zeas2/Kirikiroid2).
 *
 * Texture2D_Soft.h - the software texture the desktop back end renders into
 *
 * Upstream every iTVPTexture2D belongs to the render manager: the Windows back
 * end's textures hold a DIB section, the Android one's hold a GL texture.  The
 * desktop back end has no GPU path of its own yet, so its textures are plain
 * memory in the engine's pixel format (0xAABBGGRR), the same buffer the
 * bitmaps, the draw device and the blitters all work on.
 */
#ifndef KRKR_VISUAL_DARWIN_TEXTURE2D_SOFT_H
#define KRKR_VISUAL_DARWIN_TEXTURE2D_SOFT_H

#include "tjsCommHead.h"
#include "RenderManager.h"

#include <vector>

class tTVPTexture2D_Soft : public iTVPTexture2D
{
	std::vector<tjs_uint8> Storage;
	int Stride;
	bool Opaque;

public:
	tTVPTexture2D_Soft(tjs_int w, tjs_int h, bool opaque = false);

	void Release() override;
	TVPTextureFormat::e GetFormat() const override;
	const void* GetScanLineForRead(tjs_uint l) override;
	void* GetScanLineForWrite(tjs_uint l) override;
	tjs_int GetPitch() const override;
	void SetSize(unsigned int w, unsigned int h) override;
	void Update(const void* pixel, TVPTextureFormat::e format, int pitch, const tTVPRect& rc) override;
	uint32_t GetPoint(int x, int y) override;
	void SetPoint(int x, int y, uint32_t clr) override;
	bool IsStatic() override;
	bool IsOpaque() override;

	// The engine's textures can hand cocos2d an adapter for its GL pipeline;
	// a software texture has no such thing to give.
	cocos2d::Texture2D* GetAdapterTexture(cocos2d::Texture2D* origTex) override;
};

#endif // KRKR_VISUAL_DARWIN_TEXTURE2D_SOFT_H
