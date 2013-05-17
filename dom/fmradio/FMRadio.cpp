/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "FMRadio.h"
#include "nsContentUtils.h"
#include "mozilla/Hal.h"
#include "mozilla/HalTypes.h"
#include "mozilla/Preferences.h"
#include "mozilla/dom/FMRadioBinding.h"
#include "DOMRequest.h"
#include "mozilla/dom/ContentChild.h"
#include "mozilla/dom/fmradio/PFMRadioRequestChild.h"
#include "mozilla/dom/fmradio/FMRadioRequestChild.h"

#undef LOG
#if defined(MOZ_WIDGET_GONK)
#include <android/log.h>
#define LOG(args...)  __android_log_print(ANDROID_LOG_INFO, "FMRadio" , ## args)
#else
#define LOG(args...)
#endif

// The pref indicates if the device has an internal antenna.
// If the pref is true, the antanna will be always available.
#define DOM_FM_ANTENNA_INTERNAL_PREF "dom.fmradio.antenna.internal"

#define ENABLED_EVENT_NAME         NS_LITERAL_STRING("enabled");
#define DISABLED_EVENT_NAME        NS_LITERAL_STRING("disabled");
#define FREQUENCYCHANGE_EVENT_NAME NS_LITERAL_STRING("frequencychange");
#define ANTENNAAVAILABLECHANGE_EVENT_NAME \
  NS_LITERAL_STRING("antennaavailablechange");

using namespace mozilla::hal;
using mozilla::Preferences;

namespace mozilla {
namespace dom {
namespace fmradio {

FMRadio::FMRadio()
  : mHeadphoneState(SWITCH_STATE_OFF)
  , mHasInternalAntenna(false)
{
  LOG("FMRadio is initialized.");

  SetIsDOMBinding();

  mHasInternalAntenna = Preferences::GetBool(DOM_FM_ANTENNA_INTERNAL_PREF,
                                             /* default = */ false);
  if (mHasInternalAntenna) {
    LOG("We have an internal antenna.");
  } else {
    RegisterSwitchObserver(SWITCH_HEADPHONES, this);
    mHeadphoneState = GetCurrentSwitchState(SWITCH_HEADPHONES);
  }

  RegisterFMRadioObserver(this);
}

FMRadio::~FMRadio()
{
  UnregisterFMRadioObserver(this);
  if (!mHasInternalAntenna) {
    UnregisterSwitchObserver(SWITCH_HEADPHONES, this);
  }
}

class FMRadioRequest MOZ_FINAL : public nsIRunnable
{
public:
  FMRadioRequest(const FMRadioRequestType aRequestType,
                 DOMRequest* aRequest,
                 double aFrequency = 0)
    : mRequestType(aRequestType)
    , mRequest(aRequest)
    , mSeekUpward(false)
    , mFrequency(aFrequency) {}

  FMRadioRequest(const FMRadioRequestType aRequestType,
                 DOMRequest* aRequest,
                 bool aSeekUpward)
    : mRequestType(aRequestType)
    , mRequest(aRequest)
    , mSeekUpward(aSeekUpward)
    , mFrequency(0) {}

  ~FMRadioRequest() {}

  NS_DECL_CYCLE_COLLECTING_ISUPPORTS

  NS_IMETHOD Run()
  {
    switch (mRequestType)
    {
      case FMRADIO_REQUEST_ENABLE:
      {
        LOG("Call enable request.");
        PFMRadioRequestChild* child = new FMRadioRequestChild();
        FMRadioRequestEnableParams params;
        ContentChild::GetSingleton()->SendPFMRadioRequestConstructor(
          child, params);
        break;
      }

    }

    LOG("FMRadioRequest: run request");
    return NS_OK;
  }

private:
  int32_t mRequestType;
  nsRefPtr<DOMRequest> mRequest;
  bool mSeekUpward;
  double mFrequency;
};

void
FMRadio::Init(nsPIDOMWindow *aWindow)
{
  BindToOwner(aWindow);
}

void
FMRadio::Shutdown()
{

}

JSObject*
FMRadio::WrapObject(JSContext* aCx, JS::Handle<JSObject*> aScope)
{
  return FMRadioBinding::Wrap(aCx, aScope, this);
}

void
FMRadio::Notify(const SwitchEvent& aEvent)
{
  if (mHeadphoneState != aEvent.status()) {
    LOG("Antenna state is changed!");
    mHeadphoneState = aEvent.status();
  }
}

void
FMRadio::Notify(const FMRadioOperationInformation& info)
{
  switch (info.operation())
  {
    case FM_RADIO_OPERATION_ENABLE:
      LOG("FM HW is enabled.");
      break;
    case FM_RADIO_OPERATION_DISABLE:
      LOG("FM HW is disabled.");
      break;
    case FM_RADIO_OPERATION_SEEK:
      LOG("FM HW seek complete.");
      break;
    default:
      MOZ_NOT_REACHED();
      return;
  }
}

bool
FMRadio::Enabled() const
{
  return IsFMRadioOn();
}

bool
FMRadio::AntennaAvailable() const
{
  return mHasInternalAntenna ? true : mHeadphoneState != SWITCH_STATE_OFF;
}

Nullable<double>
FMRadio::GetFrequency() const
{
  return IsFMRadioOn() ? (Nullable<double>)(GetFMRadioFrequency() / 1000)
                       : Nullable<double>();
}

double
FMRadio::FrequencyUpperBound() const
{
  return 108.0;
}

double
FMRadio::FrequencyLowerBound() const
{
  return 87.5;
}

double
FMRadio::ChannelWidth() const
{
  return 0.5;
}

already_AddRefed<DOMRequest>
FMRadio::Enable(double aFrequency)
{
  nsCOMPtr<nsPIDOMWindow> win = GetOwner();
  if (!win)
  {
    return nullptr;
  }

  nsRefPtr<DOMRequest> request = new DOMRequest(win);
  nsRefPtr<nsIRunnable> r = new FMRadioRequest(
                              FMRADIO_REQUEST_ENABLE,
                              request,
                              aFrequency);

  NS_DispatchToMainThread(r);

  return request.forget();
}

already_AddRefed<DOMRequest>
FMRadio::Disable()
{
  return nullptr;
}

already_AddRefed<DOMRequest>
FMRadio::SetFrequency(double aFrequency)
{
  return nullptr;
}

already_AddRefed<DOMRequest>
FMRadio::SeekUp()
{
  return nullptr;
}

already_AddRefed<DOMRequest>
FMRadio::SeekDown()
{
  return NULL;
}

already_AddRefed<DOMRequest>
FMRadio::CancelSeek()
{
  return NULL;
}

} // namespace fmradio
} // namespace dom
} // namespace mozilla

