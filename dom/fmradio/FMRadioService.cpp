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
#define LOG(args...) FM_LOG("FMRadioService", args)

using namespace mozilla::hal;
using mozilla::Preferences;

BEGIN_FMRADIO_NAMESPACE

nsRefPtr<FMRadioService>
FMRadioService::sFMRadioService;

FMRadioEventObserverList*
FMRadioService::sObserverList;

FMRadioService::FMRadioService()
  : mFrequencyInKHz(0)
  , mDisabling(false)
  , mEnabling(false)
  , mDisableRequest(nullptr)
  , mEnableRequest(nullptr)
{
  LOG("constructor");

  // read power state and frequency from Hal
  mEnabled = IsFMRadioOn();
  if (mEnabled)
  {
    mFrequencyInKHz = GetFMRadioFrequency();
  }

  RegisterFMRadioObserver(this);
}

FMRadioService::~FMRadioService()
{
  LOG("destructor");

  NS_RELEASE(sFMRadioService);
  sFMRadioService = nullptr;

  delete sObserverList;
  sObserverList = nullptr;
}

/**
 * Round the frequency to match the range of frequency and the channel width.
 * If the given frequency is out of range, return 0.
 * For example:
 *  - lower: 87.5MHz, upper: 108MHz, channel width: 0.2MHz
 *    87600 is rounded to 87700
 *    87580 is rounded to 87500
 *    109000 is not rounded, null will be returned
 */
int32_t
RoundFrequncy(int32_t aFrequencyInKHz)
{
  // TODO read from settings
  int32_t upper = 108000;
  int32_t lower = 87500;
  int32_t channelWidth = 100;

  if (aFrequencyInKHz < lower ||
      aFrequencyInKHz > upper) {
    return 0;
  }

  int32_t partToBeRounded = aFrequencyInKHz - lower;
  int32_t roundedPart = round(partToBeRounded / channelWidth) * channelWidth;

  return lower + roundedPart;
}

class EnableRunnable : public nsRunnable
{
public:
  EnableRunnable() { }
  virtual ~EnableRunnable() { }

  NS_IMETHOD Run()
  {
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
    return NS_OK;
  }
};

class DisableRunnable : public nsRunnable
{
public:
  DisableRunnable() { }
  virtual ~DisableRunnable() { }

  NS_IMETHOD Run()
  {
    // Fix Bug 796733.
    // DisableFMRadio should be called before SetFmRadioAudioEnabled to prevent
    // the annoying beep sound.
    DisableFMRadio();

    nsCOMPtr<nsIAudioManager> audioManager =
      do_GetService(NS_AUDIOMANAGER_CONTRACTID);
    NS_ASSERTION(audioManager, "No AudioManager");

    audioManager->SetFmRadioAudioEnabled(false);

    return NS_OK;
  }
};

class SetFrequencyRunnable : public nsRunnable
{
public:
  SetFrequencyRunnable(FMRadioService* aService, int32_t aFrequency)
    : mService(aService)
    , mFrequency(aFrequency) { }
  virtual ~SetFrequencyRunnable() { }

  NS_IMETHOD Run()
  {
    SetFMRadioFrequency(mFrequency);
    mService->UpdateFrequency();
    return NS_OK;
  }

private:
  nsRefPtr<FMRadioService> mService;
  int32_t mFrequency;
};

void
FMRadioService::RegisterHandler(FMRadioEventObserver* aHandler)
{
  LOG("Register handler");
  sObserverList->AddObserver(aHandler);
}

void
FMRadioService::UnregisterHandler(FMRadioEventObserver* aHandler)
{
  LOG("Unregister handler");
  sObserverList->RemoveObserver(aHandler);

  if (sObserverList->Length() == 0)
  {
    LOG("No observer in the list, destroy myself");
    delete this;
  }
}

