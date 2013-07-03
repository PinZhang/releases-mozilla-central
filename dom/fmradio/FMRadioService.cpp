/* -*- Mode: c++; c-basic-offset: 2; indent-tabs-mode: nil; tab-width: 40 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "FMRadioService.h"
#include "mozilla/Hal.h"
#include "nsIAudioManager.h"
#include "AudioManager.h"
#include "nsDOMClassInfo.h"
#include "mozilla/Preferences.h"
#include "mozilla/dom/fmradio/FMRadioChild.h"
#include "nsIObserverService.h"
#include "nsISettingsService.h"
#include "nsJSUtils.h"

#undef LOG
#define LOG(args...) FM_LOG("FMRadioService", args)

#define BAND_87500_108000_kHz 1
#define BAND_76000_108000_kHz 2
#define BAND_76000_90000_kHz  3

#define CHANNEL_WIDTH_200KHZ 200
#define CHANNEL_WIDTH_100KHZ 100
#define CHANNEL_WIDTH_50KHZ  50

#define MOZSETTINGS_CHANGED_ID "mozsettings-changed"
#define SETTING_KEY_RIL_RADIO_DISABLED "ril.radio.disabled"

using namespace mozilla::hal;
using mozilla::Preferences;

BEGIN_FMRADIO_NAMESPACE

class RilSettingsObserver MOZ_FINAL : public nsIObserver
{
public:
  NS_DECL_ISUPPORTS

  NS_IMETHODIMP
  Observe(nsISupports * aSubject, const char * aTopic, const PRUnichar * aData)
  {
    MOZ_ASSERT(NS_IsMainThread());
    MOZ_ASSERT(sFMRadioService);

    if (strcmp(aTopic, MOZSETTINGS_CHANGED_ID)) {
      return NS_OK;
    }

    // The string that we're interested in will be a JSON string looks like:
    //  {"key":"ril.radio.disabled","value":true}
    mozilla::AutoSafeJSContext cx;
    const nsDependentString dataStr(aData);
    JS::Rooted<JS::Value> val(cx);
    if (!JS_ParseJSON(cx, dataStr.get(), dataStr.Length(), &val) ||
        !val.isObject()) {
      return NS_OK;
    }

    JS::Rooted<JSObject*> obj(cx, &val.toObject());
    JS::Rooted<JS::Value> key(cx);
    if (!JS_GetProperty(cx, obj, "key", key.address()) ||
        !key.isString()) {
      return NS_OK;
    }

    JS::Rooted<JSString*> jsKey(cx, key.toString());
    nsDependentJSString keyStr;
    if (!keyStr.init(cx, jsKey)) {
      return NS_OK;
    }

    JS::Rooted<JS::Value> value(cx);
    if (!JS_GetProperty(cx, obj, "value", value.address())) {
      return NS_OK;
    }

    if (keyStr.EqualsLiteral(SETTING_KEY_RIL_RADIO_DISABLED)) {
      if (!value.isBoolean()) {
        return NS_OK;
      }

      FMRadioService* fmRadioService = FMRadioService::Singleton();

      fmRadioService->mRilDisabled = value.toBoolean();

      // Disable the FM radio HW if Airplane mode is enabled.
      if (fmRadioService->mRilDisabled) {
        LOG("mRilDisabled is false, disable the FM right now");
        fmRadioService->Disable(nullptr);
      }

      return NS_OK;
    }

    return NS_OK;
  }
};

NS_IMPL_ISUPPORTS1(RilSettingsObserver, nsIObserver)

// static
IFMRadioService*
IFMRadioService::Singleton()
{
  if (XRE_GetProcessType() != GeckoProcessType_Default) {
    LOG("Return FMRadioChild for OOP");
    return FMRadioChild::Singleton();
  } else {
    return FMRadioService::Singleton();
  }
}

StaticRefPtr<FMRadioService> FMRadioService::sFMRadioService;

FMRadioService::FMRadioService()
  : mPendingFrequencyInKHz(0)
  , mState(Disabled)
  , mHasReadRilSetting(false)
  , mRilDisabled(false)
  , mPendingRequest(nullptr)
  , mObserverList(FMRadioEventObserverList())
{
  LOG("constructor");

  // Read power state and frequency from Hal.
  mEnabled = IsFMRadioOn();
  if (mEnabled) {
    mPendingFrequencyInKHz = GetFMRadioFrequency();
    SetState(Enabled);
  }

  switch (Preferences::GetInt("dom.fmradio.band", BAND_87500_108000_kHz)) {
    case BAND_76000_90000_kHz:
      mUpperBoundInKHz = 90000;
      mLowerBoundInKHz = 76000;
      break;
    case BAND_76000_108000_kHz:
      mUpperBoundInKHz = 108000;
      mLowerBoundInKHz = 76000;
      break;
    case BAND_87500_108000_kHz:
    default:
      mUpperBoundInKHz = 108000;
      mLowerBoundInKHz = 87500;
      break;
  }

  switch (Preferences::GetInt("dom.fmradio.channelWidth",
                              CHANNEL_WIDTH_100KHZ)) {
    case CHANNEL_WIDTH_200KHZ:
      mChannelWidthInKHz = 200;
      break;
    case CHANNEL_WIDTH_50KHZ:
      mChannelWidthInKHz = 50;
      break;
    case CHANNEL_WIDTH_100KHZ:
    default:
      mChannelWidthInKHz = 100;
      break;
  }

  mSettingsObserver = new RilSettingsObserver();

  nsCOMPtr<nsIObserverService> obs = services::GetObserverService();

  if (NS_FAILED(obs->AddObserver(mSettingsObserver,
                                 MOZSETTINGS_CHANGED_ID,
                                 /* useWeak */ false))) {
    NS_WARNING("Failed to add settings change observer!");
  }

  RegisterFMRadioObserver(this);
}

