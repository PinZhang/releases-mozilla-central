/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/fmradio/PFMRadioRequestChild.h"
#include "FMRadioRequestChild.h"
#include "FMRadioService.h"

#undef LOG
#define LOG(args...) FM_LOG("FMRadioRequestChild", args)

BEGIN_FMRADIO_NAMESPACE

FMRadioRequestChild::FMRadioRequestChild(ReplyRunnable* aReplyRunnable)
  : mReplyRunnable(aReplyRunnable)
{
  LOG("Constructor");
  MOZ_COUNT_CTOR(FMRadioRequestChild);
}

FMRadioRequestChild::~FMRadioRequestChild()
{
  LOG("DESTRUCTOR");
  MOZ_COUNT_DTOR(FMRadioRequestChild);
}

bool
FMRadioRequestChild::Recv__delete__(const FMRadioResponseType& aType)
{
  LOG("Recv__delete__");

  mReplyRunnable->SetReply(aType);
  NS_DispatchToMainThread(mReplyRunnable);
  // We set mReplyRunnable to nullptr to make sure we can handle it correctly
  // in ActorDestroy().
  mReplyRunnable = nullptr;
  return true;
}

void
FMRadioRequestChild::ActorDestroy(ActorDestroyReason aWhy)
{
  LOG("ActorDestroy");

  if (!mReplyRunnable)
  {
    return;
  }

  LOG("Window is closed: %d", aWhy);

  // If mReplyRunnable is not nullptr, it means we didn't receive __delete__
  // message normally, and we need cancel and dispatch it to make sure
  // it will be removed from FMRadio::mRunnables
  mReplyRunnable->Cancel();
  NS_DispatchToMainThread(mReplyRunnable);
}

END_FMRADIO_NAMESPACE
