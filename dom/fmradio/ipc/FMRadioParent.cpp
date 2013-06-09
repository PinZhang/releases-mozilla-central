/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "FMRadioParent.h"
#include "mozilla/unused.h"
#include "mozilla/dom/ContentParent.h"
#include "FMRadioRequestParent.h"
#include "FMRadioService.h"

#undef LOG
#define LOG(args...) FM_LOG("PFMRadioParent", args)

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
    case FMRadioRequestArgs::TEnableRequestArgs:
      LOG("Call Enable");
      FMRadioService::Singleton()->Enable(
        aArgs.get_EnableRequestArgs().frequency(), requestParent);
      break;
    case FMRadioRequestArgs::TDisableRequestArgs:
      LOG("Call Disable");
      FMRadioService::Singleton()->Disable(requestParent);
      break;
    case FMRadioRequestArgs::TSetFrequencyRequestArgs:
      LOG("Call SetFrequency");
      FMRadioService::Singleton()->SetFrequency(
        aArgs.get_SetFrequencyRequestArgs().frequency(), requestParent);
      break;
    case FMRadioRequestArgs::TSeekRequestArgs:
      FMRadioService::Singleton()->Seek(
        aArgs.get_SeekRequestArgs().upward(), requestParent);
      break;
    case FMRadioRequestArgs::TCancelSeekRequestArgs:
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
FMRadioParent::Notify(const FMRadioEventType& aType)
{
  switch (aType) {
    case FrequencyChanged:
      unused << SendNotifyFrequencyChanged(
        FMRadioService::Singleton()->GetFrequency());
      break;
    case EnabledChanged:
      unused << SendNotifyEnabledChanged(
        FMRadioService::Singleton()->IsEnabled(),
        FMRadioService::Singleton()->GetFrequency());
      break;
    default:
      NS_RUNTIMEABORT("not reached");
      break;
  }
}

END_FMRADIO_NAMESPACE

