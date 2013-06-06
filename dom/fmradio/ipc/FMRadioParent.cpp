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

  FMRadioService::Singleton()->AddObserver(this);
}

FMRadioParent::~FMRadioParent()
{
  LOG("destructor");
  MOZ_COUNT_DTOR(FMRadioParent);

  FMRadioService::Singleton()->RemoveObserver(this);
}

bool
FMRadioParent::RecvGetStatusInfo(StatusInfo* aStatusInfo)
{
  aStatusInfo->enabled() = FMRadioService::Singleton()->IsEnabled();
  aStatusInfo->frequency() = FMRadioService::Singleton()->GetFrequency();
  aStatusInfo->upperBound() =
    FMRadioService::Singleton()->GetFrequencyUpperBound();
  aStatusInfo->lowerBound() =
    FMRadioService::Singleton()->GetFrequencyLowerBound();
  aStatusInfo->channelWidth() =
    FMRadioService::Singleton()->GetChannelWidth();
  return true;
}

PFMRadioRequestParent*
FMRadioParent::AllocPFMRadioRequest(const FMRadioRequestArgs& aArgs)
{
  LOG("AllocPFMRadioRequest");
  nsRefPtr<FMRadioRequestParent> requestParent = new FMRadioRequestParent();

  switch (aArgs.type()) {
    case FMRadioRequestArgs::TEnableRequest:
      LOG("Call Enable");
      FMRadioService::Singleton()->Enable(
        aArgs.get_EnableRequest().frequency(), requestParent);
      break;
    case FMRadioRequestArgs::TDisableRequest:
      LOG("Call Disable");
      FMRadioService::Singleton()->Disable(requestParent);
      break;
    case FMRadioRequestArgs::TSetFrequencyRequest:
      LOG("Call SetFrequency");
      FMRadioService::Singleton()->SetFrequency(
        aArgs.get_SetFrequencyRequest().frequency(), requestParent);
      break;
    case FMRadioRequestArgs::TSeekRequest:
      FMRadioService::Singleton()->Seek(
        aArgs.get_SeekRequest().upward(), requestParent);
      break;
    case FMRadioRequestArgs::TCancelSeekRequest:
      LOG("Call CancelSeek");
      FMRadioService::Singleton()->CancelSeek(requestParent);
      break;
    default:
      NS_RUNTIMEABORT("not reached");
      break;
  }

  return requestParent.forget().get();
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
FMRadioParent::Notify(const FMRadioEventArgs& aArgs)
{
  switch (aArgs.type) {
    case FrequencyChanged:
      unused << this->SendNotifyFrequencyChanged(aArgs.frequency);
      break;
    case EnabledChanged:
      unused << this->SendNotifyEnabledChanged(aArgs.enabled, aArgs.frequency);
      break;
    default:
      NS_RUNTIMEABORT("not reached");
      break;
  }
}

END_FMRADIO_NAMESPACE
