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

FMRadioChildService*
FMRadioChildService::sFMRadioChildService;

FMRadioChildService::FMRadioChildService()
  : mEnabled(false)
  , mFrequency(0)
  , mSettings(Settings())
{
  LOG("Constructor");
  mChildEventObserverList = new FMRadioEventObserverList();

  LOG("Init sFMRadioChild");
  mFMRadioChild = new FMRadioChild();
  ContentChild::GetSingleton()->SendPFMRadioConstructor(mFMRadioChild);
}

FMRadioChildService::~FMRadioChildService()
{
  LOG("Destructor");

  // The FMRadioChild object will be released in ContentChild::DeallocPFMRadio
  mFMRadioChild = nullptr;

  delete mChildEventObserverList;
  mChildEventObserverList = nullptr;
}

void
FMRadioChildService::Init()
{
  mFMRadioChild->SendIsEnabled(&mEnabled);
  mFMRadioChild->SendGetFrequency(&mFrequency);
  mFMRadioChild->SendGetSettings(&mSettings);
}

bool
FMRadioChildService::IsEnabled() const
{
  return mEnabled;
}

double
FMRadioChildService::GetFrequency() const
{
  return mFrequency;
}


double
FMRadioChildService::GetFrequencyUpperBound() const
{
  return mSettings.upperBound();
}

double
FMRadioChildService::GetFrequencyLowerBound() const
{
  return mSettings.lowerBound();
}

double
FMRadioChildService::GetChannelWidth() const
{
  return mSettings.channelWidth();
}

void
FMRadioChildService::Enable(double aFrequency, ReplyRunnable* aRunnable)
{
  SendRequest(aRunnable, EnableRequest(aFrequency));
}

void
FMRadioChildService::Disable(ReplyRunnable* aRunnable)
{
  SendRequest(aRunnable, DisableRequest());
}

void
FMRadioChildService::SetFrequency(double aFrequency,
                                       ReplyRunnable* aRunnable)
{
  SendRequest(aRunnable, SetFrequencyRequest(aFrequency));
}

void
FMRadioChildService::Seek(bool upward, ReplyRunnable* aRunnable)
{
  SendRequest(aRunnable, SeekRequest(upward));
}

void
FMRadioChildService::CancelSeek(ReplyRunnable* aRunnable)
{
  SendRequest(aRunnable, CancelSeekRequest());
}

void
FMRadioChildService::NotifyFrequencyChanged(double aFrequency)
{
  LOG("NotifyFrequencyChanged");
  mFrequency = aFrequency;
  mChildEventObserverList->Broadcast(FMRadioEventArgs(FrequencyChanged,
                                                      mEnabled,
                                                      mFrequency));
}

void
FMRadioChildService::NotifyEnabledChanged(bool aEnabled, double aFrequency)
{
  LOG("NotifyEnabledChanged");
  mEnabled = aEnabled;
  mFrequency = aFrequency;
  mChildEventObserverList->Broadcast(FMRadioEventArgs(EnabledChanged,
                                                      aEnabled,
                                                      aFrequency));
}

void
FMRadioChildService::RegisterHandler(FMRadioEventObserver* aHandler)
{
  mChildEventObserverList->AddObserver(aHandler);
  LOG("Register observer, we have %d observers", mChildEventObserverList->Length());
}

void
FMRadioChildService::UnregisterHandler(FMRadioEventObserver* aHandler)
{
  LOG("Unregister observer");
  mChildEventObserverList->RemoveObserver(aHandler);

  if (mChildEventObserverList->Length() == 0) {
    LOG("NO handler anymore.");
    sFMRadioChildService = nullptr;
    delete this;
  } else {
    LOG("We have %d observer left", mChildEventObserverList->Length());
  }
}

void
FMRadioChildService::SendRequest(ReplyRunnable* aReplyRunnable,
                                 FMRadioRequestType aType)
{
  PFMRadioRequestChild* child = new FMRadioRequestChild(aReplyRunnable);
  mFMRadioChild->SendPFMRadioRequestConstructor(child, aType);
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

  sFMRadioChildService->Init();

  return sFMRadioChildService;
}

END_FMRADIO_NAMESPACE