FMRadioService::~FMRadioService()
{
  LOG("destructor");
  nsCOMPtr<nsIObserverService> obs = services::GetObserverService();
  if (!obs || NS_FAILED(obs->RemoveObserver(mSettingsObserver,
                                            MOZSETTINGS_CHANGED_ID))) {
    NS_WARNING("Can't unregister observers, or already unregistered");
  }
  UnregisterFMRadioObserver(this);
}

class EnableRunnable MOZ_FINAL : public nsRunnable
{
public:
  EnableRunnable(int32_t aUpperLimit, int32_t aLowerLimit, int32_t aSpaceType)
    : mUpperLimit(aUpperLimit)
    , mLowerLimit(aLowerLimit)
    , mSpaceType(aSpaceType) { }

  NS_IMETHOD Run()
  {
    FMRadioSettings info;
    info.upperLimit() = mUpperLimit;
    info.lowerLimit() = mLowerLimit;
    info.spaceType() = mSpaceType;

    EnableFMRadio(info);

    nsCOMPtr<nsIAudioManager> audioManager =
      do_GetService(NS_AUDIOMANAGER_CONTRACTID);
    audioManager->SetFmRadioAudioEnabled(true);

    // TODO apply path from bug 862899: AudioChannelAgent per process
    return NS_OK;
  }

private:
  int32_t mUpperLimit;
  int32_t mLowerLimit;
  int32_t mSpaceType;
};

/**
 * Read the airplane-mode setting, if the airplane-mode is not enabled, we
 * enable the FM radio.
 */
class ReadRilSettingTask MOZ_FINAL : public nsISettingsServiceCallback
{
public:
  NS_DECL_ISUPPORTS

  ReadRilSettingTask() { }

