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
#include "mozilla/dom/ContentChild.h"
#include "mozilla/dom/fmradio/FMRadioChildService.h"
#include "mozilla/dom/fmradio/PFMRadioChild.h"

#undef LOG
#define LOG(args...) FM_LOG("FMRadio", args)

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

BEGIN_FMRADIO_NAMESPACE

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
    mHeadphoneState = GetCurrentSwitchState(SWITCH_HEADPHONES);
    RegisterSwitchObserver(SWITCH_HEADPHONES, this);
  }
}

FMRadio::~FMRadio()
{
  LOG("FMRadio is destructed.");
  if (!mHasInternalAntenna) {
    UnregisterSwitchObserver(SWITCH_HEADPHONES, this);
  }
}

class FMRadioRequest MOZ_FINAL : public nsIRunnable
{
public:
  FMRadioRequest(DOMRequest* aRequest,
                 FMRadioRequestType aRequestType)
    : mRequest(aRequest)
    , mRequestType(aRequestType) {}

  virtual ~FMRadioRequest() {}

  NS_DECL_CYCLE_COLLECTING_ISUPPORTS
  NS_DECL_CYCLE_COLLECTION_CLASS(FMRadioRequest)

  NS_IMETHOD Run()
  {
    switch (mRequestType.type())
    {
      case FMRadioRequestType::TEnableRequest:
      case FMRadioRequestType::TDisableRequest:
      case FMRadioRequestType::TSetFrequencyRequest:
      case FMRadioRequestType::TCancelSeekRequest:
      case FMRadioRequestType::TSeekRequest:
      {
        LOG("FMRadioRequest: send request");
        FMRadioChildService::Get()->SendRequest(mRequest, mRequestType);
        break;
      }
      default:
        NS_RUNTIMEABORT("not reached");
        break;
    }

    return NS_OK;
  }

private:
  nsRefPtr<DOMRequest> mRequest;
  FMRadioRequestType mRequestType;
};

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(FMRadioRequest)
  NS_INTERFACE_MAP_ENTRY(nsIRunnable)
NS_INTERFACE_MAP_END

NS_IMPL_CYCLE_COLLECTING_ADDREF(FMRadioRequest)
NS_IMPL_CYCLE_COLLECTING_RELEASE(FMRadioRequest)

NS_IMPL_CYCLE_COLLECTION_1(FMRadioRequest, mRequest)

bool
IsMainProcess()
{
  return XRE_GetProcessType() == GeckoProcessType_Default;
}

void
FMRadio::Init(nsPIDOMWindow *aWindow)
{
  LOG("Init");
  BindToOwner(aWindow);

  if (!IsMainProcess()) {
    LOG("Register Handler");
    FMRadioChildService::Get()->RegisterHandler(this);
  } else {
    LOG("Only OOP is support now.");
  }
}

void
FMRadio::Shutdown()
{
  LOG("Shutdown");
  if (!IsMainProcess()) {
    LOG("Unregister Handler");
    FMRadioChildService::Get()->UnregisterHandler(this);
  } else {
    LOG("Only OOP is support now.");
  }
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
FMRadio::Notify(const FMRadioEventType& aType)
{
  switch(aType.type())
  {
    case FMRadioEventType::TFrequencyChangedEvent:
    {
      FrequencyChangedEvent event = aType;
      LOG("Frequency is changed to: %f", event.frequency());
      break;
    }
    case FMRadioEventType::TStateChangedEvent:
    {
      StateChangedEvent event = aType;
      LOG("Power state is changed to: %d", event.enabled());
      break;
    }
    default:
      MOZ_NOT_REACHED();
      break;
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
  nsRefPtr<nsIRunnable> r = new FMRadioRequest(request,
                                               EnableRequest(aFrequency));

  NS_DispatchToMainThread(r);

  return request.forget();
}

already_AddRefed<DOMRequest>
FMRadio::Disable()
{
  nsCOMPtr<nsPIDOMWindow> win = GetOwner();
  if (!win)
  {
    return nullptr;
  }

  nsRefPtr<DOMRequest> request = new DOMRequest(win);
  nsRefPtr<nsIRunnable> r = new FMRadioRequest(request,
                                               DisableRequest());
  NS_DispatchToMainThread(r);

  return request.forget();
}

already_AddRefed<DOMRequest>
FMRadio::SetFrequency(double aFrequency)
{
  nsCOMPtr<nsPIDOMWindow> win = GetOwner();
  if (!win)
  {
    return nullptr;
  }

  nsRefPtr<DOMRequest> request = new DOMRequest(win);
  nsRefPtr<nsIRunnable> r = new FMRadioRequest(request,
                                               SetFrequencyRequest(aFrequency));
  NS_DispatchToMainThread(r);

  return request.forget();
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

END_FMRADIO_NAMESPACE
