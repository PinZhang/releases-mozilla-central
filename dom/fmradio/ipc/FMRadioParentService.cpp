/* -*- Mode: c++; c-basic-offset: 2; indent-tabs-mode: nil; tab-width: 40 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "FMRadioParentService.h"
// FIXME Why not "ContentParent.h"?
#include "mozilla/dom/ContentParent.h"
#include "mozilla/Hal.h"
#include "nsIAudioManager.h"
#include "AudioManager.h"
#include "nsDOMClassInfo.h"
#include "mozilla/Hal.h"
#include "mozilla/Preferences.h"

#undef LOG
#if defined(MOZ_WIDGET_GONK)
#include <android/log.h>
#define LOG(args...)  __android_log_print(ANDROID_LOG_INFO, \
                                          "PFMRadioParentService", \
                                          ## args)
#else
#define LOG(args...)
#endif

using namespace mozilla::hal;
using mozilla::Preferences;

namespace mozilla {
namespace dom {
namespace fmradio {

FMRadioParentService* gFMRadioParentService;

FMRadioParentService::FMRadioParentService()
  : mFrequency(0)
{
  LOG("constructor");

  // read power state and frequency from Hal
  mEnabled = IsFMRadioOn();
  if (mEnabled)
  {
    mFrequency = GetFMRadioFrequency();
  }

  RegisterFMRadioObserver(this);
}

FMRadioParentService::~FMRadioParentService()
{
  LOG("destructor");

  UnregisterFMRadioObserver(this);
  gFMRadioParentService = nullptr;
}

void
FMRadioParentService::Enable(double aFrequency, ReplyRunnable* aRunnable)
{
  // We need to call EnableFMRadio() in main thread
  NS_ASSERTION(NS_IsMainThread(), "Wrong thread!");

  bool isEnabled = IsFMRadioOn();
  if (isEnabled) {
    aRunnable->SetReply(ErrorResponse());
    aRunnable->SetError("It's enabled");
    NS_DispatchToMainThread(aRunnable);
    return;
  }

  // TODO read from SettingsAPI
  FMRadioSettings info;
  info.upperLimit() = 108000;
  info.lowerLimit() = 87500;
  info.spaceType() = 100;

  EnableFMRadio(info);

  nsCOMPtr<nsIAudioManager> audioManager =
    do_GetService(NS_AUDIOMANAGER_CONTRACTID);
  NS_ASSERTION(audioManager, "No AudioManager");

  audioManager->SetFmRadioAudioEnabled(true);

  // TODO apply path of bug 862899: AudioChannelAgent per process

  aRunnable->SetReply(SuccessResponse());
  NS_DispatchToMainThread(aRunnable);
}

void
FMRadioParentService::Disable(ReplyRunnable* aRunnable)
{
  LOG("Check Main Thread");
  NS_ASSERTION(NS_IsMainThread(), "Wrong thread!");
  LOG("It's safe to disable FMRadio in main thread!");

  bool isEnabled = IsFMRadioOn();
  if (!isEnabled) {
    aRunnable->SetReply(ErrorResponse());
    aRunnable->SetError("It's disabled");
    NS_DispatchToMainThread(aRunnable);
    return;
  }

  // Fix Bug 796733.
  // DisableFMRadio should be called before SetFmRadioAudioEnabled to prevent
  // the annoying beep sound.
  DisableFMRadio();

  nsCOMPtr<nsIAudioManager> audioManager =
    do_GetService(NS_AUDIOMANAGER_CONTRACTID);
  NS_ASSERTION(audioManager, "No AudioManager");

  audioManager->SetFmRadioAudioEnabled(false);

  aRunnable->SetReply(SuccessResponse());
  NS_DispatchToMainThread(aRunnable);
}

void
FMRadioParentService::SetFrequency(double aFrequency, ReplyRunnable* aRunnable)
{
  LOG("Check Main Thread");
  NS_ASSERTION(NS_IsMainThread(), "Wrong thread!");
  LOG("It's safe to set frequency in main thread!");

  bool isEnabled = IsFMRadioOn();
  if (!isEnabled) {
    aRunnable->SetReply(ErrorResponse());
    aRunnable->SetError("It's disabled");
    NS_DispatchToMainThread(aRunnable);
    return;
  }

  SetFMRadioFrequency(aFrequency);

  aRunnable->SetReply(SuccessResponse());
  NS_DispatchToMainThread(aRunnable);
}

void
FMRadioParentService::Seek(bool upward, ReplyRunnable* aRunnable)
{

}

void
FMRadioParentService::CancelSeek(ReplyRunnable* aRunnable)
{

}

void
FMRadioParentService::Notify(const FMRadioOperationInformation& info)
{
  switch (info.operation())
  {
    case FM_RADIO_OPERATION_ENABLE:
      LOG("FM HW is enabled.");
      UpdatePowerState();
      break;
    case FM_RADIO_OPERATION_DISABLE:
      LOG("FM HW is disabled.");
      UpdatePowerState();
      break;
    case FM_RADIO_OPERATION_SEEK:
      LOG("FM HW seek complete.");
      UpdateFrequency();
      break;
    default:
      MOZ_NOT_REACHED();
      return;
  }
}

void
FMRadioParentService::UpdatePowerState()
{
  bool enabled = IsFMRadioOn();
  if (enabled != mEnabled)
  {
    mEnabled = enabled;
    // SendEnabled(enabled);
  }
}

void FMRadioParentService::UpdateFrequency()
{
  int32_t frequency = GetFMRadioFrequency();
  if (mFrequency != frequency)
  {
    mFrequency = frequency;
    // TODO round and keep decimal precise
    // SendFrequencyChanged(frequency / 1000.0);
  }
}

// static
FMRadioParentService*
FMRadioParentService::Get()
{
  MOZ_ASSERT(NS_IsMainThread());

  if (gFMRadioParentService) {
    LOG("Return cached gFMRadioParentService");
    return gFMRadioParentService;
  }

  // TODO release the object at some place
  gFMRadioParentService = new FMRadioParentService();

  return gFMRadioParentService;
}

} // namespace fmradio
} // namespace dom
} // namespace mozilla
