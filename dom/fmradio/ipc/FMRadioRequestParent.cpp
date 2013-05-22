/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "FMRadioRequestParent.h"
#include "FMRadioParentService.h"

#undef LOG
#if defined(MOZ_WIDGET_GONK)
#include <android/log.h>
#define LOG(args...)  __android_log_print(ANDROID_LOG_INFO, \
                                          "PFMRadioRequestParent", \
                                          ## args)
#else
#define LOG(args...)
#endif

namespace mozilla {
namespace dom {
namespace fmradio {

FMRadioRequestParent::FMRadioRequestParent(const FMRadioRequestType& aRequestType)
  : mRequestType(aRequestType)
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
      nsRefPtr<ReplyRunnable> replyRunnable = new ReplyRunnable(this);
      FMRadioParentService::Get()->Enable(request.frequency(),
                                          replyRunnable.forget().get());
      break;
    }
    case FMRadioRequestType::TDisableRequest:
    {
      LOG("Call Disable");
      nsRefPtr<ReplyRunnable> replyRunnable = new ReplyRunnable(this);
      FMRadioParentService::Get()->Disable(replyRunnable.forget().get());
      break;
    }
    case FMRadioRequestType::TSetFrequencyRequest:
    {
      LOG("Call SetFrequency");
      SetFrequencyRequest request = mRequestType;
      nsRefPtr<ReplyRunnable> replyRunnable = new ReplyRunnable(this);
      FMRadioParentService::Get()->SetFrequency(request.frequency(),
                                                replyRunnable.forget().get());
      break;
    }
    case FMRadioRequestType::TSeekRequest:
    {
      LOG("Call Seek");
      SeekRequest request = mRequestType;
      nsRefPtr<ReplyRunnable> replyRunnable = new ReplyRunnable(this);
      FMRadioParentService::Get()->Seek(request.upward(),
                                        replyRunnable.forget().get());
      break;
    }
    case FMRadioRequestType::TCancelSeekRequest:
    {
      LOG("Call CancelSeek");
      nsRefPtr<ReplyRunnable> replyRunnable = new ReplyRunnable(this);
      FMRadioParentService::Get()->CancelSeek(replyRunnable.forget().get());
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
  return;
}

NS_IMPL_THREADSAFE_ADDREF(FMRadioRequestParent)
NS_IMPL_THREADSAFE_RELEASE(FMRadioRequestParent)

} // namespace fmradio
} // namespace dom
} // namespace mozilla

