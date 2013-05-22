/* -*- Mode: c++; c-basic-offset: 2; indent-tabs-mode: nil; tab-width: 40 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_fmradio_ipc_fmradiorequestparent_h__
#define mozilla_dom_fmradio_ipc_fmradiorequestparent_h__

#include "mozilla/dom/fmradio/PFMRadioRequestParent.h"
#include "mozilla/dom/fmradio/PFMRadio.h"
#include "mozilla/dom/ContentChild.h"
#include "mozilla/unused.h"

namespace mozilla {
namespace dom {
namespace fmradio {

class FMRadioRequestParent : public PFMRadioRequestParent
{
public:
  FMRadioRequestParent(const FMRadioRequestType& aRequestType);
  virtual ~FMRadioRequestParent();

  NS_IMETHOD_(nsrefcnt) AddRef();
  NS_IMETHOD_(nsrefcnt) Release();

  void Dispatch();

  virtual void ActorDestroy(ActorDestroyReason aWhy);

private:
  nsAutoRefCnt mRefCnt;
  FMRadioRequestType mRequestType;

public:
  class ReplyRunnable : public nsRunnable
  {
  public:
    // FIXME set aParams with default value
    ReplyRunnable(FMRadioRequestParent* aParent)
      : mParent(aParent)
      , mCanceled(false)
    { }

    virtual ~ReplyRunnable() { }

    NS_IMETHOD Run()
    {
      NS_ASSERTION(NS_IsMainThread(), "Wrong thread!");

      nsresult rv = NS_OK;
      if (!mCanceled) {
        rv = CancelableRun();
      }

      return rv;
    }

    void Cancel()
    {
      mCanceled = true;
    }

    nsresult CancelableRun()
    {
      if (mResponseType.type() == FMRadioResponseType::TSuccessResponse) {
        SuccessResponse response;
        unused << mParent->Send__delete__(mParent, response);
      } else {
        ErrorResponse response(mError);
        unused << mParent->Send__delete__(mParent, response);
      }

      return NS_OK;
    }

    void SetReply(const FMRadioResponseType& aResponseType)
    {
      mResponseType = aResponseType;
    }

    void SetError(const char* aError)
    {
      CopyASCIItoUTF16(aError, mError);
    }

  protected:
    nsRefPtr<FMRadioRequestParent> mParent;
    FMRadioResponseType mResponseType;
    nsString mError;

  private:
    bool mCanceled;
  };
};

} // namespace fmradio
} // namespace dom
} // namespace mozilla

#endif // mozilla_dom_fmradio_ipc_fmradiorequestparent_h__

