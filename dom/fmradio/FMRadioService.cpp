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
#include "nsCxPusher.h"

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

  nsCOMPtr<nsIObserverService> obs = services::GetObserverService();

  if (NS_FAILED(obs->AddObserver(this,
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
  if (!obs || NS_FAILED(obs->RemoveObserver(this,
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
        NS_LITERAL_STRING("Airplane mode currently enabled")));
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
  MOZ_ASSERT(NS_IsMainThread(), "Wrong thread!");
  mObserverList.AddObserver(aObserver);
}

void
FMRadioService::RemoveObserver(FMRadioEventObserver* aObserver)
{
  LOG("Unregister handler");
  MOZ_ASSERT(NS_IsMainThread(), "Wrong thread!");
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

  // Make sure 87.49999MHz would be rounded to the lower bound when
  // the lower bound is 87500KHz.
  if (aFrequencyInMHz < mLowerBoundInKHz / 1000.0 - halfChannelWidthInMHz ||
      aFrequencyInMHz > mUpperBoundInKHz / 1000.0 + halfChannelWidthInMHz) {
    return 0;
  }

  int32_t partToBeRounded = round(aFrequencyInMHz * 1000) - mLowerBoundInKHz;
  int32_t roundedPart = round(partToBeRounded / (double)mChannelWidthInKHz) *
                        mChannelWidthInKHz;

  return mLowerBoundInKHz + roundedPart;
}

bool
FMRadioService::IsEnabled() const
{
  MOZ_ASSERT(NS_IsMainThread(), "Wrong thread!");
  return IsFMRadioOn();
}

double
FMRadioService::GetFrequency() const
{
  MOZ_ASSERT(NS_IsMainThread(), "Wrong thread!");
  if (IsEnabled()) {
    int32_t frequencyInKHz = GetFMRadioFrequency();
    return frequencyInKHz / 1000.0;
  }

  return 0;
}

double
FMRadioService::GetFrequencyUpperBound() const
{
  MOZ_ASSERT(NS_IsMainThread(), "Wrong thread!");
  return mUpperBoundInKHz / 1000.0;
}

double
FMRadioService::GetFrequencyLowerBound() const
{
  MOZ_ASSERT(NS_IsMainThread(), "Wrong thread!");
  return mLowerBoundInKHz / 1000.0;
}

double
FMRadioService::GetChannelWidth() const
{
  MOZ_ASSERT(NS_IsMainThread(), "Wrong thread!");
  return mChannelWidthInKHz / 1000.0;
}

void
FMRadioService::Enable(double aFrequencyInMHz, ReplyRunnable* aReplyRunnable)
{
  MOZ_ASSERT(NS_IsMainThread(), "Wrong thread!");
  MOZ_ASSERT(aReplyRunnable);

  switch (mState) {
    case Seeking:
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
    case Disabled:
      break;
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
      NS_LITERAL_STRING("Airplane mode currently enabled")));
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
    if (NS_FAILED(rv)) {
      aReplyRunnable->SetReply(ErrorResponse(
        NS_LITERAL_STRING("Can't create settings lock")));
      NS_DispatchToMainThread(aReplyRunnable);
    }

    nsRefPtr<ReadRilSettingTask> callback = new ReadRilSettingTask();
    rv = settingsLock->Get(SETTING_KEY_RIL_RADIO_DISABLED, callback);
    if (NS_FAILED(rv)) {
      aReplyRunnable->SetReply(ErrorResponse(
        NS_LITERAL_STRING("Can't get settings value")));
      NS_DispatchToMainThread(aReplyRunnable);
    }

    return;
  }

  NS_DispatchToMainThread(new EnableRunnable(mUpperBoundInKHz,
                                             mLowerBoundInKHz,
                                             mChannelWidthInKHz));
}

void
FMRadioService::Disable(ReplyRunnable* aReplyRunnable)
{
  // When airplane-mode is enabled, we will call this function from
  // FMRadioService::Observe without passing a ReplyRunnable, so we have to
  // check if |aReplyRunnable| is null before we dispatch it.
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
          ErrorResponse(NS_LITERAL_STRING("FM radio currently disabled")));
        NS_DispatchToMainThread(aReplyRunnable);
      }
      return;
    case Enabled:
    case Enabling:
    case Seeking:
      break;
  }

  // If the FM Radio is currently seeking, no fail-to-seek or similar
  // event will be fired, execute the seek callback manually.
  if (mState == Seeking) {
    mPendingRequest->SetReply(ErrorResponse(
      NS_LITERAL_STRING("Seek action is cancelled")));
    NS_DispatchToMainThread(mPendingRequest);
  }

  nsRefPtr<ReplyRunnable> enablingRequest = mPendingRequest;
  FMRadioState preState = mState;
  SetState(Disabling);
  mPendingRequest = aReplyRunnable;

  if (preState == Enabling) {
    // If the radio is currently enabling, we fire the error callback on the
    // enable request immediately. When the radio finishes enabling, we'll call
    // DoDisable and fire the success callback on the disable request.
    LOG("FM radio currently enabling, fail it immediately.");
    enablingRequest->SetReply(
      ErrorResponse(NS_LITERAL_STRING("Seek action is cancelled")));
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
  // To make such codes work:
  //    navigator.mozFMRadio.disable();
  //    navigator.mozFMRadio.ondisabled = function() {
  //      console.log("We will catch disabled event ");
  //    };
  // we need to call hal::DisableFMRadio() asynchronously. Same reason for
  // EnableRunnable and SetFrequencyRunnable.
  NS_DispatchToMainThread(new DisableRunnable());
}

