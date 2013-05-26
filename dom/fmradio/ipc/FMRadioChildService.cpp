/* -*- Mode: c++; c-basic-offset: 2; indent-tabs-mode: nil; tab-width: 40 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "FMRadioChildService.h"
#include "FMRadioChild.h"
#include "mozilla/dom/ContentChild.h"
#include "mozilla/dom/fmradio/FMRadioChild.h"
#include "mozilla/dom/fmradio/FMRadioRequestChild.h"

#undef LOG
#define LOG(args...) FM_LOG("PFMRadioChildService", args)

BEGIN_FMRADIO_NAMESPACE

FMRadioChild*
FMRadioChildService::sFMRadioChild;

FMRadioChildService*
FMRadioChildService::sFMRadioChildService;

FMRadioEventObserverList*
FMRadioChildService::sChildEventObserverList;

FMRadioChildService::FMRadioChildService()
  : mEnabled(false)
  , mFrequency(0)
{
  LOG("Constructor");
}

FMRadioChildService::~FMRadioChildService()
{
  LOG("Destructor");
  sFMRadioChildService = nullptr;
  // The FMRadioChild object will be released in ContentChild::DeallocPFMRadio
  sFMRadioChild = nullptr;
  delete sChildEventObserverList;
  sChildEventObserverList = nullptr;
}

void
FMRadioChildService::Init()
{
  sFMRadioChild->SendIsEnabled(&mEnabled);
  sFMRadioChild->SendGetFrequency(&mFrequency);
}

bool FMRadioChildService::IsEnabled()
{
  return mEnabled;
}

double FMRadioChildService::GetFrequency()
{
  return mFrequency;
}

void FMRadioChildService::Enable(double aFrequency, ReplyRunnable* aRunnable)
{
  SendRequest(aRunnable, EnableRequest(aFrequency));
}

void FMRadioChildService::Disable(ReplyRunnable* aRunnable)
{
  SendRequest(aRunnable, DisableRequest());
}

void FMRadioChildService::SetFrequency(double aFrequency,
                                       ReplyRunnable* aRunnable)
{
  SendRequest(aRunnable, SetFrequencyRequest(aFrequency));
}

void FMRadioChildService::Seek(bool upward, ReplyRunnable* aRunnable)
{
  SendRequest(aRunnable, SeekRequest(upward));
}

void FMRadioChildService::CancelSeek(ReplyRunnable* aRunnable)
{
  SendRequest(aRunnable, CancelSeekRequest());
}

void
FMRadioChildService::DistributeEvent(const FMRadioEventType& aType)
{
  LOG("We have %d observer to broadcast the event", sChildEventObserverList->Length());

  switch(aType.type())
  {
    case FMRadioEventType::TFrequencyChangedEvent:
    {
      FrequencyChangedEvent event = aType;
      mFrequency = event.frequency();
      break;
    }
    case FMRadioEventType::TStateChangedEvent:
    {
      StateChangedEvent event = aType;
      mEnabled = event.enabled();
      mFrequency = event.frequency();
      break;
    }
    default:
      NS_RUNTIMEABORT("not reached");
      break;
  }

  sChildEventObserverList->Broadcast(aType);
}

void
FMRadioChildService::RegisterHandler(FMRadioEventObserver* aHandler)
{
  sChildEventObserverList->AddObserver(aHandler);
  LOG("Register observer, we have %d observers", sChildEventObserverList->Length());
}

void
FMRadioChildService::UnregisterHandler(FMRadioEventObserver* aHandler)
{
  LOG("Unregister observer");
  sChildEventObserverList->RemoveObserver(aHandler);

  if (sChildEventObserverList->Length() == 0)
  {
    LOG("NO handler anymore.");
    delete this;
  }
  else
  {
    LOG("We have %d observer left", sChildEventObserverList->Length());
  }
}

void
FMRadioChildService::SendRequest(ReplyRunnable* aReplyRunnable,
                                 FMRadioRequestType aType)
{
  PFMRadioRequestChild* child = new FMRadioRequestChild(aReplyRunnable);
  sFMRadioChild->SendPFMRadioRequestConstructor(child, aType);
  LOG("Request is sent.");
}

// static
FMRadioChildService*
FMRadioChildService::Get()
{
  MOZ_ASSERT(NS_IsMainThread());

  if (sFMRadioChildService) {
    LOG("Return cached sFMRadioChildService");
    return sFMRadioChildService;
  }

  LOG("Create sFMRadioChild");
  MOZ_ASSERT(!sFMRadioChild);

  LOG("Create sFMRadioChildService");
  sFMRadioChildService = new FMRadioChildService();

  LOG("Init sChildEventObserverList");
  sChildEventObserverList = new FMRadioEventObserverList();

  LOG("Init sFMRadioChild");
  sFMRadioChild = new FMRadioChild();
  ContentChild::GetSingleton()->SendPFMRadioConstructor(sFMRadioChild);

  sFMRadioChildService->Init();

  return sFMRadioChildService;
}

END_FMRADIO_NAMESPACE
