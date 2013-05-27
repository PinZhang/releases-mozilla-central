/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "FMRadioRequestParent.h"
#include "FMRadioService.h"

#undef LOG
#define LOG(args...) FM_LOG("PFMRadioRequestParent", args)

BEGIN_FMRADIO_NAMESPACE

FMRadioRequestParent::FMRadioRequestParent(const FMRadioRequestType& aType)
  : mRequestType(aType)
  , mMutex("FMRadioRequestParent::mMutex")
  , mActorDestroyed(false)
{
  LOG("constructor");
  MOZ_COUNT_CTOR(FMRadioRequestParent);
}

void
FMRadioRequestParent::Dispatch()
{
  switch (mRequestType.type())
  {
    case FMRadioRequestType::TEnableRequest:
    {
      LOG("Call Enable");
      EnableRequest request = mRequestType;
      nsRefPtr<ReplyRunnable> replyRunnable = new ParentReplyRunnable(this);
      FMRadioService::Get()->Enable(request.frequency(),
                                    replyRunnable.forget().get());
      break;
    }
    case FMRadioRequestType::TDisableRequest:
    {
      LOG("Call Disable");
      nsRefPtr<ReplyRunnable> replyRunnable = new ParentReplyRunnable(this);
      FMRadioService::Get()->Disable(replyRunnable.forget().get());
      break;
    }
    case FMRadioRequestType::TSetFrequencyRequest:
    {
      LOG("Call SetFrequency");
      SetFrequencyRequest request = mRequestType;
      nsRefPtr<ReplyRunnable> replyRunnable = new ParentReplyRunnable(this);
      FMRadioService::Get()->SetFrequency(request.frequency(),
                                          replyRunnable.forget().get());
      break;
    }
    case FMRadioRequestType::TSeekRequest:
    {
      LOG("Call Seek");
      SeekRequest request = mRequestType;
      nsRefPtr<ReplyRunnable> replyRunnable = new ParentReplyRunnable(this);
      FMRadioService::Get()->Seek(request.upward(),
                                  replyRunnable.forget().get());
      break;
    }
    case FMRadioRequestType::TCancelSeekRequest:
    {
      LOG("Call CancelSeek");
      nsRefPtr<ReplyRunnable> replyRunnable = new ParentReplyRunnable(this);
      FMRadioService::Get()->CancelSeek(replyRunnable.forget().get());
      break;
    }
    default:
      NS_RUNTIMEABORT("not reached");
      break;
  }
}

FMRadioRequestParent::~FMRadioRequestParent()
{
  LOG("destructor");
  MOZ_COUNT_DTOR(FMRadioRequestParent);
}

void
FMRadioRequestParent::ActorDestroy(ActorDestroyReason aWhy)
{
  LOG("ActorDestroy: %d", aWhy);
  MutexAutoLock lock(mMutex);
  mActorDestroyed = true;
  int32_t count = mRunnables.Length();
  for (int32_t index = 0; index < count; index++)
  {
    mRunnables[index]->Cancel();
  }
}

NS_IMPL_THREADSAFE_ADDREF(FMRadioRequestParent)
NS_IMPL_THREADSAFE_RELEASE(FMRadioRequestParent)

END_FMRADIO_NAMESPACE
