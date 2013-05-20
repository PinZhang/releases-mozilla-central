/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "FMRadioRequestParent.h"
#include "mozilla/unused.h"
// FIXME Why not "ContentParent.h"?
#include "mozilla/dom/ContentParent.h"

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

FMRadioRequestParent::FMRadioRequestParent(const FMRadioRequestParams& aParams)
  : mParams(aParams)
{
  LOG("constructor");
  MOZ_COUNT_CTOR(FMRadioRequestParent);
}

void
FMRadioRequestParent::Dispatch()
{
  switch (mParams.type())
  {
    case FMRadioRequestParams::TFMRadioRequestEnableParams:
    {
      FMRadioRequestEnableParams params = mParams;
      nsRefPtr<nsIRunnable> r = new EnableEvent(this, params);

      // FIXME Why NS_STREAMTRANSPORTSERVICE_CONTRACTID?
      nsCOMPtr<nsIEventTarget> target = do_GetService(NS_STREAMTRANSPORTSERVICE_CONTRACTID);
      NS_ASSERTION(target, "Must have stream transport service");
      target->Dispatch(r, NS_DISPATCH_NORMAL);
      break;
    }
    case FMRadioRequestParams::TFMRadioRequestDisableParams:
      break;
    case FMRadioRequestParams::TFMRadioRequestSetFrequencyParams:
      break;
    case FMRadioRequestParams::TFMRadioRequestSeekParams:
      break;
    case FMRadioRequestParams::TFMRadioRequestCancelSeekParams:
      break;
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

/***********************************
 *       PostErrorEvent            *
 ***********************************/
FMRadioRequestParent::PostErrorEvent::PostErrorEvent(
  FMRadioRequestParent* aParent, const char* aError)
  : CancelableRunnable(aParent)
{
  CopyASCIItoUTF16(aError, mError);
}

FMRadioRequestParent::PostErrorEvent::~PostErrorEvent() { }

nsresult
FMRadioRequestParent::PostErrorEvent::CancelableRun()
{
  NS_ASSERTION(NS_IsMainThread(), "Wrong thread!");

  ErrorResponse response(mError);
  unused << mParent->Send__delete__(mParent, response);
  return NS_OK;
}

/***********************************
 *       PostSuccessEvent          *
 ***********************************/
FMRadioRequestParent::PostSuccessEvent::PostSuccessEvent(
  FMRadioRequestParent* aParent)
  : CancelableRunnable(aParent) { }

FMRadioRequestParent::PostSuccessEvent::~PostSuccessEvent() { }

nsresult
FMRadioRequestParent::PostSuccessEvent::CancelableRun()
{
  NS_ASSERTION(NS_IsMainThread(), "Wrong thread!");

  SuccessResponse response;
  unused << mParent->Send__delete__(mParent, response);
  return NS_OK;
}

/*******************************
 *       EnableEvent           *
 *******************************/
FMRadioRequestParent::EnableEvent::EnableEvent(
  FMRadioRequestParent* aParent, FMRadioRequestEnableParams aParams)
  : CancelableRunnable(aParent, aParams) { }

FMRadioRequestParent::EnableEvent::~EnableEvent() {}

nsresult
FMRadioRequestParent::EnableEvent::CancelableRun()
{
  nsRefPtr<nsRunnable> event = new PostSuccessEvent(mParent);
  NS_DispatchToMainThread(event);
  return NS_OK;
}

} // namespace fmradio
} // namespace dom
} // namespace mozilla

