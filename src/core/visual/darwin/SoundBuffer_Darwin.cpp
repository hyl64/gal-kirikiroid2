/*
 * This file is part of the macOS port of Kirikiroid2 (zeas2/Kirikiroid2).
 *
 * SoundBuffer_Darwin.cpp - the script-visible wave sound buffer
 *
 * The Windows implementation is waveOut; there is no audio device on this back end
 * yet.  What a framework does at start-up is create buffers, open files and set
 * volumes, so this class keeps that state and answers consistently - playing
 * nothing.  When an audio path exists, these are the methods it will fill in.
 */

#include "tjsCommHead.h"
#include "tjsNative.h"
#include "tjsVariant.h"
#include "StorageIntf.h"

namespace
{
	class WaveInstance : public tTJSNativeInstance
	{
	public:
		std::string Storage;
		tjs_int Volume = 0;      // -10000 .. 0 dB, as the engine uses
		tjs_int Pan = 0;
		bool Playing = false;
		bool Looping = false;
		bool Opened = false;
		double Position = 0.0;
		double TotalTime = 0.0;
	};
}

class tTJSNC_WaveSoundBuffer_Darwin : public tTJSNativeClass
{
	typedef tTJSNativeClass inherited;

public:
	tTJSNC_WaveSoundBuffer_Darwin();
	static tjs_uint32 ClassID;

private:
	iTJSNativeInstance* CreateNativeInstance() override;
};

tjs_uint32 tTJSNC_WaveSoundBuffer_Darwin::ClassID = (tjs_uint32)-1;

