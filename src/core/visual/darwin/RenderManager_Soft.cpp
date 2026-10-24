/*
 * This file is part of the macOS port of Kirikiroid2 (zeas2/Kirikiroid2).
 *
 * RenderManager_Soft.cpp - the desktop back end's render manager
 *
 * Upstream the manager is the graphics back end: the Windows build has a
 * Direct3D one, the Android build a cocos2d/GLES one, and the layers ask it for
 * textures and for the render methods that compose them.  This one has no GPU:
 * its textures are plain memory (Texture2D_Soft) and its composition runs on
 * the CPU through the engine's own blitters.  That is enough for a game to draw
 * a picture and its text, which is what the port needs first.
 */

#include "tjsCommHead.h"
#include "RenderManager.h"
#include "tjsString.h"
#include "SysInitIntf.h"

#include "Texture2D_Soft.h"

#include <cstring>

// the tables the C blitters read
extern void TVPGL_C_Init();

class tTVPRenderManager_Soft : public iTVPRenderManager
{
public:
	iTVPTexture2D* CreateTexture2D(const void* pixel, int pitch, unsigned int w, unsigned int h,
		TVPTextureFormat::e format, int flags) override;
	iTVPTexture2D* CreateTexture2D(tTVPBitmap* bmp) override;
	iTVPTexture2D* CreateTexture2D(TJS::tTJSBinaryStream* s) override;
	iTVPTexture2D* CreateTexture2D(unsigned int neww, unsigned int newh, iTVPTexture2D* tex) override;
	const char* GetName() override;
	bool GetRenderStat(unsigned int& drawCount, uint64_t& vmemsize) override;
	void OperateRect(iTVPRenderMethod* method, iTVPTexture2D* tar, iTVPTexture2D* reftar,
		const tTVPRect& rctar, const tRenderTexRectArray& textures) override;
	void OperateTriangles(iTVPRenderMethod* method, int nTriangles, iTVPTexture2D* target,
		iTVPTexture2D* reftar, const tTVPRect& rcclip, const tTVPPointD* pttar,
		const tRenderTexQuadArray& textures) override;
	void OperatePerspective(iTVPRenderMethod* method, int nQuads, iTVPTexture2D* target,
		iTVPTexture2D* reftar, const tTVPRect& rcclip, const tTVPPointD* pttar,
		const tRenderTexQuadArray& textures) override;

	bool IsSoftware() override { return true; }
};

iTVPTexture2D* tTVPRenderManager_Soft::CreateTexture2D(const void* pixel, int pitch,
	unsigned int w, unsigned int h, TVPTextureFormat::e format, int flags)
{
	(void)flags;
	tTVPTexture2D_Soft* tex = new tTVPTexture2D_Soft((tjs_int)w, (tjs_int)h,
		format != TVPTextureFormat::RGBA);
	if(pixel != nullptr)
	{
		tTVPRect rc(0, 0, (tjs_int)w - 1, (tjs_int)h - 1);
		tex->Update(pixel, format, pitch, rc);
	}
	return tex;
}

iTVPTexture2D* tTVPRenderManager_Soft::CreateTexture2D(tTVPBitmap* bmp)
{
	(void)bmp; // province images; nothing asks for one yet
	return nullptr;
}

iTVPTexture2D* tTVPRenderManager_Soft::CreateTexture2D(TJS::tTJSBinaryStream* s)
{
	(void)s; // compressed and special formats have no loader here
	return nullptr;
}

// a resized copy: nearest neighbour, which is what a software fallback can
// promise without the resampling lattice the GL back ends have
iTVPTexture2D* tTVPRenderManager_Soft::CreateTexture2D(unsigned int neww, unsigned int newh,
	iTVPTexture2D* tex)
{
	if(tex == nullptr)
		return nullptr;
	tTVPTexture2D_Soft* out = new tTVPTexture2D_Soft((tjs_int)neww, (tjs_int)newh, false);
	tjs_uint sw = tex->GetInternalWidth(), sh = tex->GetInternalHeight();
	if(sw == 0 || sh == 0)
		return out;
	for(unsigned int y = 0; y < newh; y++)
	{
		unsigned sy = (unsigned)((uint64_t)y * sh / newh);
		for(unsigned int x = 0; x < neww; x++)
		{
			unsigned sx = (unsigned)((uint64_t)x * sw / neww);
			out->SetPoint((int)x, (int)y, tex->GetPoint((int)sx, (int)sy));
		}
	}
	return out;
}

