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
#include "mozilla/dom/fmradio/PFMRadioChild.h"
#include "nsIPermissionManager.h"

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
  , mIsShutdown(false)
{
  LOG("FMRadio is initialized.");

  SetIsDOMBinding();
}

FMRadio::~FMRadio()
{
  LOG("FMRadio is destructed.");
}

void
FMRadio::Init(nsPIDOMWindow *aWindow)
{
  LOG("Init");
  BindToOwner(aWindow);

  LOG("Register Handler");
  FMRadioService::Singleton()->AddObserver(this);

  mHasInternalAntenna = Preferences::GetBool(DOM_FM_ANTENNA_INTERNAL_PREF,
                                             /* default = */ false);
  if (mHasInternalAntenna) {
    LOG("We have an internal antenna.");
  } else {
    mHeadphoneState = GetCurrentSwitchState(SWITCH_HEADPHONES);
    RegisterSwitchObserver(SWITCH_HEADPHONES, this);
  }
}

void
FMRadio::Shutdown()
{
  LOG("Shutdown, Unregister Handler");
  FMRadioService::Singleton()->RemoveObserver(this);

  if (!mHasInternalAntenna) {
    LOG("Unregister SWITCH_HEADPHONES observer.");
    UnregisterSwitchObserver(SWITCH_HEADPHONES, this);
  }

  mIsShutdown = true;
}

JSObject*
FMRadio::WrapObject(JSContext* aCx, JS::Handle<JSObject*> aScope)
{
  return FMRadioBinding::Wrap(aCx, aScope, this);
}

void
FMRadio::Notify(const SwitchEvent& aEvent)
{
  MOZ_ASSERT(!mHasInternalAntenna);

  if (mHeadphoneState != aEvent.status()) {
    LOG("Antenna state is changed!");
    mHeadphoneState = aEvent.status();

    DispatchTrustedEvent(ANTENNAAVAILABLECHANGE_EVENT_NAME);
  }
}

void
FMRadio::Notify(const FMRadioEventType& aType)
{
  switch (aType) {
    case FrequencyChanged:
      DispatchTrustedEvent(FREQUENCYCHANGE_EVENT_NAME);
      break;
    case EnabledChanged:
      if (FMRadioService::Singleton()->IsEnabled()) {
        LOG("Fire onenabled");
        DispatchTrustedEvent(ENABLED_EVENT_NAME);
      } else {
        LOG("Fire ondisabled");
        DispatchTrustedEvent(DISABLED_EVENT_NAME);
      }
      break;
    default:
      MOZ_CRASH();
      break;
  }
}

bool
FMRadio::Enabled() const
{
  return FMRadioService::Singleton()->IsEnabled();
}

bool
FMRadio::AntennaAvailable() const
{
  return mHasInternalAntenna ? true : mHeadphoneState != SWITCH_STATE_OFF;
}

Nullable<double>
FMRadio::GetFrequency() const
{
  return Enabled() ? Nullable<double>(FMRadioService::Singleton()->GetFrequency())
                   : Nullable<double>();
}

double
FMRadio::FrequencyUpperBound() const
{
  return FMRadioService::Singleton()->GetFrequencyUpperBound();
}

double
FMRadio::FrequencyLowerBound() const
{
  return FMRadioService::Singleton()->GetFrequencyLowerBound();
}

double
FMRadio::ChannelWidth() const
{
  return FMRadioService::Singleton()->GetChannelWidth();
}

already_AddRefed<DOMRequest>
FMRadio::Enable(double aFrequency)
{
  nsCOMPtr<nsPIDOMWindow> win = GetOwner();
  if (!win) {
    return nullptr;
  }

  // |FMRadio| inherits from |nsIDOMEventTarget| and |nsISupportsWeakReference|
  // which both inherits from nsISupports, so |nsISupports| is an ambiguous
  // base of |FMRadio|, we have to cast |this| to one of the base classes.
  nsWeakPtr weakFMRadio = do_GetWeakReference(
    static_cast<nsIDOMEventTarget*>(this));
  nsRefPtr<FMRadioRequest> r = new FMRadioRequest(win, weakFMRadio);
  FMRadioService::Singleton()->Enable(aFrequency, r);

  return r.forget();
}

