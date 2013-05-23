/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/fmradio/PFMRadioRequestChild.h"
#include "FMRadioRequestChild.h"
#include "ReplyRunnable.h"

#undef LOG
#define LOG(args...) FM_LOG("PFMRadioRequestChild", args)

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
  mReplyRunnable->SetReply(aType);
  NS_DispatchToMainThread(mReplyRunnable);
  return true;
}

END_FMRADIO_NAMESPACE