void
FMRadioService::Enable(double aFrequencyInMHz, ReplyRunnable* aRunnable)
{
  // We need to call EnableFMRadio() in main thread
  NS_ASSERTION(NS_IsMainThread(), "Wrong thread!");

  bool isEnabled = IsFMRadioOn();
  if (isEnabled)
  {
    LOG("It's enabled");
    aRunnable->SetReply(ErrorResponse(NS_ConvertASCIItoUTF16("It's enabled")));
    NS_DispatchToMainThread(aRunnable);
    return;
  }

  // If we call Disable() immediately after Enable() is called, we will wait for
  // the enabled-event from HAL, and apply disable action after that, so we need
  // to check `mDisabling` before checking `mEnabling`.
  if (mDisabling)
  {
    LOG("It's disabling");
    aRunnable->SetReply(ErrorResponse(
      NS_ConvertASCIItoUTF16("It's disabling")));
    NS_DispatchToMainThread(aRunnable);
    return;
  }

  if (mEnabling)
  {
    LOG("It's enabling");
    aRunnable->SetReply(ErrorResponse(
      NS_ConvertASCIItoUTF16("It's enabling")));
    NS_DispatchToMainThread(aRunnable);
    return;
  }

  int32_t roundedFrequency = RoundFrequncy(aFrequencyInMHz * 1000);
  if (!roundedFrequency)
  {
    LOG("Frequency is out of range");
    aRunnable->SetReply(ErrorResponse(
      NS_ConvertASCIItoUTF16("Frequency is out of range")));
    NS_DispatchToMainThread(aRunnable);
    return;
  }

  mEnabling = true;
  // Cache the enable request just in case disable() is called
  // while the FM radio HW is being enabled.
  mEnableRequest = aRunnable;

  // Cache the frequency value, and set it after the FM HW is enabled
  mFrequencyInKHz = roundedFrequency;

  NS_DispatchToMainThread(new EnableRunnable());
}

void
FMRadioService::Disable(ReplyRunnable* aRunnable)
{
  LOG("Check Main Thread");
  NS_ASSERTION(NS_IsMainThread(), "Wrong thread!");

  if (mDisabling)
  {
    LOG("It's disabling");
    aRunnable->SetReply(ErrorResponse(NS_ConvertASCIItoUTF16("It's disabling")));
    NS_DispatchToMainThread(aRunnable);
    return;
  }

  if (!mEnabling && !IsFMRadioOn()) {
    LOG("It's disabled");
    aRunnable->SetReply(ErrorResponse(NS_ConvertASCIItoUTF16("It's disabled")));
    NS_DispatchToMainThread(aRunnable);
    return;
  }

  mDisabling = true;
  // If the radio is currently enabling, we send an enable-failed message
  // immediately. When the radio finishes enabling, we'll send the
  // disable-succeeded message.
  mDisableRequest = aRunnable;

  if (mEnabling)
  {
    LOG("It's enabling, fail it immediately.");
    mEnableRequest->SetReply(ErrorResponse(NS_ConvertASCIItoUTF16("It's canceled")));
    NS_DispatchToMainThread(mEnableRequest);
    mEnableRequest = nullptr;
    return;
  }

  DoDisable();
}

void
FMRadioService::DoDisable()
{
  NS_DispatchToMainThread(new DisableRunnable());
}

