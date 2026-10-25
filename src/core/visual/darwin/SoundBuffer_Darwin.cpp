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

#include <SDL2/SDL_mixer.h>

#include <cmath>
#include <cstring>
#include <vector>

namespace
{
	/* One mixer for the whole process, opened on first use.  KiriKiri's wave
	 * buffers are background music and short effects, which is what SDL_mixer
	 * does; the engine's own volume is -10000..0 dB, and 0 here means silent. */
	bool EnsureMixer()
	{
		static bool opened = false;
		static bool ok = false;
		if(!opened)
		{
			opened = true;
			ok = Mix_OpenAudio(44100, MIX_DEFAULT_FORMAT, 2, 2048) == 0;
			if(ok)
				Mix_AllocateChannels(8);
		}
		return ok;
	}

	int VolumeToMixer(tjs_int db)
	{
		// -10000 dB is silence and 0 dB is full
		if(db <= -10000) return 0;
		if(db >= 0) return MIX_MAX_VOLUME;
		const double linear = std::pow(10.0, db / 2000.0);
		return (int)(linear * MIX_MAX_VOLUME);
	}
}

namespace
{
	class WaveInstance : public tTJSNativeInstance
	{
	public:
		std::string Storage;
		tjs_int Volume = 0;      // -10000 .. 0 dB, as the engine uses
		tjs_int Pan = 0;
		bool Playing = false;
		bool Paused = false;
		bool Looping = false;
		bool Opened = false;
		double Position = 0.0;
		double TotalTime = 0.0;
		Mix_Music* Music = nullptr;
		SDL_RWops* Rw = nullptr;

		~WaveInstance() override
		{
			if(Music != nullptr)
				Mix_FreeMusic(Music);
			if(Rw != nullptr)
				SDL_FreeRW(Rw);
		}
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

			if(EnsureMixer())
			{
				// the file comes through the engine's storage, so an archive works
				// as well as a folder
				tTJSBinaryStream* stream = TVPCreateStream(param[0]->GetString());
				if(stream != nullptr)
				{
					std::vector<char> bytes;
					char buffer[0x2000];
					tjs_uint read;
					while((read = stream->Read(buffer, sizeof buffer)) > 0)
						bytes.insert(bytes.end(), buffer, buffer + read);
					delete stream;

					if(!bytes.empty())
					{
						_this->Rw = SDL_RWFromConstMem(bytes.data(), (int)bytes.size());
						// SDL_RWFromConstMem points into `bytes`, which dies here, so
						// hand SDL a copy of its own instead
						if(_this->Rw != nullptr)
						{
							void* copy = malloc(bytes.size());
							memcpy(copy, bytes.data(), bytes.size());
							SDL_FreeRW(_this->Rw);
							_this->Rw = SDL_RWFromMem(copy, (int)bytes.size());
							_this->Music = Mix_LoadMUS_RW(_this->Rw, 0);
							if(_this->Music == nullptr)
							{
								SDL_FreeRW(_this->Rw);
								_this->Rw = nullptr;
								free(copy);
							}
							else
							{
								_this->TotalTime = (double)Mix_MusicDuration(_this->Music);
							}
						}
					}
				}
			}
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

	WAVE_SIMPLE(/*func. name*/ play,
		_this->Playing = true;
		if(_this->Music != nullptr)
		{
			const int loops = _this->Looping ? -1 : 0;
			if(Mix_PlayMusic(_this->Music, loops) != 0)
				_this->Playing = false;
		})
	WAVE_SIMPLE(/*func. name*/ stop,
		_this->Playing = false;
		if(_this->Music != nullptr) Mix_HaltMusic();)
	WAVE_SIMPLE(/*func. name*/ pause,
		_this->Paused = true;
		if(_this->Music != nullptr) Mix_PauseMusic();)
	WAVE_SIMPLE(/*func. name*/ fade, (void)numparams; (void)param;)
	/* a fade that is cancelled: with no mixer of its own this back end stops the
	 * sound at once, which is what the caller is asking to happen */
	WAVE_SIMPLE(/*func. name*/ stopFade, _this->Playing = false;)
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

	TJS_BEGIN_NATIVE_PROP_DECL(volume)
	{
		TJS_BEGIN_NATIVE_PROP_GETTER
		{
			TJS_GET_NATIVE_INSTANCE(_this, WaveInstance);
			*result = (tjs_int)_this->Volume;
			return TJS_S_OK;
		}
		TJS_END_NATIVE_PROP_GETTER
		TJS_BEGIN_NATIVE_PROP_SETTER
		{
			TJS_GET_NATIVE_INSTANCE(_this, WaveInstance);
			_this->Volume = (tjs_int)*param;
			Mix_VolumeMusic(VolumeToMixer(_this->Volume));
			return TJS_S_OK;
		}
		TJS_END_NATIVE_PROP_SETTER
	}
	TJS_END_NATIVE_PROP_DECL(volume)
	WAVE_INT_PROP(pan, Pan)
#undef WAVE_INT_PROP

	/* 0 stopped, 1 playing, 2 paused - the three states a script asks about */
	TJS_BEGIN_NATIVE_PROP_DECL(status)
	{
		TJS_BEGIN_NATIVE_PROP_GETTER
		{
			TJS_GET_NATIVE_INSTANCE(_this, WaveInstance);
			*result = _this->Playing ? (tjs_int)(_this->Paused ? 2 : 1) : 0;
			return TJS_S_OK;
		}
		TJS_END_NATIVE_PROP_GETTER
		TJS_BEGIN_NATIVE_PROP_SETTER
		{
			return TJS_E_NOTIMPL;
		}
		TJS_END_NATIVE_PROP_SETTER
	}
	TJS_END_NATIVE_PROP_DECL(status)

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

	TJS_BEGIN_NATIVE_PROP_DECL(paused)
	{
		TJS_BEGIN_NATIVE_PROP_GETTER
		{
			TJS_GET_NATIVE_INSTANCE(_this, WaveInstance);
			*result = (tjs_int)(_this->Paused ? 1 : 0);
			return TJS_S_OK;
		}
		TJS_END_NATIVE_PROP_GETTER
		TJS_BEGIN_NATIVE_PROP_SETTER
		{
			TJS_GET_NATIVE_INSTANCE(_this, WaveInstance);
			_this->Paused = (tjs_int)*param != 0;
			return TJS_S_OK;
		}
		TJS_END_NATIVE_PROP_SETTER
	}
	TJS_END_NATIVE_PROP_DECL(paused)

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