  NS_IMETHOD
  Handle(const nsAString& aName, const JS::Value& aResult)
  {
    LOG("Read settings value");

    FMRadioService* fmRadioService = FMRadioService::Singleton();

    fmRadioService->mHasReadRilSetting = true;

    if (!fmRadioService->mPendingRequest) {
      return NS_OK;
    }

    if (!aResult.isBoolean()) {
      LOG("Settings is not boolean");
      fmRadioService->mPendingRequest->SetReply(
        ErrorResponse(NS_LITERAL_STRING("Unexpected error")));
      NS_DispatchToMainThread(fmRadioService->mPendingRequest);

      // Failed to read the setting value, set the state back to Disabled.
      fmRadioService->SetState(Disabled);
      return NS_OK;
    }

    fmRadioService->mRilDisabled = aResult.toBoolean();
    LOG("Settings is: %d", fmRadioService->mRilDisabled);
    if (!fmRadioService->mRilDisabled) {
      EnableRunnable* runnable =
        new EnableRunnable(fmRadioService->mUpperBoundInKHz,
                           fmRadioService->mLowerBoundInKHz,
                           fmRadioService->mChannelWidthInKHz);
      NS_DispatchToMainThread(runnable);
    } else {
      fmRadioService->mPendingRequest->SetReply(ErrorResponse(
        NS_LITERAL_STRING("Airplane mode is enabled")));
      NS_DispatchToMainThread(fmRadioService->mPendingRequest);

      // Airplane mode is enabled, set the state back to Disabled.
      fmRadioService->SetState(Disabled);
    }

    return NS_OK;
  }

  NS_IMETHOD
  HandleError(const nsAString& aName)
  {
    LOG("Can not read settings value.");

    FMRadioService* fmRadioService = FMRadioService::Singleton();

    if (!fmRadioService->mPendingRequest) {
      return NS_OK;
    }

    fmRadioService->mPendingRequest->SetReply(ErrorResponse(
      NS_LITERAL_STRING("Unexpected error")));
    NS_DispatchToMainThread(fmRadioService->mPendingRequest);
    fmRadioService->SetState(Disabled);
    return NS_OK;
  }
};

NS_IMPL_ISUPPORTS1(ReadRilSettingTask, nsISettingsServiceCallback)

class DisableRunnable MOZ_FINAL : public nsRunnable
{
public:
  DisableRunnable() { }

  NS_IMETHOD Run()
  {
    // Fix Bug 796733. DisableFMRadio should be called before
    // SetFmRadioAudioEnabled to prevent the annoying beep sound.
    DisableFMRadio();

    nsCOMPtr<nsIAudioManager> audioManager =
      do_GetService(NS_AUDIOMANAGER_CONTRACTID);

    audioManager->SetFmRadioAudioEnabled(false);

    return NS_OK;
  }
};

class SetFrequencyRunnable MOZ_FINAL : public nsRunnable
{
public:
  SetFrequencyRunnable(FMRadioService* aService, int32_t aFrequency)
    : mFrequency(aFrequency) { }

  NS_IMETHOD Run()
  {
    SetFMRadioFrequency(mFrequency);

    FMRadioService* fmRadioService = FMRadioService::Singleton();
    fmRadioService->UpdateFrequency();

    return NS_OK;
  }

private:
  int32_t mFrequency;
};

class SeekRunnable MOZ_FINAL : public nsRunnable
{
public:
  SeekRunnable(FMRadioSeekDirection aDirection) : mDirection(aDirection) { }

  NS_IMETHOD Run()
  {
    switch (mDirection) {
      case FM_RADIO_SEEK_DIRECTION_UP:
      case FM_RADIO_SEEK_DIRECTION_DOWN:
        FMRadioSeek(mDirection);
        break;
      default:
        MOZ_CRASH();
    }

    return NS_OK;
  }

private:
  FMRadioSeekDirection mDirection;
};

void
FMRadioService::SetState(FMRadioState aState)
{
  mState = aState;
  mPendingRequest = nullptr;
}

void
FMRadioService::AddObserver(FMRadioEventObserver* aObserver)
{
  LOG("Register handler");
  mObserverList.AddObserver(aObserver);
}

