/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "FMRadioRequestParent.h"
#include "mozilla/unused.h"
// FIXME Why not "ContentParent.h"?
#include "mozilla/dom/ContentParent.h"
#include "mozilla/Hal.h"
#include "nsIAudioManager.h"
#include "AudioManager.h"
#include "nsDOMClassInfo.h"

#undef LOG
#if defined(MOZ_WIDGET_GONK)
#include <android/log.h>
#define LOG(args...)  __android_log_print(ANDROID_LOG_INFO, \
                                          "PFMRadioRequestParent", \
                                          ## args)
#else
#define LOG(args...)
#endif

using namespace mozilla::hal;

namespace mozilla {
namespace dom {
namespace fmradio {

FMRadioRequestParent::FMRadioRequestParent(const FMRadioRequestParams& aParams)
  : mParams(aParams)
{
  LOG("constructor");
  MOZ_COUNT_CTOR(FMRadioRequestParent);
}

void
FMRadioRequestParent::Dispatch()
{
  switch (mParams.type())
  {
    case FMRadioRequestParams::TFMRadioRequestEnableParams:
    {
      FMRadioRequestEnableParams params = mParams;
      nsRefPtr<nsRunnable> r = new EnableEvent(this, params);
      NS_DispatchToMainThread(r);
      break;
    }
    case FMRadioRequestParams::TFMRadioRequestDisableParams:
    {
      LOG("Call DisableEvent");
      nsRefPtr<nsRunnable> r = new DisableEvent(this);
      NS_DispatchToMainThread(r);
      break;
    }
    case FMRadioRequestParams::TFMRadioRequestSetFrequencyParams:
    {
      LOG("Call SetFrequency");
      FMRadioRequestSetFrequencyParams params = mParams;
      nsRefPtr<nsRunnable> r = new SetFrequencyEvent(this, params);
      NS_DispatchToMainThread(r);
      break;
    }
    case FMRadioRequestParams::TFMRadioRequestSeekParams:
      break;
    case FMRadioRequestParams::TFMRadioRequestCancelSeekParams:
      break;
    default:
      NS_RUNTIMEABORT("not reached");
      break;
  }
}

FMRadioRequestParent::~FMRadioRequestParent()
{
  LOG("destructor");
  MOZ_COUNT_DTOR(FMRadioRequestParent);
}

void
FMRadioRequestParent::ActorDestroy(ActorDestroyReason aWhy)
{
  return;
}

NS_IMPL_THREADSAFE_ADDREF(FMRadioRequestParent)
NS_IMPL_THREADSAFE_RELEASE(FMRadioRequestParent)

/***********************************
 *       PostErrorEvent            *
 ***********************************/
FMRadioRequestParent::PostErrorEvent::PostErrorEvent(
  FMRadioRequestParent* aParent, const char* aError)
  : CancelableRunnable(aParent)
{
  CopyASCIItoUTF16(aError, mError);
}

FMRadioRequestParent::PostErrorEvent::~PostErrorEvent() { }

nsresult
FMRadioRequestParent::PostErrorEvent::CancelableRun()
{
  NS_ASSERTION(NS_IsMainThread(), "Wrong thread!");

  ErrorResponse response(mError);
  unused << mParent->Send__delete__(mParent, response);
  return NS_OK;
}

/***********************************
 *       PostSuccessEvent          *
 ***********************************/
FMRadioRequestParent::PostSuccessEvent::PostSuccessEvent(
  FMRadioRequestParent* aParent)
  : CancelableRunnable(aParent) { }

FMRadioRequestParent::PostSuccessEvent::~PostSuccessEvent() { }

nsresult
FMRadioRequestParent::PostSuccessEvent::CancelableRun()
{
  NS_ASSERTION(NS_IsMainThread(), "Wrong thread!");

  SuccessResponse response;
  unused << mParent->Send__delete__(mParent, response);
  return NS_OK;
}

/*******************************
 *       EnableEvent           *
 *******************************/
FMRadioRequestParent::EnableEvent::EnableEvent(
  FMRadioRequestParent* aParent, FMRadioRequestEnableParams aParams)
  : CancelableRunnable(aParent, aParams) { }

FMRadioRequestParent::EnableEvent::~EnableEvent() {}

nsresult
FMRadioRequestParent::EnableEvent::CancelableRun()
{
  // We need to call EnableFMRadio() in main thread
  NS_ASSERTION(NS_IsMainThread(), "Wrong thread!");

  bool isEnabled = IsFMRadioOn();
  if (isEnabled) {
    nsRefPtr<nsRunnable> event = new PostErrorEvent(mParent, "It's enabled");
    NS_DispatchToMainThread(event);
    return NS_OK;
  }

  // TODO read from SettingsAPI
  FMRadioSettings info;
  info.upperLimit() = 108000;
  info.lowerLimit() = 87500;
  info.spaceType() = 100;

  EnableFMRadio(info);

  nsCOMPtr<nsIAudioManager> audioManager =
    do_GetService(NS_AUDIOMANAGER_CONTRACTID);
  NS_ENSURE_TRUE(audioManager, NS_OK);

  audioManager->SetFmRadioAudioEnabled(true);

  // TODO apply path of bug 862899: AudioChannelAgent per process

  nsRefPtr<nsRunnable> event = new PostSuccessEvent(mParent);
  NS_DispatchToMainThread(event);

  return NS_OK;
}

/***********************************
 *           DisableEvent          *
 ***********************************/
FMRadioRequestParent::DisableEvent::DisableEvent(FMRadioRequestParent* aParent)
  : CancelableRunnable(aParent) { }

FMRadioRequestParent::DisableEvent::~DisableEvent() { }

nsresult
FMRadioRequestParent::DisableEvent::CancelableRun()
{
  LOG("Check Main Thread");
  NS_ASSERTION(NS_IsMainThread(), "Wrong thread!");
  LOG("It's safe to disable FMRadio in main thread!");

  bool isEnabled = IsFMRadioOn();
  if (!isEnabled) {
    nsRefPtr<nsRunnable> event = new PostErrorEvent(mParent, "It's disabled");
    NS_DispatchToMainThread(event);
    return NS_OK;
  }

  // Fix Bug 796733. 
  // DisableFMRadio should be called before SetFmRadioAudioEnabled to prevent
  // the annoying beep sound.
  DisableFMRadio();

  nsCOMPtr<nsIAudioManager> audioManager =
    do_GetService(NS_AUDIOMANAGER_CONTRACTID);
  NS_ENSURE_TRUE(audioManager, NS_OK);

  audioManager->SetFmRadioAudioEnabled(false);

  nsRefPtr<nsRunnable> event = new PostSuccessEvent(mParent);
  NS_DispatchToMainThread(event);

  return NS_OK;
}

/***********************************
 *        SetFrequencyEvent        *
 ***********************************/
FMRadioRequestParent::SetFrequencyEvent::SetFrequencyEvent(
  FMRadioRequestParent* aParent, FMRadioRequestSetFrequencyParams aParams)
  : CancelableRunnable(aParent, aParams) { }

FMRadioRequestParent::SetFrequencyEvent::~SetFrequencyEvent() { }

nsresult
FMRadioRequestParent::SetFrequencyEvent::CancelableRun()
{
  LOG("Check Main Thread");
  NS_ASSERTION(NS_IsMainThread(), "Wrong thread!");
  LOG("It's safe to set frequency in main thread!");

  bool isEnabled = IsFMRadioOn();
  if (!isEnabled) {
    nsRefPtr<nsRunnable> event = new PostErrorEvent(mParent, "It's disabled");
    NS_DispatchToMainThread(event);
    return NS_OK;
  }

  FMRadioRequestSetFrequencyParams params = mParams;
  SetFMRadioFrequency(params.frequency());

  nsRefPtr<nsRunnable> event = new PostSuccessEvent(mParent);
  NS_DispatchToMainThread(event);

  return NS_OK;
}

} // namespace fmradio
} // namespace dom
} // namespace mozilla