tTJSNC_WaveSoundBuffer_Darwin::tTJSNC_WaveSoundBuffer_Darwin()
	: inherited(TJS_W("WaveSoundBuffer"))
{
	TJS_BEGIN_NATIVE_MEMBERS(WaveSoundBuffer)
	TJS_DECL_EMPTY_FINALIZE_METHOD

	TJS_BEGIN_NATIVE_CONSTRUCTOR_DECL(/*var.name*/ _this,
		/*var.type*/ WaveInstance, /*TJS class name*/ WaveSoundBuffer)
	{
		return TJS_S_OK;
	}
	TJS_END_NATIVE_CONSTRUCTOR_DECL(/*TJS class name*/ WaveSoundBuffer)

	// open: the file it would play; recorded, and reported as not opened
	TJS_BEGIN_NATIVE_METHOD_DECL(/*func. name*/ open)
	{
		TJS_GET_NATIVE_INSTANCE(_this, WaveInstance);
		if(numparams >= 1 && param[0]->Type() == tvtString)
		{
			tTJSNarrowStringHolder narrow(param[0]->GetString());
			_this->Storage = (const char*)narrow;
		}
		_this->Opened = true;
		return TJS_S_OK;
	}
	TJS_END_NATIVE_METHOD_DECL(/*func. name*/ open)

	TJS_BEGIN_NATIVE_METHOD_DECL(/*func. name*/ close)
	{
		TJS_GET_NATIVE_INSTANCE(_this, WaveInstance);
		_this->Opened = false;
		_this->Playing = false;
		return TJS_S_OK;
	}
	TJS_END_NATIVE_METHOD_DECL(/*func. name*/ close)

#define WAVE_SIMPLE(name, action)                                              \
	TJS_BEGIN_NATIVE_METHOD_DECL(name)                                          \
	{                                                                          \
		TJS_GET_NATIVE_INSTANCE(_this, WaveInstance);                           \
		action                                                                  \
		return TJS_S_OK;                                                        \
	}                                                                          \
	TJS_END_NATIVE_METHOD_DECL(name)

	WAVE_SIMPLE(/*func. name*/ play, _this->Playing = true;)
	WAVE_SIMPLE(/*func. name*/ stop, _this->Playing = false;)
	WAVE_SIMPLE(/*func. name*/ pause, _this->Playing = false;)
	WAVE_SIMPLE(/*func. name*/ fade, (void)numparams; (void)param;)
#undef WAVE_SIMPLE

#define WAVE_INT_PROP(name, field)                                             \
	TJS_BEGIN_NATIVE_PROP_DECL(name)                                           \
	{                                                                          \
		TJS_BEGIN_NATIVE_PROP_GETTER                                           \
		{                                                                      \
			TJS_GET_NATIVE_INSTANCE(_this, WaveInstance);                      \
			*result = (tjs_int)_this->field;                                    \
			return TJS_S_OK;                                                    \
		}                                                                      \
		TJS_END_NATIVE_PROP_GETTER                                             \
		TJS_BEGIN_NATIVE_PROP_SETTER                                           \
		{                                                                      \
			TJS_GET_NATIVE_INSTANCE(_this, WaveInstance);                      \
			_this->field = (tjs_int)*param;                                     \
			return TJS_S_OK;                                                    \
		}                                                                      \
		TJS_END_NATIVE_PROP_SETTER                                             \
	}                                                                          \
	TJS_END_NATIVE_PROP_DECL(name)

	WAVE_INT_PROP(volume, Volume)
	WAVE_INT_PROP(pan, Pan)
#undef WAVE_INT_PROP

	TJS_BEGIN_NATIVE_PROP_DECL(playing)
	{
		TJS_BEGIN_NATIVE_PROP_GETTER
		{
			TJS_GET_NATIVE_INSTANCE(_this, WaveInstance);
			*result = (tjs_int)(_this->Playing ? 1 : 0);
			return TJS_S_OK;
		}
		TJS_END_NATIVE_PROP_GETTER
		TJS_DENY_NATIVE_PROP_SETTER
	}
	TJS_END_NATIVE_PROP_DECL(playing)

	TJS_BEGIN_NATIVE_PROP_DECL(looping)
	{
		TJS_BEGIN_NATIVE_PROP_GETTER
		{
			TJS_GET_NATIVE_INSTANCE(_this, WaveInstance);
			*result = (tjs_int)(_this->Looping ? 1 : 0);
			return TJS_S_OK;
		}
		TJS_END_NATIVE_PROP_GETTER
		TJS_BEGIN_NATIVE_PROP_SETTER
		{
			TJS_GET_NATIVE_INSTANCE(_this, WaveInstance);
			_this->Looping = (tjs_int)*param != 0;
			return TJS_S_OK;
		}
		TJS_END_NATIVE_PROP_SETTER
	}
	TJS_END_NATIVE_PROP_DECL(looping)

	TJS_BEGIN_NATIVE_PROP_DECL(position)
	{
		TJS_BEGIN_NATIVE_PROP_GETTER
		{
			TJS_GET_NATIVE_INSTANCE(_this, WaveInstance);
			*result = (tjs_real)_this->Position;
			return TJS_S_OK;
		}
		TJS_END_NATIVE_PROP_GETTER
		TJS_DENY_NATIVE_PROP_SETTER
	}
	TJS_END_NATIVE_PROP_DECL(position)

	TJS_BEGIN_NATIVE_PROP_DECL(totalTime)
	{
		TJS_BEGIN_NATIVE_PROP_GETTER
		{
			TJS_GET_NATIVE_INSTANCE(_this, WaveInstance);
			*result = (tjs_real)_this->TotalTime;
			return TJS_S_OK;
		}
		TJS_END_NATIVE_PROP_GETTER
		TJS_DENY_NATIVE_PROP_SETTER
	}
	TJS_END_NATIVE_PROP_DECL(totalTime)

	TJS_END_NATIVE_MEMBERS
}

iTJSNativeInstance* tTJSNC_WaveSoundBuffer_Darwin::CreateNativeInstance()
{
	return new WaveInstance();
}

tTJSNativeClass* TVPCreateNativeClass_WaveSoundBuffer()
{
	return new tTJSNC_WaveSoundBuffer_Darwin();
}