void
FMRadioService::SetFrequency(double aFrequencyInMHz,
                             ReplyRunnable* aReplyRunnable)
{
  LOG("Check Main Thread");
  MOZ_ASSERT(NS_IsMainThread(), "Wrong thread!");
  MOZ_ASSERT(aReplyRunnable);

  switch (mState) {
    case Disabled:
      LOG("It's Disabled");
      aReplyRunnable->SetReply(
        ErrorResponse(NS_LITERAL_STRING("FM radio currently disabled")));
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
    case Seeking:
      LOG("It's seeking, fail it immediately.");
      CancelFMRadioSeek();
      mPendingRequest->SetReply(
        ErrorResponse(NS_LITERAL_STRING("Seek action is cancelled")));
      NS_DispatchToMainThread(mPendingRequest);
      SetState(Enabled);
      break;
    case Enabled:
      break;
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
  MOZ_ASSERT(aReplyRunnable);

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
        ErrorResponse(NS_LITERAL_STRING("FM radio currently disabled")));
      NS_DispatchToMainThread(aReplyRunnable);
      return;
    case Seeking:
      LOG("It's Seeking");
      aReplyRunnable->SetReply(
        ErrorResponse(NS_LITERAL_STRING("FM radio currently seeking")));
      NS_DispatchToMainThread(aReplyRunnable);
      return;
    case Disabling:
      LOG("FM radio currently disabling");
      aReplyRunnable->SetReply(
        ErrorResponse(NS_LITERAL_STRING("FM radio currently disabling")));
      NS_DispatchToMainThread(aReplyRunnable);
      return;
    case Enabled:
      break;
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
  MOZ_ASSERT(aReplyRunnable);

  // We accept canceling seek request only if it's currently seeking.
  if (mState != Seeking) {
    LOG("It's not seeking");
    aReplyRunnable->SetReply(
      ErrorResponse(NS_LITERAL_STRING("FM radio currently not seeking")));
    NS_DispatchToMainThread(aReplyRunnable);
    return;
  }

  LOG("Cancel seeking immediately");
  // Cancel the seek immediately to prevent it from completing.
  CancelFMRadioSeek();

  LOG("Fail seeking request");
  mPendingRequest->SetReply(
    ErrorResponse(NS_LITERAL_STRING("Seek action is cancelled")));
  NS_DispatchToMainThread(mPendingRequest);

  SetState(Enabled);

  LOG("Fire success for cancel seeking");
  aReplyRunnable->SetReply(SuccessResponse());
  NS_DispatchToMainThread(aReplyRunnable);
}

