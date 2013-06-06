/* -*- Mode: c++; c-basic-offset: 2; indent-tabs-mode: nil; tab-width: 40 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "FMRadioChildService.h"
#include "mozilla/dom/ContentChild.h"
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
  MOZ_COUNT_CTOR(FMRadioChildService);
  mChildEventObserverList = new FMRadioEventObserverList();
}

FMRadioChildService::~FMRadioChildService()
{
  LOG("Destructor");
  MOZ_COUNT_DTOR(FMRadioChildService);

  delete mChildEventObserverList;
  mChildEventObserverList = nullptr;
}

void
FMRadioChildService::Init()
{
  ContentChild::GetSingleton()->SendPFMRadioConstructor(this);
  this->SendIsEnabled(&mEnabled);
  this->SendGetFrequency(&mFrequency);
  this->SendGetSettings(&mSettings);
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
FMRadioChildService::Seek(bool aUpward, ReplyRunnable* aRunnable)
{
  SendRequest(aRunnable, SeekRequest(aUpward));
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
FMRadioChildService::AddObserver(FMRadioEventObserver* aObserver)
{
  mChildEventObserverList->AddObserver(aObserver);
  LOG("Register observer, we have %d observers", mChildEventObserverList->Length());
}

void
FMRadioChildService::RemoveObserver(FMRadioEventObserver* aObserver)
{
  LOG("Unregister observer");
  mChildEventObserverList->RemoveObserver(aObserver);

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
                                 FMRadioRequestArgs aArgs)
{
  PFMRadioRequestChild* child = new FMRadioRequestChild(aReplyRunnable);
  this->SendPFMRadioRequestConstructor(child, aArgs);
  LOG("Request is sent.");
}

bool
FMRadioChildService::RecvNotifyFrequencyChanged(const double& aFrequency)
{
  LOG("RecvNotifyFrequencyChanged");
  this->NotifyFrequencyChanged(aFrequency);
  return true;
}

bool
FMRadioChildService::RecvNotifyEnabledChanged(const bool& aEnabled,
                                              const double& aFrequency)
{
  LOG("RecvEnabledChanged");
  this->NotifyEnabledChanged(aEnabled, aFrequency);
  return true;
}

bool
FMRadioChildService::Recv__delete__()
{
  LOG("Recv__delete__");
  return true;
}

PFMRadioRequestChild*
FMRadioChildService::AllocPFMRadioRequest(const FMRadioRequestArgs& aArgs)
{
  MOZ_NOT_REACHED("Caller is supposed to manually construct a request");
  return nullptr;
}

bool
FMRadioChildService::DeallocPFMRadioRequest(PFMRadioRequestChild* aActor)
{
  delete aActor;
  return true;
}

// static
FMRadioChildService*
FMRadioChildService::Singleton()
{
  MOZ_ASSERT(NS_IsMainThread());

  if (sFMRadioChildService) {
    LOG("Return cached sFMRadioChildService");
    return sFMRadioChildService;
  }

  LOG("Create sFMRadioChildService");
  sFMRadioChildService = new FMRadioChildService();

  sFMRadioChildService->Init();

  return sFMRadioChildService;
}

END_FMRADIO_NAMESPACE