void
FMRadioService::RemoveObserver(FMRadioEventObserver* aObserver)
{
  LOG("Unregister handler");
  mObserverList.RemoveObserver(aObserver);

  if (mObserverList.Length() == 0)
  {
    // Turning off the FM radio HW because observer list is empty.
    if (IsFMRadioOn()) {
      LOG("Turn off FM HW");
      NS_DispatchToMainThread(new DisableRunnable());
    }
  }
}

/**
 * Round the frequency to match the range of frequency and the channel width. If
 * the given frequency is out of range, return 0. For example:
 *  - lower: 87500KHz, upper: 108000KHz, channel width: 200KHz
 *    87.6MHz is rounded to 87700KHz
 *    87.58MHz is rounded to 87500KHz
 *    87.49MHz is rounded to 87500KHz
 *    109MHz is not rounded, 0 will be returned
 *
 * We take frequency in MHz to prevent precision losing, and return rounded
 * value in KHz for Gonk using.
 */
int32_t
FMRadioService::RoundFrequency(double aFrequencyInMHz)
{
  double halfChannelWidthInMHz = mChannelWidthInKHz / 1000.0 / 2;

  if (aFrequencyInMHz < mLowerBoundInKHz / 1000.0 - halfChannelWidthInMHz ||
      aFrequencyInMHz > mUpperBoundInKHz / 1000.0 + halfChannelWidthInMHz) {
    return 0;
  }

  aFrequencyInMHz += halfChannelWidthInMHz;
  int32_t aFrequencyInKHz = round(aFrequencyInMHz * 1000);

  int32_t partToBeRounded = aFrequencyInKHz - mLowerBoundInKHz;
  int32_t roundedPart = partToBeRounded / mChannelWidthInKHz *
                        mChannelWidthInKHz;

  return mLowerBoundInKHz + roundedPart;
}

bool
FMRadioService::IsEnabled() const
{
  return IsFMRadioOn();
}

double
FMRadioService::GetFrequency() const
{
  if (IsEnabled()) {
    int32_t frequencyInKHz = GetFMRadioFrequency();
    return frequencyInKHz / 1000.0;
  }

  return 0;
}

double
FMRadioService::GetFrequencyUpperBound() const
{
  return mUpperBoundInKHz / 1000.0;
}

double
FMRadioService::GetFrequencyLowerBound() const
{
  return mLowerBoundInKHz / 1000.0;
}

double
FMRadioService::GetChannelWidth() const
{
  return mChannelWidthInKHz / 1000.0;
}

void
FMRadioService::Enable(double aFrequencyInMHz, ReplyRunnable* aReplyRunnable)
{
  // We need to call EnableFMRadio() in main thread
  MOZ_ASSERT(NS_IsMainThread(), "Wrong thread!");

  switch (mState) {
    case Enabled:
      LOG("FM radio currently enabled");
      aReplyRunnable->SetReply(
        ErrorResponse(NS_LITERAL_STRING("FM radio currently enabled")));
      NS_DispatchToMainThread(aReplyRunnable);
      return;
    case Disabling:
      LOG("FM radio currently disabling");
      aReplyRunnable->SetReply(
        ErrorResponse(NS_LITERAL_STRING("FM radio currently disabling")));
      NS_DispatchToMainThread(aReplyRunnable);
      return;
    case Enabling:
      LOG("FM radio currently enabling");
      aReplyRunnable->SetReply(
        ErrorResponse(NS_LITERAL_STRING("FM radio currently enabling")));
      NS_DispatchToMainThread(aReplyRunnable);
      return;
  }

  int32_t roundedFrequency = RoundFrequency(aFrequencyInMHz);

  if (!roundedFrequency) {
    LOG("Frequency is out of range");
    aReplyRunnable->SetReply(ErrorResponse(
      NS_LITERAL_STRING("Frequency is out of range")));
    NS_DispatchToMainThread(aReplyRunnable);
    return;
  }

  if (mHasReadRilSetting && mRilDisabled) {
    LOG("Airplane mode is enabled");
    aReplyRunnable->SetReply(ErrorResponse(
      NS_LITERAL_STRING("Airplane mode is enabled")));
    NS_DispatchToMainThread(aReplyRunnable);
    return;
  }

  SetState(Enabling);
  // Cache the enable request just in case disable() is called
  // while the FM radio HW is being enabled.
  mPendingRequest = aReplyRunnable;

  // Cache the frequency value, and set it after the FM radio HW is enabled
  mPendingFrequencyInKHz = roundedFrequency;

  if (!mHasReadRilSetting) {
    LOG("Settings value has not been read.");
    nsCOMPtr<nsISettingsService> settings =
      do_GetService("@mozilla.org/settingsService;1");

    nsCOMPtr<nsISettingsServiceLock> settingsLock;
    nsresult rv = settings->CreateLock(getter_AddRefs(settingsLock));
    MOZ_ASSERT(rv, "Can't create settings lock");

    nsRefPtr<ReadRilSettingTask> callback = new ReadRilSettingTask();
    rv = settingsLock->Get(SETTING_KEY_RIL_RADIO_DISABLED, callback);
    MOZ_ASSERT(rv, "Can't get settings value");

    return;
  }

  NS_DispatchToMainThread(new EnableRunnable(mUpperBoundInKHz,
                                             mLowerBoundInKHz,
                                             mChannelWidthInKHz));
}

