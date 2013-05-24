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
#include "mozilla/dom/fmradio/FMRadioService.h"
#include "mozilla/dom/fmradio/PFMRadioChild.h"

#undef LOG
#define LOG(args...) FM_LOG("FMRadio", args)

// The pref indicates if the device has an internal antenna.
// If the pref is true, the antanna will be always available.
#define DOM_FM_ANTENNA_INTERNAL_PREF "dom.fmradio.antenna.internal"

#define ENABLED_EVENT_NAME         NS_LITERAL_STRING("enabled")
#define DISABLED_EVENT_NAME        NS_LITERAL_STRING("disabled")
#define FREQUENCYCHANGE_EVENT_NAME NS_LITERAL_STRING("frequencychange")
#define ANTENNAAVAILABLECHANGE_EVENT_NAME \
  NS_LITERAL_STRING("antennaavailablechange")

using namespace mozilla::hal;
using mozilla::Preferences;

BEGIN_FMRADIO_NAMESPACE

FMRadio::FMRadio()
  : mHeadphoneState(SWITCH_STATE_OFF)
  , mHasInternalAntenna(false)
  , mFrequency(0)
  , mEnabled(false)
{
  LOG("FMRadio is initialized.");

  SetIsDOMBinding();
}

FMRadio::~FMRadio()
{
  LOG("FMRadio is destructed.");
}

class FMRadioRequest MOZ_FINAL : public ReplyRunnable
{
public:
  FMRadioRequest(DOMRequest* aRequest)
    : mRequest(aRequest) { }

  virtual ~FMRadioRequest() { }

  NS_DECL_CYCLE_COLLECTING_ISUPPORTS
  NS_DECL_CYCLE_COLLECTION_CLASS(FMRadioRequest)

  NS_IMETHOD CancelableRun()
  {
    switch (mResponseType.type())
    {
      case FMRadioResponseType::TErrorResponse:
      {
        ErrorResponse response = mResponseType;
        mRequest->FireError(response.error());
        break;
      }
      case FMRadioResponseType::TSuccessResponse:
      {
        // FIXME create a meaningfull result
        JS::Value result = JS_NumberValue(1);
        mRequest->FireSuccess(result);
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
};

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(FMRadioRequest)
  NS_INTERFACE_MAP_ENTRY(nsIRunnable)
NS_INTERFACE_MAP_END

NS_IMPL_CYCLE_COLLECTING_ADDREF(FMRadioRequest)
NS_IMPL_CYCLE_COLLECTING_RELEASE(FMRadioRequest)

NS_IMPL_CYCLE_COLLECTION_1(FMRadioRequest, mRequest)

void
FMRadio::Init(nsPIDOMWindow *aWindow)
{
  LOG("Init");
  BindToOwner(aWindow);

  LOG("Register Handler");
  FMRadioService::Get()->RegisterHandler(this);

  mHasInternalAntenna = Preferences::GetBool(DOM_FM_ANTENNA_INTERNAL_PREF,
                                             /* default = */ false);
  if (mHasInternalAntenna) {
    LOG("We have an internal antenna.");
  } else {
    mHeadphoneState = GetCurrentSwitchState(SWITCH_HEADPHONES);
    RegisterSwitchObserver(SWITCH_HEADPHONES, this);
  }

  // TODO update mEnabled/mFrequency
}

void
FMRadio::Shutdown()
{
  LOG("Shutdown, Unregister Handler");
  FMRadioService::Get()->UnregisterHandler(this);

  if (!mHasInternalAntenna) {
    UnregisterSwitchObserver(SWITCH_HEADPHONES, this);
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

    DispatchTrustedEvent(ANTENNAAVAILABLECHANGE_EVENT_NAME);
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

      mFrequency = event.frequency();
      DispatchTrustedEvent(FREQUENCYCHANGE_EVENT_NAME);
      break;
    }
    case FMRadioEventType::TStateChangedEvent:
    {
      StateChangedEvent event = aType;
      LOG("Power state is changed from %d to %d", mEnabled, event.enabled());

      mEnabled = event.enabled();
      mFrequency = event.frequency();

      if (mEnabled)
      {
        LOG("Fire onenabled");
        DispatchTrustedEvent(ENABLED_EVENT_NAME);
      }
      else
      {
        LOG("Fire ondisabled");
        DispatchTrustedEvent(DISABLED_EVENT_NAME);
      }
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
  return mEnabled;
}

bool
FMRadio::AntennaAvailable() const
{
  return mHasInternalAntenna ? true : mHeadphoneState != SWITCH_STATE_OFF;
}

Nullable<double>
FMRadio::GetFrequency() const
{
  return mEnabled ? (Nullable<double>)(mFrequency) : Nullable<double>();
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
  nsRefPtr<ReplyRunnable> r = new FMRadioRequest(request);

  FMRadioService::Get()->Enable(aFrequency, r);

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
  nsRefPtr<ReplyRunnable> r = new FMRadioRequest(request);

  FMRadioService::Get()->Disable(r);

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
  nsRefPtr<ReplyRunnable> r = new FMRadioRequest(request);

  FMRadioService::Get()->SetFrequency(aFrequency, r);

  return request.forget();
}

already_AddRefed<DOMRequest>
FMRadio::SeekUp()
{
  nsCOMPtr<nsPIDOMWindow> win = GetOwner();
  if (!win)
  {
    return nullptr;
  }

  nsRefPtr<DOMRequest> request = new DOMRequest(win);
  nsRefPtr<ReplyRunnable> r = new FMRadioRequest(request);

  FMRadioService::Get()->Seek(true, r);

  return request.forget();
}

already_AddRefed<DOMRequest>
FMRadio::SeekDown()
{

  nsCOMPtr<nsPIDOMWindow> win = GetOwner();
  if (!win)
  {
    return nullptr;
  }

  nsRefPtr<DOMRequest> request = new DOMRequest(win);
  nsRefPtr<ReplyRunnable> r = new FMRadioRequest(request);

  FMRadioService::Get()->Seek(false, r);

  return request.forget();
}

already_AddRefed<DOMRequest>
FMRadio::CancelSeek()
{
  nsCOMPtr<nsPIDOMWindow> win = GetOwner();
  if (!win)
  {
    return nullptr;
  }

  nsRefPtr<DOMRequest> request = new DOMRequest(win);
  nsRefPtr<ReplyRunnable> r = new FMRadioRequest(request);

  FMRadioService::Get()->CancelSeek(r);

  return request.forget();
}

END_FMRADIO_NAMESPACE