NS_IMETHODIMP
FMRadioService::Observe(nsISupports * aSubject,
                        const char * aTopic,
                        const PRUnichar * aData)
{
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(sFMRadioService);

  if (strcmp(aTopic, MOZSETTINGS_CHANGED_ID)) {
    return NS_OK;
  }

  // The string that we're interested in will be a JSON string looks like:
  //  {"key":"ril.radio.disabled","value":true}
  AutoSafeJSContext cx;
  const nsDependentString dataStr(aData);
  JS::Rooted<JS::Value> val(cx);
  if (!JS_ParseJSON(cx, dataStr.get(), dataStr.Length(), &val) ||
      !val.isObject()) {
    return NS_OK;
  }

  JSObject& obj(val.toObject());
  JS::Rooted<JS::Value> key(cx);
  if (!JS_GetProperty(cx, &obj, "key", &key) ||
      !key.isString()) {
    return NS_OK;
  }

  JS::Rooted<JSString*> jsKey(cx, key.toString());
  nsDependentJSString keyStr;
  if (!keyStr.init(cx, jsKey)) {
    return NS_OK;
  }

  JS::Rooted<JS::Value> value(cx);
  if (!JS_GetProperty(cx, &obj, "value", &value)) {
    return NS_OK;
  }

  if (keyStr.EqualsLiteral(SETTING_KEY_RIL_RADIO_DISABLED)) {
    if (!value.isBoolean()) {
      return NS_OK;
    }

    mRilDisabled = value.toBoolean();
    mHasReadRilSetting = true;

    // Disable the FM radio HW if Airplane mode is enabled.
    if (mRilDisabled) {
      LOG("mRilDisabled is false, disable the FM right now");
      Disable(nullptr);
    }

    return NS_OK;
  }

  return NS_OK;
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
      LOG("FM HW is enabled.");
      MOZ_ASSERT(IsFMRadioOn());
      MOZ_ASSERT(mState == Disabling || mState == Enabling);

      // If we're disabling, disable the radio right now.
      if (mState == Disabling) {
        LOG("We need to disable FM Radio for the waiting request.");
        DoDisable();
        return;
      }

      // Fire success callback on the enable request.
      LOG("Fire Success for enable request.");
      mPendingRequest->SetReply(SuccessResponse());
      NS_DispatchToMainThread(mPendingRequest);

      SetState(Enabled);

      // To make sure the FM app will get the right frequency after the FM
      // radio is enabled, we have to set the frequency first.
      SetFMRadioFrequency(mPendingFrequencyInKHz);

      // Update the current frequency without sending the`FrequencyChanged`
      // event, to make sure the FM app will get the right frequency when the
      // `EnabledChange` event is sent.
      mPendingFrequencyInKHz = GetFMRadioFrequency();
      UpdatePowerState();

      // The frequency was changed from '0' to some meaningful number, so we
      // should send the `FrequencyChanged` event manually.
      NotifyFMRadioEvent(FrequencyChanged);
      break;
    case FM_RADIO_OPERATION_DISABLE:
      LOG("FM HW is disabled.");
      MOZ_ASSERT(mState == Disabling);

      if (mPendingRequest) {
        mPendingRequest->SetReply(SuccessResponse());
        NS_DispatchToMainThread(mPendingRequest);
      }

      SetState(Disabled);
      UpdatePowerState();
      break;
    case FM_RADIO_OPERATION_SEEK:
      LOG("FM HW seek complete.");

      // Seek action might be cancelled by SetFrequency(), we need to check if
      // the current state is Seeking.
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

NS_IMPL_ISUPPORTS1(FMRadioService, nsIObserver)

END_FMRADIO_NAMESPACE

