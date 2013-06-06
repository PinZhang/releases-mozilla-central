/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "FMRadioRequestParent.h"
#include "FMRadioService.h"
#include "mozilla/dom/fmradio/PFMRadio.h"

#undef LOG
#define LOG(args...) FM_LOG("PFMRadioRequestParent", args)

BEGIN_FMRADIO_NAMESPACE

FMRadioRequestParent::FMRadioRequestParent(const FMRadioRequestArgs& aArgs)
  : mRequestArgs(aArgs)
  , mMutex("FMRadioRequestParent::mMutex")
  , mActorDestroyed(false)
{
  LOG("constructor");
  MOZ_COUNT_CTOR(FMRadioRequestParent);
}

void
FMRadioRequestParent::Dispatch()
{
  switch (mRequestArgs.type()) {
    case FMRadioRequestArgs::TEnableRequest:
    {
      LOG("Call Enable");
      const EnableRequest& request = mRequestArgs.get_EnableRequest();
      nsRefPtr<ReplyRunnable> replyRunnable = new ParentReplyRunnable(this);
      FMRadioService::Singleton()->Enable(request.frequency(),
                                          replyRunnable.forget().get());
      break;
    }
    case FMRadioRequestArgs::TDisableRequest:
    {
      LOG("Call Disable");
      nsRefPtr<ReplyRunnable> replyRunnable = new ParentReplyRunnable(this);
      FMRadioService::Singleton()->Disable(replyRunnable.forget().get());
      break;
    }
    case FMRadioRequestArgs::TSetFrequencyRequest:
    {
      LOG("Call SetFrequency");
      const SetFrequencyRequest& request = mRequestArgs.get_SetFrequencyRequest();
      nsRefPtr<ReplyRunnable> replyRunnable = new ParentReplyRunnable(this);
      FMRadioService::Singleton()->SetFrequency(request.frequency(),
                                                replyRunnable.forget().get());
      break;
    }
    case FMRadioRequestArgs::TSeekRequest:
    {
      LOG("Call Seek");
      const SeekRequest& request = mRequestArgs.get_SeekRequest();
      nsRefPtr<ReplyRunnable> replyRunnable = new ParentReplyRunnable(this);
      FMRadioService::Singleton()->Seek(request.upward(),
                                        replyRunnable.forget().get());
      break;
    }
    case FMRadioRequestArgs::TCancelSeekRequest:
    {
      LOG("Call CancelSeek");
      nsRefPtr<ReplyRunnable> replyRunnable = new ParentReplyRunnable(this);
      FMRadioService::Singleton()->CancelSeek(replyRunnable.forget().get());
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
  for (int32_t index = 0; index < count; index++) {
    mRunnables[index]->Cancel();
  }
}

END_FMRADIO_NAMESPACE