const char* tTVPRenderManager_Soft::GetName()
{
	return "software";
}

bool tTVPRenderManager_Soft::GetRenderStat(unsigned int& drawCount, uint64_t& vmemsize)
{
	drawCount = 0;
	vmemsize = 0;
	return true;
}

// Copy the source rectangle of the first texture into the target rectangle.
// The engine's software blitters do the per-pixel work when a render method is
// given; without one this is the plain 1:1 copy a layer update needs.
void tTVPRenderManager_Soft::OperateRect(iTVPRenderMethod* method, iTVPTexture2D* tar,
	iTVPTexture2D* reftar, const tTVPRect& rctar, const tRenderTexRectArray& textures)
{
	(void)method;
	(void)reftar;
	if(tar == nullptr || textures.size() == 0)
		return;
	iTVPTexture2D* src = textures[0].first;
	const tTVPRect& srcRect = textures[0].second;
	if(src == nullptr)
		return;
	int w = rctar.right - rctar.left + 1;
	int h = rctar.bottom - rctar.top + 1;
	for(int y = 0; y < h; y++)
	{
		const void* s = src->GetScanLineForRead((tjs_uint)(srcRect.top + y));
		if(s == nullptr)
			continue;
		void* d = tar->GetScanLineForWrite((tjs_uint)(rctar.top + y));
		if(d == nullptr)
			continue;
		memcpy((tjs_uint8*)d + (size_t)rctar.left * 4,
			(const tjs_uint8*)s + (size_t)srcRect.left * 4, (size_t)w * 4);
	}
}

void tTVPRenderManager_Soft::OperateTriangles(iTVPRenderMethod* method, int nTriangles,
	iTVPTexture2D* target, iTVPTexture2D* reftar, const tTVPRect& rcclip, const tTVPPointD* pttar,
	const tRenderTexQuadArray& textures)
{
	(void)method;
	(void)nTriangles;
	(void)target;
	(void)reftar;
	(void)rcclip;
	(void)pttar;
	(void)textures;
}

void tTVPRenderManager_Soft::OperatePerspective(iTVPRenderMethod* method, int nQuads,
	iTVPTexture2D* target, iTVPTexture2D* reftar, const tTVPRect& rcclip, const tTVPPointD* pttar,
	const tRenderTexQuadArray& textures)
{
	(void)method;
	(void)nQuads;
	(void)target;
	(void)reftar;
	(void)rcclip;
	(void)pttar;
	(void)textures;
}

// The manager's own method lookup: upstream this hands out the GL programs the
// cocos renderer compiles per blend mode.  A software back end has no programs,
// and the blitters it uses instead are chosen by the caller's blend mode, so
// there is nothing to hand out.
iTVPRenderMethod* iTVPRenderManager::GetRenderMethod(const char* name, uint32_t* hint)
{
	(void)name;
	(void)hint;
	return nullptr;
}

iTVPRenderMethod* iTVPRenderManager::GetRenderMethod(int nTex, bool useStencil, int flags)
{
	(void)nTex;
	(void)useStencil;
	(void)flags;
	return nullptr;
}

// one manager for the whole process, which is how the engine uses it
static tTVPRenderManager_Soft* gRenderManager = nullptr;

iTVPRenderManager* TVPGetRenderManager()
{
	if(gRenderManager == nullptr)
	{
		TVPGL_C_Init();
		gRenderManager = new tTVPRenderManager_Soft();
	}
	return gRenderManager;
}

iTVPRenderManager* TVPGetSoftwareRenderManager()
{
	return TVPGetRenderManager();
}