void
FMRadioService::SetFrequency(double aFrequencyInMHz, ReplyRunnable* aRunnable)
{
  LOG("Check Main Thread");
  NS_ASSERTION(NS_IsMainThread(), "Wrong thread!");

  // Because IsFMRadioOn() should be false when it's being enabled, we don't
  // need to check `mEnabling`.
  if (!IsFMRadioOn()) {
    aRunnable->SetReply(ErrorResponse(
      NS_ConvertASCIItoUTF16("It's disabled")));
    NS_DispatchToMainThread(aRunnable);
    return;
  }

  if (mDisabling)
  {
    aRunnable->SetReply(ErrorResponse(
      NS_ConvertASCIItoUTF16("It's disabling")));
    NS_DispatchToMainThread(aRunnable);
    return;
  }

  int32_t roundedFrequency = RoundFrequncy(aFrequencyInMHz * 1000);
  if (!roundedFrequency)
  {
    aRunnable->SetReply(ErrorResponse(
      NS_ConvertASCIItoUTF16("Frequency is out of range")));
    NS_DispatchToMainThread(aRunnable);
    return;
  }

  aRunnable->SetReply(SuccessResponse());
  NS_DispatchToMainThread(aRunnable);

  NS_DispatchToMainThread(new SetFrequencyRunnable(this, roundedFrequency));
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
FMRadioService::DistributeEvent(const FMRadioEventType& aType)
{
  sObserverList->Broadcast(aType);
}

void
FMRadioService::Notify(const FMRadioOperationInformation& info)
{
  switch (info.operation())
  {
    case FM_RADIO_OPERATION_ENABLE:
    {
      LOG("FM HW is enabled.");

      // The signal we received might be triggered by other thread/process, we
      // should check if `mEnabling` is true, if false, we should skip it.
      if (!mEnabling)
      {
        LOG("Not triggered by current thread, skip it.");
        return;
      }

      mEnabling = false;

      // If we're disabling, go disable the radio now.
      // We don't release `mEnableRequest` here, because we have release it
      // when Disable() is called.
      if (mDisabling)
      {
        LOG("We need to disable FM Radio for the waiting request.");
        DoDisable();
        return;
      }

      LOG("Fire Success for enable request.");
      mEnableRequest->SetReply(SuccessResponse());
      NS_DispatchToMainThread(mEnableRequest);
      mEnableRequest = nullptr;

      // To make sure the FM app will get right frequency after the FM
      // radio is enabled, we have to set the frequency first.
      SetFMRadioFrequency(mFrequencyInKHz);

      // Update the current frequency without sending 'frequencyChange'
      // msg, to make sure the FM app will get the right frequency when the
      // 'enabled' event is fired.
      mFrequencyInKHz = GetFMRadioFrequency();
      UpdatePowerState();

      // The frequency is changed from '0' to some number, so we should
      // send the 'frequencyChange' message manually.
      double frequencyInMHz = mFrequencyInKHz / 1000.0;
      DistributeEvent(FrequencyChangedEvent(frequencyInMHz));

      break;
    }
    case FM_RADIO_OPERATION_DISABLE:
    {
      LOG("FM HW is disabled.");

      // The signal we received might be triggered by other thread/process, we
      // should check if `mDisabling` is true, if false, we should skip it.
      if (!mDisabling)
      {
        LOG("Not triggered by current thread, skip it.");
        return;
      }

      mDisabling = false;

      mDisableRequest->SetReply(SuccessResponse());
      NS_DispatchToMainThread(mDisableRequest);
      mDisableRequest = nullptr;

      UpdatePowerState();

      // If the FM Radio is currently seeking, no fail-to-seek or similar
      // event will be fired, execute the seek callback manually.
      break;
    }
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
    double frequencyInMHz = mFrequencyInKHz / 1000.0;
    // To make sure the FM app will get the right frequency when the
    // 'enabled' event is fired.
    DistributeEvent(StateChangedEvent(enabled, frequencyInMHz));
  }
}

void
FMRadioService::UpdateFrequency()
{
  int32_t frequency = GetFMRadioFrequency();
  if (mFrequencyInKHz != frequency)
  {
    mFrequencyInKHz = frequency;
    double frequencyInMHz = mFrequencyInKHz / 1000.0;
    DistributeEvent(FrequencyChangedEvent(frequencyInMHz));
  }
}

bool
IsMainProcess()
{
  return XRE_GetProcessType() == GeckoProcessType_Default;
}

NS_IMPL_THREADSAFE_ADDREF(IFMRadioService)
NS_IMPL_THREADSAFE_RELEASE(IFMRadioService)

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

  if (sFMRadioService) {
    LOG("Return cached gFMRadioService");
    return sFMRadioService;
  }

  // TODO release the object at some place
  sFMRadioService = new FMRadioService();

  sObserverList = new FMRadioEventObserverList();

  return sFMRadioService;
}

END_FMRADIO_NAMESPACE
