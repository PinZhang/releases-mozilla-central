/* -*- Mode: c++; c-basic-offset: 2; indent-tabs-mode: nil; tab-width: 40 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "FMRadioChild.h"
#include "mozilla/dom/ContentChild.h"
#include "mozilla/dom/fmradio/FMRadioRequestChild.h"

#undef LOG
#define LOG(args...) FM_LOG("FMRadioChild", args)

BEGIN_FMRADIO_NAMESPACE

StaticAutoPtr<FMRadioChild> FMRadioChild::sFMRadioChild;

FMRadioChild::FMRadioChild()
  : mEnabled(false)
  , mFrequency(0)
  , mObserverList(FMRadioEventObserverList())
{
  LOG("Constructor");
  MOZ_COUNT_CTOR(FMRadioChild);

  ContentChild::GetSingleton()->SendPFMRadioConstructor(this);

  StatusInfo statusInfo;
  SendGetStatusInfo(&statusInfo);

  mEnabled = statusInfo.enabled();
  mFrequency = statusInfo.frequency();
  mUpperBound = statusInfo.upperBound();
  mLowerBound= statusInfo.lowerBound();
  mChannelWidth = statusInfo.channelWidth();
}

FMRadioChild::~FMRadioChild()
{
  LOG("Destructor");
  MOZ_COUNT_DTOR(FMRadioChild);
}

bool
FMRadioChild::IsEnabled() const
{
  return mEnabled;
}

double
FMRadioChild::GetFrequency() const
{
  return mFrequency;
}


double
FMRadioChild::GetFrequencyUpperBound() const
{
  return mUpperBound;
}

double
FMRadioChild::GetFrequencyLowerBound() const
{
  return mLowerBound;
}

double
FMRadioChild::GetChannelWidth() const
{
  return mChannelWidth;
}

void
FMRadioChild::Enable(double aFrequency, ReplyRunnable* aRunnable)
{
  SendRequest(aRunnable, EnableRequestArgs(aFrequency));
}

void
FMRadioChild::Disable(ReplyRunnable* aRunnable)
{
  SendRequest(aRunnable, DisableRequestArgs());
}

void
FMRadioChild::SetFrequency(double aFrequency,
                                  ReplyRunnable* aRunnable)
{
  SendRequest(aRunnable, SetFrequencyRequestArgs(aFrequency));
}

void
FMRadioChild::Seek(bool aUpward, ReplyRunnable* aRunnable)
{
  SendRequest(aRunnable, SeekRequestArgs(aUpward));
}

void
FMRadioChild::CancelSeek(ReplyRunnable* aRunnable)
{
  SendRequest(aRunnable, CancelSeekRequestArgs());
}

inline void
FMRadioChild::NotifyFMRadioEvent(FMRadioEventType aType)
{
  LOG("Notify FMRadioEvent");
  mObserverList.Broadcast(aType);
}

void
FMRadioChild::AddObserver(FMRadioEventObserver* aObserver)
{
  mObserverList.AddObserver(aObserver);
  LOG("Register observer, we have %d observers", mObserverList.Length());
}

void
FMRadioChild::RemoveObserver(FMRadioEventObserver* aObserver)
{
  LOG("Unregister observer");
  mObserverList.RemoveObserver(aObserver);
}

void
FMRadioChild::SendRequest(ReplyRunnable* aReplyRunnable,
                                 FMRadioRequestArgs aArgs)
{
  PFMRadioRequestChild* childRequest = new FMRadioRequestChild(aReplyRunnable);
  SendPFMRadioRequestConstructor(childRequest, aArgs);
  LOG("Request is sent.");
}

bool
FMRadioChild::RecvNotifyFrequencyChanged(const double& aFrequency)
{
  LOG("RecvNotifyFrequencyChanged");
  mFrequency = aFrequency;
  NotifyFMRadioEvent(FrequencyChanged);
  return true;
}

bool
FMRadioChild::RecvNotifyEnabledChanged(const bool& aEnabled,
                                       const double& aFrequency)
{
  LOG("RecvEnabledChanged");
  mEnabled = aEnabled;
  mFrequency = aFrequency;
  NotifyFMRadioEvent(EnabledChanged);
  return true;
}

bool
FMRadioChild::Recv__delete__()
{
  LOG("Recv__delete__");
  return true;
}

PFMRadioRequestChild*
FMRadioChild::AllocPFMRadioRequest(const FMRadioRequestArgs& aArgs)
{
  NS_RUNTIMEABORT("Caller is supposed to manually construct a request");
  return nullptr;
}

bool
FMRadioChild::DeallocPFMRadioRequest(PFMRadioRequestChild* aActor)
{
  delete aActor;
  return true;
}

// static
FMRadioChild*
FMRadioChild::Singleton()
{
  MOZ_ASSERT(NS_IsMainThread());

  if (!sFMRadioChild) {
    LOG("Create sFMRadioChild");
    sFMRadioChild = new FMRadioChild();
  }

  return sFMRadioChild;
}

END_FMRADIO_NAMESPACE

