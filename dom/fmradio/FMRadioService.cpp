/* -*- Mode: c++; c-basic-offset: 2; indent-tabs-mode: nil; tab-width: 40 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "FMRadioService.h"
// FIXME Why not "ContentParent.h"?
#include "mozilla/dom/ContentParent.h"
#include "mozilla/Hal.h"
#include "nsIAudioManager.h"
#include "AudioManager.h"
#include "nsDOMClassInfo.h"
#include "mozilla/Hal.h"
#include "mozilla/Preferences.h"
#include "mozilla/dom/fmradio/FMRadioChildService.h"

#undef LOG
#define LOG(args...) FM_LOG("PFMRadioService", args)

using namespace mozilla::hal;
using mozilla::Preferences;

BEGIN_FMRADIO_NAMESPACE

// TODO release static object
FMRadioService* gFMRadioService;
FMRadioEventObserverList* gEventObserverList;

FMRadioService::FMRadioService()
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

FMRadioService::~FMRadioService()
{
  LOG("destructor");

  gFMRadioService = nullptr;
  gEventObserverList = nullptr;
}

void
FMRadioService::RegisterHandler(FMRadioEventObserver* aHandler)
{
  LOG("Register handler");
  gEventObserverList->AddObserver(aHandler);
}

void
FMRadioService::UnregisterHandler(FMRadioEventObserver* aHandler)
{
  LOG("Unregister handler");
  gEventObserverList->RemoveObserver(aHandler);

  if (gEventObserverList->Length() == 0)
  {
    LOG("No observer in the list, destroy myself");
    delete this;
  }
}


void
FMRadioService::Enable(double aFrequency, ReplyRunnable* aRunnable)
{
  // We need to call EnableFMRadio() in main thread
  NS_ASSERTION(NS_IsMainThread(), "Wrong thread!");

  bool isEnabled = IsFMRadioOn();
  if (isEnabled) {
    nsString error;
    aRunnable->SetReply(ErrorResponse(NS_ConvertASCIItoUTF16("It's enabled")));
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
FMRadioService::Disable(ReplyRunnable* aRunnable)
{
  LOG("Check Main Thread");
  NS_ASSERTION(NS_IsMainThread(), "Wrong thread!");
  LOG("It's safe to disable FMRadio in main thread!");

  bool isEnabled = IsFMRadioOn();
  if (!isEnabled) {
    nsString error;
    aRunnable->SetReply(ErrorResponse(NS_ConvertASCIItoUTF16("It's disabled")));
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
FMRadioService::SetFrequency(double aFrequency, ReplyRunnable* aRunnable)
{
  LOG("Check Main Thread");
  NS_ASSERTION(NS_IsMainThread(), "Wrong thread!");
  LOG("It's safe to set frequency in main thread!");

  bool isEnabled = IsFMRadioOn();
  if (!isEnabled) {
    nsString error;
    aRunnable->SetReply(ErrorResponse(NS_ConvertASCIItoUTF16("It's disabled")));
    NS_DispatchToMainThread(aRunnable);
    return;
  }

  SetFMRadioFrequency(aFrequency);

  aRunnable->SetReply(SuccessResponse());
  NS_DispatchToMainThread(aRunnable);
}

void
FMRadioService::Seek(bool upward, ReplyRunnable* aRunnable)
{

}

void
FMRadioService::CancelSeek(ReplyRunnable* aRunnable)
{

}

void
FMRadioService::Notify(const FMRadioOperationInformation& info)
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
FMRadioService::UpdatePowerState()
{
  bool enabled = IsFMRadioOn();
  if (enabled != mEnabled)
  {
    mEnabled = enabled;
    gEventObserverList->Broadcast(StateChangedEvent(enabled));
  }
}

void
FMRadioService::UpdateFrequency()
{
  int32_t frequency = GetFMRadioFrequency();
  if (mFrequency != frequency)
  {
    mFrequency = frequency;
    // TODO round and keep decimal precise
    gEventObserverList->Broadcast(FrequencyChangedEvent(frequency));
  }
}

void
FMRadioService::DistributeEvent(const FMRadioEventType& aType)
{
  gEventObserverList->Broadcast(aType);
}

bool
IsMainProcess()
{
  return XRE_GetProcessType() == GeckoProcessType_Default;
}

// static
IFMRadioService*
FMRadioService::Get()
{
  MOZ_ASSERT(NS_IsMainThread());

  if (!IsMainProcess())
  {
    LOG("Return FMRadioChildService for OOP");
    return FMRadioChildService::Get();
  }

  if (gFMRadioService) {
    LOG("Return cached gFMRadioService");
    return gFMRadioService;
  }

  // TODO release the object at some place
  gFMRadioService = new FMRadioService();

  gEventObserverList = new FMRadioEventObserverList();

  return gFMRadioService;
}

END_FMRADIO_NAMESPACE
