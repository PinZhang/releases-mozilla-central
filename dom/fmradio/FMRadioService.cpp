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

#define BAND_87500_108000_kHz 1
#define BAND_76000_108000_kHz 2
#define BAND_76000_90000_kHz  3

#define CHANNEL_WIDTH_200KHZ 200
#define CHANNEL_WIDTH_100KHZ 100
#define CHANNEL_WIDTH_50KHZ  50

#define DOM_FMRADIO_BAND_PREF "dom.fmradio.band"
#define DOM_FMRADIO_CHANNEL_WIDTH_PREF "dom.fmradio.channelWidth"

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
  , mSeeking(false)
  , mDisableRequest(nullptr)
  , mEnableRequest(nullptr)
  , mSeekRequest(nullptr)
{
  LOG("constructor");

  // read power state and frequency from Hal
  mEnabled = IsFMRadioOn();
  if (mEnabled)
  {
    mFrequencyInKHz = GetFMRadioFrequency();
  }

  switch (Preferences::GetInt(DOM_FMRADIO_BAND_PREF, BAND_87500_108000_kHz))
  {
    case BAND_76000_90000_kHz:
      mUpperBoundInMHz = 90.0;
      mLowerBoundInMHz = 76.0;
      break;
    case BAND_76000_108000_kHz:
      mUpperBoundInMHz = 108.0;
      mLowerBoundInMHz = 76.0;
      break;
    case BAND_87500_108000_kHz:
    default:
      mUpperBoundInMHz = 108.0;
      mLowerBoundInMHz = 87.5;
      break;
  }

  switch (Preferences::GetInt(DOM_FMRADIO_CHANNEL_WIDTH_PREF,
    CHANNEL_WIDTH_100KHZ))
  {
    case CHANNEL_WIDTH_200KHZ:
      mChannelWidthInMHz = 0.2;
      break;
    case CHANNEL_WIDTH_50KHZ:
      mChannelWidthInMHz = 0.05;
      break;
    case CHANNEL_WIDTH_100KHZ:
    default:
      mChannelWidthInMHz = 0.1;
      break;
  }

  RegisterFMRadioObserver(this);
}

FMRadioService::~FMRadioService()
{
  LOG("destructor");
  delete sObserverList;
  sObserverList = nullptr;
  UnregisterFMRadioObserver(this);
}


int32_t
FMRadioService::RoundFrequency(int32_t aFrequencyInKHz)
{
  int32_t lower = mLowerBoundInMHz * 1000;
  int32_t upper = mUpperBoundInMHz * 1000;
  int32_t channelWidth = mChannelWidthInMHz * 1000;

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
  EnableRunnable(int32_t aUpperLimit, int32_t aLowerLimit, int32_t aSpaceType)
    : mUpperLimit(aUpperLimit)
    , mLowerLimit(aLowerLimit)
    , mSpaceType(aSpaceType) { }

  virtual ~EnableRunnable() { }

  NS_IMETHOD Run()
  {
    FMRadioSettings info;
    info.upperLimit() = mUpperLimit;
    info.lowerLimit() = mLowerLimit;
    info.spaceType() = mSpaceType;

    EnableFMRadio(info);

    nsCOMPtr<nsIAudioManager> audioManager =
      do_GetService(NS_AUDIOMANAGER_CONTRACTID);
    NS_ASSERTION(audioManager, "No AudioManager");

    audioManager->SetFmRadioAudioEnabled(true);

    // TODO apply path of bug 862899: AudioChannelAgent per process
    return NS_OK;
  }
private:
  int32_t mUpperLimit;
  int32_t mLowerLimit;
  int32_t mSpaceType;
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

class SeekRunnable : public nsRunnable
{
public:
  SeekRunnable(bool aUpward) : mUpward(aUpward) { }
  virtual ~SeekRunnable() { }

  NS_IMETHOD Run()
  {
    FMRadioSeek(mUpward ? FM_RADIO_SEEK_DIRECTION_UP
                        : FM_RADIO_SEEK_DIRECTION_DOWN);
    return NS_OK;
  }

private:
  bool mUpward;
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
    sFMRadioService = nullptr;

    // No observer in the list means no app is using WebFM API, so we should
    // turn off the FM HW.
    if (IsEnabled())
    {
      LOG("Turn off FM HW");
      NS_DispatchToMainThread(new DisableRunnable());
    }
  }
}

bool
FMRadioService::IsEnabled()
{
  return IsFMRadioOn();
}

double
FMRadioService::GetFrequency()
{
  if (IsEnabled())
  {
    int32_t frequencyInKHz = GetFMRadioFrequency();
    return frequencyInKHz / 1000.0;
  }

  return 0;
}

