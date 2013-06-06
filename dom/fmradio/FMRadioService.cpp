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
#include "mozilla/dom/fmradio/FMRadioChildService.h"
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

#define DOM_FMRADIO_BAND_PREF "dom.fmradio.band"
#define DOM_FMRADIO_CHANNEL_WIDTH_PREF "dom.fmradio.channelWidth"

#define MOZSETTINGS_CHANGED_ID "mozsettings-changed"
#define SETTING_KEY_RIL_RADIO_DISABLED "ril.radio.disabled"

using namespace mozilla::hal;
using mozilla::Preferences;

BEGIN_FMRADIO_NAMESPACE

nsRefPtr<FMRadioService>
FMRadioService::sFMRadioService;

FMRadioService::FMRadioService()
  : mFrequencyInKHz(0)
  , mState(Disabled)
  , mHasReadRilSetting(false)
  , mRilDisabled(false)
  , mPendingRequest(nullptr)
  , mObserverList(FMRadioEventObserverList())
{
  LOG("constructor");

  // read power state and frequency from Hal
  mEnabled = IsFMRadioOn();
  if (mEnabled) {
    mFrequencyInKHz = GetFMRadioFrequency();
    SetState(Enabled);
  }

  switch (Preferences::GetInt(DOM_FMRADIO_BAND_PREF, BAND_87500_108000_kHz)) {
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
    CHANNEL_WIDTH_100KHZ)) {
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

  mSettingsObserver = new RilSettingsObserver(this);

  nsCOMPtr<nsIObserverService> obs = services::GetObserverService();
  MOZ_ASSERT(obs);

  if (NS_FAILED(obs->AddObserver(mSettingsObserver,
                                 MOZSETTINGS_CHANGED_ID,
                                 false))) {
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
    MOZ_ASSERT(audioManager, "No AudioManager");

    audioManager->SetFmRadioAudioEnabled(true);

    // TODO apply path of bug 862899: AudioChannelAgent per process
    return NS_OK;
  }

private:
  int32_t mUpperLimit;
  int32_t mLowerLimit;
  int32_t mSpaceType;
};

class ReadRilSettingTask MOZ_FINAL : public nsISettingsServiceCallback
{
public:
  NS_DECL_ISUPPORTS

  ReadRilSettingTask(FMRadioService* aFMRadioService)
    : mFMRadioService(aFMRadioService) { }

  NS_IMETHOD
  Handle(const nsAString& aName, const JS::Value& aResult)
  {
    LOG("Read settings value");
    mFMRadioService->mHasReadRilSetting = true;

    if (!aResult.isBoolean()) {
      LOG("Settings is not boolean");
      mFMRadioService->mPendingRequest->SetReply(
        ErrorResponse(NS_LITERAL_STRING("Unexpected error")));
      NS_DispatchToMainThread(mFMRadioService->mPendingRequest);

      // Failed to read the setting value, set the state back to Disabled.
      mFMRadioService->SetState(Disabled);
      return NS_OK;
    }

    mFMRadioService->mRilDisabled = aResult.toBoolean();
    LOG("Settings is: %d", mFMRadioService->mRilDisabled);
    if (!mFMRadioService->mRilDisabled) {
      EnableRunnable* runnable =
        new EnableRunnable(mFMRadioService->mUpperBoundInMHz * 1000,
                           mFMRadioService->mLowerBoundInMHz * 1000,
                           mFMRadioService->mChannelWidthInMHz * 1000);
      NS_DispatchToMainThread(runnable);
    } else {
      mFMRadioService->mPendingRequest->SetReply(ErrorResponse(
        NS_LITERAL_STRING("Airplane mode is enabled")));
      NS_DispatchToMainThread(mFMRadioService->mPendingRequest);

      // Airplane mode is enabled, set the state back to Disabled.
      mFMRadioService->SetState(Disabled);
    }

    return NS_OK;
  }

  NS_IMETHOD
  HandleError(const nsAString& aName)
  {
    LOG("Can not read settings value.");
    mFMRadioService->mPendingRequest->SetReply(ErrorResponse(
      NS_LITERAL_STRING("Unexpected error")));
    NS_DispatchToMainThread(mFMRadioService->mPendingRequest);
    mFMRadioService->SetState(Disabled);
    return NS_OK;
  }

private:
  nsRefPtr<FMRadioService> mFMRadioService;
};

NS_IMPL_ISUPPORTS1(ReadRilSettingTask, nsISettingsServiceCallback)

class DisableRunnable MOZ_FINAL : public nsRunnable
{
public:
  DisableRunnable() { }

