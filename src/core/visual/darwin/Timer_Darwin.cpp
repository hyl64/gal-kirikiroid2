/*
 * This file is part of the macOS port of Kirikiroid2 (zeas2/Kirikiroid2).
 *
 * Timer_Darwin.cpp - the script-visible timer
 *
 * The Windows build's timer is a window timer: it owns a WM_TIMER and fires an
 * onTimer event on the message pump.  This back end has no pump of that shape, so
 * the class here keeps what a script sets - interval, enabled, mode, capacity, the
 * handler - and does not fire on its own.  A framework can create, configure and
 * destroy timers, which is what KAG does while it is starting up; wiring them to
 * real time comes when the main loop exists.
 */

#include "tjsCommHead.h"
#include "tjsNative.h"
#include "tjsVariant.h"

namespace
{
	class TimerInstance : public tTJSNativeInstance
	{
	public:
		tjs_int Interval = 1000;
		bool Enabled = false;
		tjs_int Mode = 0;
		tjs_int Capacity = 0;
		tTJSVariant* Handler = nullptr;

		~TimerInstance() override
		{
			delete Handler;
		}

		tjs_error TJS_INTF_METHOD Construct(tjs_int numparams, tTJSVariant** param,
			iTJSDispatch2* tjs_obj) override
		{
			(void)tjs_obj;
			// Timer(owner, handler) - the second argument is the event handler
			if(numparams >= 2 && param[1]->Type() != tvtVoid)
			{
				Handler = new tTJSVariant(*param[1]);
			}
			return TJS_S_OK;
		}

		void TJS_INTF_METHOD Invalidate() override
		{
		}
	};
}

class tTJSNC_Timer_Darwin : public tTJSNativeClass
{
	typedef tTJSNativeClass inherited;

public:
	tTJSNC_Timer_Darwin();
	static tjs_uint32 ClassID;

private:
	iTJSNativeInstance* CreateNativeInstance() override;
};

tjs_uint32 tTJSNC_Timer_Darwin::ClassID = (tjs_uint32)-1;

tTJSNC_Timer_Darwin::tTJSNC_Timer_Darwin()
	: inherited(TJS_W("Timer"))
{
	TJS_BEGIN_NATIVE_MEMBERS(Timer)
	TJS_DECL_EMPTY_FINALIZE_METHOD

	TJS_BEGIN_NATIVE_CONSTRUCTOR_DECL(/*var.name*/ _this,
		/*var.type*/ TimerInstance, /*TJS class name*/ Timer)
	{
		return TJS_S_OK;
	}
	TJS_END_NATIVE_CONSTRUCTOR_DECL(/*TJS class name*/ Timer)

	TJS_BEGIN_NATIVE_METHOD_DECL(/*func. name*/ start)
	{
		TJS_GET_NATIVE_INSTANCE(_this, TimerInstance);
		_this->Enabled = true;
		return TJS_S_OK;
	}
	TJS_END_NATIVE_METHOD_DECL(/*func. name*/ start)

	TJS_BEGIN_NATIVE_METHOD_DECL(/*func. name*/ stop)
	{
		TJS_GET_NATIVE_INSTANCE(_this, TimerInstance);
		_this->Enabled = false;
		return TJS_S_OK;
	}
	TJS_END_NATIVE_METHOD_DECL(/*func. name*/ stop)

#define TIMER_INT_PROP(name, field)                                            \
	TJS_BEGIN_NATIVE_PROP_DECL(name)                                           \
	{                                                                          \
		TJS_BEGIN_NATIVE_PROP_GETTER                                           \
		{                                                                      \
			TJS_GET_NATIVE_INSTANCE(_this, TimerInstance);                      \
			*result = (tjs_int)_this->field;                                    \
			return TJS_S_OK;                                                    \
		}                                                                      \
		TJS_END_NATIVE_PROP_GETTER                                             \
		TJS_BEGIN_NATIVE_PROP_SETTER                                           \
		{                                                                      \
			TJS_GET_NATIVE_INSTANCE(_this, TimerInstance);                      \
			_this->field = (tjs_int)*param;                                     \
			return TJS_S_OK;                                                    \
		}                                                                      \
		TJS_END_NATIVE_PROP_SETTER                                             \
	}                                                                          \
	TJS_END_NATIVE_PROP_DECL(name)

	TIMER_INT_PROP(interval, Interval)
	TIMER_INT_PROP(mode, Mode)
	TIMER_INT_PROP(capacity, Capacity)
#undef TIMER_INT_PROP

	TJS_BEGIN_NATIVE_PROP_DECL(enabled)
	{
		TJS_BEGIN_NATIVE_PROP_GETTER
		{
			TJS_GET_NATIVE_INSTANCE(_this, TimerInstance);
			*result = (tjs_int)(_this->Enabled ? 1 : 0);
			return TJS_S_OK;
		}
		TJS_END_NATIVE_PROP_GETTER
		TJS_BEGIN_NATIVE_PROP_SETTER
		{
			TJS_GET_NATIVE_INSTANCE(_this, TimerInstance);
			_this->Enabled = (tjs_int)*param != 0;
			return TJS_S_OK;
		}
		TJS_END_NATIVE_PROP_SETTER
	}
	TJS_END_NATIVE_PROP_DECL(enabled)

	TJS_BEGIN_NATIVE_PROP_DECL(onTimer)
	{
		TJS_BEGIN_NATIVE_PROP_GETTER
		{
			TJS_GET_NATIVE_INSTANCE(_this, TimerInstance);
			if(_this->Handler != nullptr)
				*result = *_this->Handler;
			return TJS_S_OK;
		}
		TJS_END_NATIVE_PROP_GETTER
		TJS_BEGIN_NATIVE_PROP_SETTER
		{
			TJS_GET_NATIVE_INSTANCE(_this, TimerInstance);
			if(_this->Handler == nullptr)
				_this->Handler = new tTJSVariant();
			*_this->Handler = *param;
			return TJS_S_OK;
		}
		TJS_END_NATIVE_PROP_SETTER
	}
	TJS_END_NATIVE_PROP_DECL(onTimer)

	TJS_END_NATIVE_MEMBERS
}

iTJSNativeInstance* tTJSNC_Timer_Darwin::CreateNativeInstance()
{
	return new TimerInstance();
}

tTJSNativeClass* TVPCreateNativeClass_Timer()
{
	return new tTJSNC_Timer_Darwin();
}