double
FMRadioService::GetFrequencyUpperBound()
{
  return mUpperBoundInMHz;
}

double
FMRadioService::GetFrequencyLowerBound()
{
  return mLowerBoundInMHz;
}

double
FMRadioService::GetChannelWidth()
{
  return mChannelWidthInMHz;
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

  int32_t roundedFrequency = RoundFrequency(aFrequencyInMHz * 1000);

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

  NS_DispatchToMainThread(new EnableRunnable(mUpperBoundInMHz * 1000,
                                             mLowerBoundInMHz * 1000,
                                             mChannelWidthInMHz * 1000));
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

  if (!mEnabling && !IsFMRadioOn())
  {
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
  if (!IsFMRadioOn())
  {
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

  int32_t roundedFrequency = RoundFrequency(aFrequencyInMHz * 1000);

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
  LOG("Check Main Thread");
  NS_ASSERTION(NS_IsMainThread(), "Wrong thread!");

  if (!IsFMRadioOn())
  {
    aRunnable->SetReply(ErrorResponse(
      NS_ConvertASCIItoUTF16("It's disabled")));
    NS_DispatchToMainThread(aRunnable);
    return;
  }

  if (mSeeking)
  {
    aRunnable->SetReply(ErrorResponse(
      NS_ConvertASCIItoUTF16("It's disabling")));
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

  mSeeking = true;
  mSeekRequest = aRunnable;

  NS_DispatchToMainThread(new SeekRunnable(upward));
}

void
FMRadioService::CancelSeek(ReplyRunnable* aRunnable)
{
  LOG("Check Main Thread for CancelSeek");
  NS_ASSERTION(NS_IsMainThread(), "Wrong thread!");

  if (!IsFMRadioOn())
  {
    LOG("It's disabled");
    aRunnable->SetReply(ErrorResponse(
      NS_ConvertASCIItoUTF16("It's disabled")));
    NS_DispatchToMainThread(aRunnable);
    return;
  }

  if (!mSeeking)
  {
    LOG("It's not seeking");
    aRunnable->SetReply(ErrorResponse(
      NS_ConvertASCIItoUTF16("It's not seeking")));
    NS_DispatchToMainThread(aRunnable);
    return;
  }

  if (mDisabling)
  {
    LOG("It's disabling");
    aRunnable->SetReply(ErrorResponse(
      NS_ConvertASCIItoUTF16("It's disabling")));
    NS_DispatchToMainThread(aRunnable);
    return;
  }

  LOG("Cancel seeking immediately");
  // Cancel it immediately to prevent seeking complete
  CancelFMRadioSeek();

  LOG("Fail seeking request");
  mSeeking = false;
  mSeekRequest->SetReply(ErrorResponse(
    NS_ConvertASCIItoUTF16("It's canceled")));
  NS_DispatchToMainThread(mSeekRequest);
  mSeekRequest = nullptr;

  LOG("Fire success for cancel seeking");
  aRunnable->SetReply(SuccessResponse());
  NS_DispatchToMainThread(aRunnable);
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
      // should check if `mEnabling` is true, if false, we should skip it and
      // just update the power state and frequency.
      if (mEnabling)
      {
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
      }
      else
      {
        LOG("Not triggered by current thread, skip it.");
      }

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
      // should check if `mDisabling` is true, if false, we should skip it and
      // just update the power state.
      if (mDisabling)
      {
        mDisabling = false;
        mDisableRequest->SetReply(SuccessResponse());
        NS_DispatchToMainThread(mDisableRequest);
        mDisableRequest = nullptr;
      }
      else
      {
        LOG("Not triggered by current thread, skip it.");
      }

      UpdatePowerState();

      // If the FM Radio is currently seeking, no fail-to-seek or similar
      // event will be fired, execute the seek callback manually.
      if (mSeeking)
      {
        mSeeking = false;
        mSeekRequest->SetReply(ErrorResponse(
            NS_ConvertASCIItoUTF16("It's canceled")));
        NS_DispatchToMainThread(mSeekRequest);
        mSeekRequest = nullptr;
      }
      break;
    }
    case FM_RADIO_OPERATION_SEEK:
    {
      LOG("FM HW seek complete.");

      // The signal we received might be triggered by other thread/process, we
      // should check if `mSeeking` is true, if false, we should skip it and
      // and just update the frequency
      if (mSeeking)
      {
        LOG("Fire success event for seeking request");
        mSeeking = false;
        mSeekRequest->SetReply(SuccessResponse());
        NS_DispatchToMainThread(mSeekRequest);
        mSeekRequest = nullptr;
      }
      else
      {
        LOG("Not triggered by current thread, skip it.");
      }

      LOG("Update frequency");
      UpdateFrequency();
      break;
    }
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