  NS_IMETHOD Run()
  {
    // Fix Bug 796733.
    // DisableFMRadio should be called before SetFmRadioAudioEnabled to prevent
    // the annoying beep sound.
    DisableFMRadio();

    nsCOMPtr<nsIAudioManager> audioManager =
      do_GetService(NS_AUDIOMANAGER_CONTRACTID);
    MOZ_ASSERT(audioManager, "No AudioManager");

    audioManager->SetFmRadioAudioEnabled(false);

    return NS_OK;
  }
};

class SetFrequencyRunnable MOZ_FINAL : public nsRunnable
{
public:
  SetFrequencyRunnable(FMRadioService* aService, int32_t aFrequency)
    : mService(aService)
    , mFrequency(aFrequency) { }

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

class SeekRunnable MOZ_FINAL : public nsRunnable
{
public:
  SeekRunnable(bool aUpward) : mUpward(aUpward) { }

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
    // No observer in the list means no app is using WebFM API, so we should
    // turn off the FM HW.
    if (IsFMRadioOn()) {
      LOG("Turn off FM HW");
      NS_DispatchToMainThread(new DisableRunnable());
    }

    LOG("No observer in the list, destroy myself");
    sFMRadioService = nullptr;
  }
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
FMRadioService::RoundFrequency(int32_t aFrequencyInKHz)
{
  int32_t lowerBoundInKHz = mLowerBoundInMHz * 1000;
  int32_t upperBoundInKHz = mUpperBoundInMHz * 1000;
  int32_t channelWidth = mChannelWidthInMHz * 1000;

  if (aFrequencyInKHz < lowerBoundInKHz ||
      aFrequencyInKHz > upperBoundInKHz) {
    return 0;
  }

  int32_t partToBeRounded = aFrequencyInKHz - lowerBoundInKHz;
  int32_t roundedPart = round(partToBeRounded / channelWidth) * channelWidth;

  return lowerBoundInKHz + roundedPart;
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
  return mUpperBoundInMHz;
}

double
FMRadioService::GetFrequencyLowerBound() const
{
  return mLowerBoundInMHz;
}

double
FMRadioService::GetChannelWidth() const
{
  return mChannelWidthInMHz;
}

void
FMRadioService::Enable(double aFrequencyInMHz, ReplyRunnable* aRunnable)
{
  // We need to call EnableFMRadio() in main thread
  MOZ_ASSERT(NS_IsMainThread(), "Wrong thread!");

  switch (mState) {
    case Enabled:
      LOG("It's enabled");
      aRunnable->SetReply(ErrorResponse(NS_LITERAL_STRING("It's enabled")));
      NS_DispatchToMainThread(aRunnable);
      return;
    case Disabling:
      LOG("It's disabling");
      aRunnable->SetReply(ErrorResponse(NS_LITERAL_STRING("It's disabling")));
      NS_DispatchToMainThread(aRunnable);
      return;
    case Enabling:
      LOG("It's enabling");
      aRunnable->SetReply(ErrorResponse(NS_LITERAL_STRING("It's enabling")));
      NS_DispatchToMainThread(aRunnable);
      return;
  }

  int32_t roundedFrequency = RoundFrequency(aFrequencyInMHz * 1000);

  if (!roundedFrequency) {
    LOG("Frequency is out of range");
    aRunnable->SetReply(ErrorResponse(
      NS_LITERAL_STRING("Frequency is out of range")));
    NS_DispatchToMainThread(aRunnable);
    return;
  }

  if (mHasReadRilSetting && mRilDisabled) {
    LOG("Airplane mode is enabled");
    aRunnable->SetReply(ErrorResponse(
      NS_LITERAL_STRING("Airplane mode is enabled")));
    NS_DispatchToMainThread(aRunnable);
    return;
  }

  SetState(Enabling);
  // Cache the enable request just in case disable() is called
  // while the FM radio HW is being enabled.
  mPendingRequest = aRunnable;

  // Cache the frequency value, and set it after the FM radio HW is enabled
  mFrequencyInKHz = roundedFrequency;

  if (!mHasReadRilSetting) {
    LOG("Settings value has not been read.");
    nsCOMPtr<nsISettingsService> settings =
      do_GetService("@mozilla.org/settingsService;1");
    MOZ_ASSERT(settings, "Can't create settings service");

    nsCOMPtr<nsISettingsServiceLock> settingsLock;
    nsresult rv = settings->CreateLock(getter_AddRefs(settingsLock));
    MOZ_ASSERT(rv, "Can't create settings lock");

    nsRefPtr<ReadRilSettingTask> callback = new ReadRilSettingTask(this);
    rv = settingsLock->Get(SETTING_KEY_RIL_RADIO_DISABLED, callback);
    MOZ_ASSERT(rv, "Can't get settings value");

    return;
  }

  NS_DispatchToMainThread(new EnableRunnable(mUpperBoundInMHz * 1000,
                                             mLowerBoundInMHz * 1000,
                                             mChannelWidthInMHz * 1000));
}

void
FMRadioService::Disable(ReplyRunnable* aRunnable)
{
  LOG("Check Main Thread");
  MOZ_ASSERT(NS_IsMainThread(), "Wrong thread!");

  switch (mState) {
    case Disabling:
      LOG("It's disabling");
      if (aRunnable) {
        aRunnable->SetReply(ErrorResponse(NS_LITERAL_STRING("It's disabling")));
        NS_DispatchToMainThread(aRunnable);
      }
      return;
    case Disabled:
      LOG("It's disabled");
      if (aRunnable) {
        aRunnable->SetReply(ErrorResponse(NS_LITERAL_STRING("It's disabled")));
        NS_DispatchToMainThread(aRunnable);
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

  bool isEnabling = mState == Enabling;
  nsRefPtr<ReplyRunnable> enablingRequest = mPendingRequest;
  SetState(Disabling);
  mPendingRequest = aRunnable;

  if (isEnabling) {
    // If the radio is currently enabling, we fire the error callback
    // immediately. When the radio finishes enabling, we'll fire the success
    // callback for the disable request.
    LOG("It's enabling, fail it immediately.");
    enablingRequest->SetReply(ErrorResponse(NS_LITERAL_STRING("It's canceled")));
    NS_DispatchToMainThread(enablingRequest);
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
  MOZ_ASSERT(NS_IsMainThread(), "Wrong thread!");

  switch (mState) {
    case Disabled:
    case Enabling:
      aRunnable->SetReply(ErrorResponse(NS_LITERAL_STRING("It's disabled")));
      NS_DispatchToMainThread(aRunnable);
      return;
    case Disabling:
      aRunnable->SetReply(ErrorResponse(NS_LITERAL_STRING("It's disabling")));
      NS_DispatchToMainThread(aRunnable);
      return;
  }

  int32_t roundedFrequency = RoundFrequency(aFrequencyInMHz * 1000);

  if (!roundedFrequency) {
    aRunnable->SetReply(ErrorResponse(
      NS_LITERAL_STRING("Frequency is out of range")));
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
  MOZ_ASSERT(NS_IsMainThread(), "Wrong thread!");

  switch (mState) {
    case Enabling:
    case Disabled:
      LOG("It's disabled");
      aRunnable->SetReply(ErrorResponse(NS_LITERAL_STRING("It's disabled")));
      NS_DispatchToMainThread(aRunnable);
      return;
    case Seeking:
      LOG("It's Seeking");
      aRunnable->SetReply(ErrorResponse(NS_LITERAL_STRING("It's Seeking")));
      NS_DispatchToMainThread(aRunnable);
      return;
    case Disabling:
      LOG("It's disabling");
      aRunnable->SetReply(ErrorResponse(NS_LITERAL_STRING("It's disabling")));
      NS_DispatchToMainThread(aRunnable);
      return;
  }

  SetState(Seeking);
  mPendingRequest = aRunnable;

  NS_DispatchToMainThread(new SeekRunnable(upward));
}

void
FMRadioService::CancelSeek(ReplyRunnable* aRunnable)
{
  LOG("Check Main Thread for CancelSeek");
  MOZ_ASSERT(NS_IsMainThread(), "Wrong thread!");

  // We accept canceling seek request only if it's currently seeking.
  if (mState != Seeking) {
    LOG("It's not seeking");
    aRunnable->SetReply(ErrorResponse(NS_LITERAL_STRING("It's not seeking")));
    NS_DispatchToMainThread(aRunnable);
    return;
  }

  LOG("Cancel seeking immediately");
  // Cancel it immediately to prevent seeking complete
  CancelFMRadioSeek();

  LOG("Fail seeking request");
  mPendingRequest->SetReply(ErrorResponse(NS_LITERAL_STRING("It's canceled")));
  NS_DispatchToMainThread(mPendingRequest);

  SetState(Enabled);

  LOG("Fire success for cancel seeking");
  aRunnable->SetReply(SuccessResponse());
  NS_DispatchToMainThread(aRunnable);
}

void
FMRadioService::NotifyFrequencyChanged(double aFrequency)
{
  mObserverList.Broadcast(FMRadioEventArgs(FrequencyChanged,
                                           IsFMRadioOn(),
                                           aFrequency));
}

void
FMRadioService::NotifyEnabledChanged(bool aEnabled, double aFrequency)
{
  mObserverList.Broadcast(FMRadioEventArgs(EnabledChanged,
                                           aEnabled,
                                           aFrequency));
}

void
FMRadioService::Notify(const FMRadioOperationInformation& info)
{
  switch (info.operation()) {
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

      // The signal we received might be triggered by enable request in other
      // process, we should check if `mState` is Disabling or Disabling, if
      // false, we should skip it and just update the power state and frequency.
      if (mState == Enabling || mState == Disabling) {
        // If we're disabling, go disable the radio right now.
        // We don't change the `mState` value here, because we have set it to
        // `Disabled` when Disable() is called.
        if (mState == Disabling) {
          LOG("We need to disable FM Radio for the waiting request.");
          DoDisable();
          return;
        }

        // Fire success callback for the enable request.
        LOG("Fire Success for enable request.");
        mPendingRequest->SetReply(SuccessResponse());
        NS_DispatchToMainThread(mPendingRequest);

        SetState(Enabled);

        // To make sure the FM app will get the right frequency after the FM
        // radio is enabled, we have to set the frequency first.
        SetFMRadioFrequency(mFrequencyInKHz);
      } else {
        LOG("Not triggered by current thread, skip it.");
      }

      // Update the current frequency without distributing the
      // `FrequencyChangedEvent`, to make sure the FM app will get the right
      // frequency when the `StateChangedEvent` is fired.
      mFrequencyInKHz = GetFMRadioFrequency();
      UpdatePowerState();

      // The frequency is changed from '0' to some meaningful number, so we
      // should distribute the `FrequencyChangedEvent` manually.
      double frequencyInMHz = mFrequencyInKHz / 1000.0;
      NotifyFrequencyChanged(frequencyInMHz);
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
      MOZ_NOT_REACHED();
      return;
  }
}

void
FMRadioService::UpdatePowerState()
{
  bool enabled = IsFMRadioOn();
  if (enabled != mEnabled) {
    mEnabled = enabled;
    // We pass the frequency with `StateChangedEvent` to make sure the FM app
    // will get the right frequency value when the `onenabled` event is fired.
    double frequencyInMHz = mFrequencyInKHz / 1000.0;
    NotifyEnabledChanged(enabled, frequencyInMHz);
  }
}

void
FMRadioService::UpdateFrequency()
{
  int32_t frequency = GetFMRadioFrequency();
  if (mFrequencyInKHz != frequency) {
    mFrequencyInKHz = frequency;
    double frequencyInMHz = mFrequencyInKHz / 1000.0;
    NotifyFrequencyChanged(frequencyInMHz);
  }
}

NS_IMETHODIMP
FMRadioService::RilSettingsObserver::Observe(nsISupports * aSubject,
                                             const char * aTopic,
                                             const PRUnichar * aData)
{
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(sFMRadioService);

  if (!strcmp(aTopic, MOZSETTINGS_CHANGED_ID)) {
    // The string that we're interested in will be a JSON string looks like:
    //  {"key":"ril.radio.disabled","value":true}
    mozilla::AutoSafeJSContext cx;
    nsDependentString dataStr(aData);
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

      mService->mRilDisabled = value.toBoolean();

      // Disable the FM radio HW if Airplane mode is enabled.
      if (mService->mRilDisabled) {
        LOG("mRilDisabled is false, disable the FM right now");
        mService->Disable(nullptr);
      }

      return NS_OK;
    }

    LOG("Observer: Settings is changed to %d", mService->mRilDisabled);
  }

  return NS_OK;
}

NS_IMPL_ISUPPORTS1(FMRadioService::RilSettingsObserver, nsIObserver)

bool
IsMainProcess()
{
  return XRE_GetProcessType() == GeckoProcessType_Default;
}

// static
IFMRadioService*
FMRadioService::Singleton()
{
  MOZ_ASSERT(NS_IsMainThread());

  if (!IsMainProcess()) {
    LOG("Return FMRadioChildService for OOP");
    return FMRadioChildService::Singleton();
  }

  if (sFMRadioService) {
    LOG("Return cached gFMRadioService");
    return sFMRadioService;
  }

  sFMRadioService = new FMRadioService();

  return sFMRadioService;
}

END_FMRADIO_NAMESPACE