already_AddRefed<DOMRequest>
FMRadio::Disable()
{
  nsCOMPtr<nsPIDOMWindow> win = GetOwner();
  if (!win) {
    return nullptr;
  }

  nsWeakPtr weakFMRadio = do_GetWeakReference(
    static_cast<nsIDOMEventTarget*>(this));
  nsRefPtr<FMRadioRequest> r = new FMRadioRequest(win, weakFMRadio);
  FMRadioService::Singleton()->Disable(r);

  return r.forget();
}

already_AddRefed<DOMRequest>
FMRadio::SetFrequency(double aFrequency)
{
  nsCOMPtr<nsPIDOMWindow> win = GetOwner();
  if (!win) {
    return nullptr;
  }

  nsWeakPtr weakFMRadio = do_GetWeakReference(
    static_cast<nsIDOMEventTarget*>(this));
  nsRefPtr<FMRadioRequest> r = new FMRadioRequest(win, weakFMRadio);
  FMRadioService::Singleton()->SetFrequency(aFrequency, r);

  return r.forget();
}

already_AddRefed<DOMRequest>
FMRadio::SeekUp()
{
  nsCOMPtr<nsPIDOMWindow> win = GetOwner();
  if (!win) {
    return nullptr;
  }

  nsWeakPtr weakFMRadio = do_GetWeakReference(
    static_cast<nsIDOMEventTarget*>(this));
  nsRefPtr<FMRadioRequest> r = new FMRadioRequest(win, weakFMRadio);
  FMRadioService::Singleton()->Seek(true, r);

  return r.forget();
}

already_AddRefed<DOMRequest>
FMRadio::SeekDown()
{
  nsCOMPtr<nsPIDOMWindow> win = GetOwner();
  if (!win) {
    return nullptr;
  }

  nsWeakPtr weakFMRadio = do_GetWeakReference(
    static_cast<nsIDOMEventTarget*>(this));
  nsRefPtr<FMRadioRequest> r = new FMRadioRequest(win, weakFMRadio);
  FMRadioService::Singleton()->Seek(false, r);

  return r.forget();
}

already_AddRefed<DOMRequest>
FMRadio::CancelSeek()
{
  nsCOMPtr<nsPIDOMWindow> win = GetOwner();
  if (!win) {
    return nullptr;
  }

  nsWeakPtr weakFMRadio = do_GetWeakReference(
    static_cast<nsIDOMEventTarget*>(this));
  nsRefPtr<FMRadioRequest> r = new FMRadioRequest(win, weakFMRadio);
  FMRadioService::Singleton()->CancelSeek(r);

  return r.forget();
}

// static
already_AddRefed<FMRadio>
FMRadio::CheckPermissionAndCreateInstance(nsPIDOMWindow* aWindow)
{
  nsCOMPtr<nsIPermissionManager> permMgr =
    do_GetService(NS_PERMISSIONMANAGER_CONTRACTID);
  NS_ENSURE_TRUE(permMgr, nullptr);

  uint32_t permission = nsIPermissionManager::DENY_ACTION;
  permMgr->TestPermissionFromWindow(aWindow, "fmradio", &permission);

  if (permission != nsIPermissionManager::ALLOW_ACTION) {
    return nullptr;
  }

  nsRefPtr<FMRadio> fmRadio = new FMRadio();
  fmRadio->Init(aWindow);

  return fmRadio.forget();
}

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION_INHERITED(FMRadio)
  NS_INTERFACE_MAP_ENTRY(nsISupportsWeakReference)
NS_INTERFACE_MAP_END_INHERITING(nsDOMEventTargetHelper)

NS_IMPL_ADDREF_INHERITED(FMRadio, nsDOMEventTargetHelper)
NS_IMPL_RELEASE_INHERITED(FMRadio, nsDOMEventTargetHelper)

NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN_INHERITED(FMRadioRequest, DOMRequest)
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END

NS_IMPL_CYCLE_COLLECTION_UNLINK_BEGIN_INHERITED(FMRadioRequest, DOMRequest)
NS_IMPL_CYCLE_COLLECTION_UNLINK_END

NS_IMPL_ISUPPORTS_INHERITED0(FMRadioRequest, DOMRequest)

END_FMRADIO_NAMESPACE

