/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "FMRadioParent.h"
#include "mozilla/dom/ContentParent.h"
#include "FMRadioRequestParent.h"
#include "FMRadioService.h"

#undef LOG
#define LOG(args...) FM_LOG("PFMRadioParent", args)

// The pref indicates if the device has an internal antenna.
// If the pref is true, the antanna will be always available.
#define DOM_FM_ANTENNA_INTERNAL_PREF "dom.fmradio.antenna.internal"

BEGIN_FMRADIO_NAMESPACE

FMRadioParent::FMRadioParent()
{
  LOG("constructor");
  MOZ_COUNT_CTOR(FMRadioParent);

  FMRadioService::Get()->RegisterHandler(this);
}

FMRadioParent::~FMRadioParent()
{
  LOG("destructor");
  MOZ_COUNT_DTOR(FMRadioParent);

  FMRadioService::Get()->UnregisterHandler(this);
}

void
FMRadioParent::ActorDestroy(ActorDestroyReason aWhy)
{
  LOG("ActorDestroy");
  return;
}

bool
FMRadioParent::RecvIsEnabled(bool* aEnabled)
{
  *aEnabled = FMRadioService::Get()->IsEnabled();
  return true;
}

bool
FMRadioParent::RecvGetFrequency(double* aFrequency)
{
  *aFrequency = FMRadioService::Get()->GetFrequency();
  return true;
}

PFMRadioRequestParent*
FMRadioParent::AllocPFMRadioRequest(const FMRadioRequestType& aRequestType)
{
  LOG("AllocPFMRadioRequest");
  nsRefPtr<FMRadioRequestParent> result = new FMRadioRequestParent(aRequestType);
  result->Dispatch();
  return result.forget().get();
}

bool
FMRadioParent::DeallocPFMRadioRequest(PFMRadioRequestParent* aActor)
{
  LOG("DeallocPFMRadioRequest");
  FMRadioRequestParent* parent = static_cast<FMRadioRequestParent*>(aActor);
  NS_RELEASE(parent);
  return true;
}

void
FMRadioParent::Notify(const FMRadioEventType& aType)
{
  switch(aType.type())
  {
    case FMRadioEventType::TStateChangedEvent:
    case FMRadioEventType::TFrequencyChangedEvent:
    {
      unused << this->SendNotify(aType);
      break;
    }
    default:
      NS_RUNTIMEABORT("not reached");
      break;
  }
}

END_FMRADIO_NAMESPACE