void
FMRadioService::Disable(ReplyRunnable* aReplyRunnable)
{
  LOG("Check Main Thread");
  MOZ_ASSERT(NS_IsMainThread(), "Wrong thread!");

  switch (mState) {
    case Disabling:
      LOG("FM radio currently disabling");
      if (aReplyRunnable) {
        aReplyRunnable->SetReply(
          ErrorResponse(NS_LITERAL_STRING("FM radio currently disabling")));
        NS_DispatchToMainThread(aReplyRunnable);
      }
      return;
    case Disabled:
      LOG("It's disabled");
      if (aReplyRunnable) {
        aReplyRunnable->SetReply(
          ErrorResponse(NS_LITERAL_STRING("It's disabled")));
        NS_DispatchToMainThread(aReplyRunnable);
      }
      return;
  }

  // If the FM Radio is currently seeking, no fail-to-seek or similar
  // event will be fired, execute the seek callback manually.
  if (mState == Seeking) {
    mPendingRequest->SetReply(ErrorResponse(
      NS_LITERAL_STRING("It's canceled")));
    NS_DispatchToMainThread(mPendingRequest);
  }

  nsRefPtr<ReplyRunnable> enablingRequest = mPendingRequest;
  FMRadioState preState = mState;
  SetState(Disabling);
  mPendingRequest = aReplyRunnable;

  if (preState == Enabling) {
    // If the radio is currently enabling, we fire the error callback on the
    // enable request immediately. When the radio finishes enabling, we'll call
    // DoDisable and fire the success callback for the disable request.
    LOG("FM radio currently enabling, fail it immediately.");
    enablingRequest->SetReply(
      ErrorResponse(NS_LITERAL_STRING("It's canceled")));
    NS_DispatchToMainThread(enablingRequest);

    // If we havn't read the ril settings yet which means we won't enable
    // the FM radio HW, fail the disable request immediately.
    if (!mHasReadRilSetting) {
      SetState(Disabled);

      if (aReplyRunnable) {
        aReplyRunnable->SetReply(SuccessResponse());
        NS_DispatchToMainThread(aReplyRunnable);
      }
    }

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
FMRadioService::SetFrequency(double aFrequencyInMHz,
                             ReplyRunnable* aReplyRunnable)
{
  LOG("Check Main Thread");
  MOZ_ASSERT(NS_IsMainThread(), "Wrong thread!");

  switch (mState) {
    case Disabled:
      LOG("It's Disabled");
      aReplyRunnable->SetReply(
        ErrorResponse(NS_LITERAL_STRING("It's disabled")));
      NS_DispatchToMainThread(aReplyRunnable);
      return;
    case Enabling:
      aReplyRunnable->SetReply(
        ErrorResponse(NS_LITERAL_STRING("FM radio currently enabling")));
      NS_DispatchToMainThread(aReplyRunnable);
      return;
    case Disabling:
      aReplyRunnable->SetReply(
        ErrorResponse(NS_LITERAL_STRING("FM radio currently disabling")));
      NS_DispatchToMainThread(aReplyRunnable);
      return;
  }

  int32_t roundedFrequency = RoundFrequency(aFrequencyInMHz);

  if (!roundedFrequency) {
    aReplyRunnable->SetReply(ErrorResponse(
      NS_LITERAL_STRING("Frequency is out of range")));
    NS_DispatchToMainThread(aReplyRunnable);
    return;
  }

  aReplyRunnable->SetReply(SuccessResponse());
  NS_DispatchToMainThread(aReplyRunnable);

  NS_DispatchToMainThread(new SetFrequencyRunnable(this, roundedFrequency));
}

void
FMRadioService::Seek(FMRadioSeekDirection aDirection,
                     ReplyRunnable* aReplyRunnable)
{
  LOG("Check Main Thread");
  MOZ_ASSERT(NS_IsMainThread(), "Wrong thread!");

  switch (mState) {
    case Enabling:
      LOG("FM radio currently enabling");
      aReplyRunnable->SetReply(
        ErrorResponse(NS_LITERAL_STRING("FM radio currently enabling")));
      NS_DispatchToMainThread(aReplyRunnable);
      return;
    case Disabled:
      LOG("It's disabled");
      aReplyRunnable->SetReply(
        ErrorResponse(NS_LITERAL_STRING("It's disabled")));
      NS_DispatchToMainThread(aReplyRunnable);
      return;
    case Seeking:
      LOG("It's Seeking");
      aReplyRunnable->SetReply(
        ErrorResponse(NS_LITERAL_STRING("It's Seeking")));
      NS_DispatchToMainThread(aReplyRunnable);
      return;
    case Disabling:
      LOG("FM radio currently disabling");
      aReplyRunnable->SetReply(
        ErrorResponse(NS_LITERAL_STRING("FM radio currently disabling")));
      NS_DispatchToMainThread(aReplyRunnable);
      return;
  }

  SetState(Seeking);
  mPendingRequest = aReplyRunnable;

  NS_DispatchToMainThread(new SeekRunnable(aDirection));
}

void
FMRadioService::CancelSeek(ReplyRunnable* aReplyRunnable)
{
  LOG("Check Main Thread for CancelSeek");
  MOZ_ASSERT(NS_IsMainThread(), "Wrong thread!");

  // We accept canceling seek request only if it's currently seeking.
  if (mState != Seeking) {
    LOG("It's not seeking");
    aReplyRunnable->SetReply(
      ErrorResponse(NS_LITERAL_STRING("It's not seeking")));
    NS_DispatchToMainThread(aReplyRunnable);
    return;
  }

  LOG("Cancel seeking immediately");
  // Cancel the seek immediately to prevent it from completing.
  CancelFMRadioSeek();

  LOG("Fail seeking request");
  mPendingRequest->SetReply(ErrorResponse(NS_LITERAL_STRING("It's canceled")));
  NS_DispatchToMainThread(mPendingRequest);

  SetState(Enabled);

  LOG("Fire success for cancel seeking");
  aReplyRunnable->SetReply(SuccessResponse());
  NS_DispatchToMainThread(aReplyRunnable);
}

void
FMRadioService::NotifyFMRadioEvent(FMRadioEventType aType)
{
  mObserverList.Broadcast(aType);
}

void
FMRadioService::Notify(const FMRadioOperationInformation& aInfo)
{
  switch (aInfo.operation()) {
    case FM_RADIO_OPERATION_ENABLE:
    {
      LOG("FM HW is enabled.");

      // We will receive FM_RADIO_OPERATION_ENABLE if we call DisableFMRadio(),
      // there must be some problem with HAL layer, we need to double check
      // the FM radio HW status here.
      if (!IsFMRadioOn())
      {
        LOG("FMRadio should not be off!!");
        return;
      }

      // If we're disabling, go disable the radio right now.
      // We don't change the `mState` value here, because we have set it to
      // `Disabled` when Disable() is called.
      if (mState == Disabling) {
        LOG("We need to disable FM Radio for the waiting request.");
        DoDisable();
        return;
      }

      // The signal we received might be triggered by enable request in other
      // process, we should check if `mState` is Enabling, if it's not enabling,
      // we should skip it and just update the power state and frequency.
      if (mState == Enabling) {
        // Fire success callback for the enable request.
        LOG("Fire Success for enable request.");
        mPendingRequest->SetReply(SuccessResponse());
        NS_DispatchToMainThread(mPendingRequest);

        SetState(Enabled);

        // To make sure the FM app will get the right frequency after the FM
        // radio is enabled, we have to set the frequency first.
        SetFMRadioFrequency(mPendingFrequencyInKHz);
      } else {
        LOG("Not triggered by current thread, skip it.");
      }

      // Update the current frequency without distributing the
      // `FrequencyChangedEvent`, to make sure the FM app will get the right
      // frequency when the `StateChangedEvent` is fired.
      mPendingFrequencyInKHz = GetFMRadioFrequency();
      UpdatePowerState();

      // The frequency was changed from '0' to some meaningful number, so we
      // should send the `FrequencyChangedEvent` manually.
      NotifyFMRadioEvent(FrequencyChanged);
      break;
    }
    case FM_RADIO_OPERATION_DISABLE:
    {
      LOG("FM HW is disabled.");

      // The signal we received might be triggered by disable request in other
      // process, we should check if `mState` is Disabling, if false, we should
      // skip it and just update the power state.
      if (mState == Disabling) {
        if (mPendingRequest) {
          mPendingRequest->SetReply(SuccessResponse());
          NS_DispatchToMainThread(mPendingRequest);
        }

        SetState(Disabled);
      } else {
        LOG("Not triggered by current thread, skip it.");
      }

      UpdatePowerState();
      break;
    }
    case FM_RADIO_OPERATION_SEEK:
    {
      LOG("FM HW seek complete.");

      // The signal we received might be triggered by disable request in other
      // process, we should check if `mState` is Seeking, if false, we should
      // skip it and just update the frequency.
      if (mState == Seeking) {
        LOG("Fire success event for seeking request");
        mPendingRequest->SetReply(SuccessResponse());
        NS_DispatchToMainThread(mPendingRequest);

        SetState(Enabled);
      } else {
        LOG("Not triggered by current thread, skip it.");
      }

      LOG("Update frequency");
      UpdateFrequency();
      break;
    }
    default:
      MOZ_CRASH();
  }
}

void
FMRadioService::UpdatePowerState()
{
  bool enabled = IsFMRadioOn();
  if (enabled != mEnabled) {
    mEnabled = enabled;
    NotifyFMRadioEvent(EnabledChanged);
  }
}

void
FMRadioService::UpdateFrequency()
{
  int32_t frequency = GetFMRadioFrequency();
  if (mPendingFrequencyInKHz != frequency) {
    mPendingFrequencyInKHz = frequency;
    NotifyFMRadioEvent(FrequencyChanged);
  }
}

// static
FMRadioService*
FMRadioService::Singleton()
{
  MOZ_ASSERT(XRE_GetProcessType() == GeckoProcessType_Default);
  MOZ_ASSERT(NS_IsMainThread());

  if (!sFMRadioService) {
    LOG("Return cached gFMRadioService");
    sFMRadioService = new FMRadioService();
  }

  return sFMRadioService;
}

END_FMRADIO_NAMESPACE

