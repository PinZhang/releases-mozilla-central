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

FMRadioChild* gFMRadioChild = nullptr;
FMRadioChildService* gFMRadioChildService = nullptr;
FMRadioEventObserverList* gObserverList = nullptr;

FMRadioChildService::FMRadioChildService()
{
  LOG("Constructor");
}

FMRadioChildService::~FMRadioChildService()
{
  LOG("Destructor");
  gFMRadioChildService = nullptr;
  // The FMRadioChild object will be released in ContentChild::DeallocPFMRadio
  gFMRadioChild = nullptr;
  gObserverList = nullptr;
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
  LOG("We have %d observer to broadcast the event", gObserverList->Length());
  gObserverList->Broadcast(aType);
}

void
FMRadioChildService::RegisterHandler(FMRadioEventObserver* aHandler)
{
  gObserverList->AddObserver(aHandler);
  LOG("Register observer, we have %d observers", gObserverList->Length());
}

void
FMRadioChildService::UnregisterHandler(FMRadioEventObserver* aHandler)
{
  LOG("Unregister observer");
  gObserverList->RemoveObserver(aHandler);

  if (gObserverList->Length() == 0)
  {
    LOG("NO handler anymore.");
    delete this;
  }
  else
  {
    LOG("We have %d observer left", gObserverList->Length());
  }
}

void
FMRadioChildService::SendRequest(ReplyRunnable* aReplyRunnable,
                                 FMRadioRequestType aType)
{
  PFMRadioRequestChild* child = new FMRadioRequestChild(aReplyRunnable);
  gFMRadioChild->SendPFMRadioRequestConstructor(child, aType);
  LOG("Request is sent.");
}

// static
FMRadioChildService*
FMRadioChildService::Get()
{
  MOZ_ASSERT(NS_IsMainThread());

  if (gFMRadioChildService) {
    LOG("Return cached gFMRadioChildService");
    return gFMRadioChildService;
  }

  LOG("Create gFMRadioChild");
  MOZ_ASSERT(!gFMRadioChild);

  LOG("Create gFMRadioChildService");
  gFMRadioChildService = new FMRadioChildService();

  LOG("Init gObserverList");
  gObserverList = new FMRadioEventObserverList();

  LOG("Init gFMRadioChild");
  gFMRadioChild = new FMRadioChild();
  ContentChild::GetSingleton()->SendPFMRadioConstructor(gFMRadioChild);

  return gFMRadioChildService;
}

END_FMRADIO_NAMESPACE
