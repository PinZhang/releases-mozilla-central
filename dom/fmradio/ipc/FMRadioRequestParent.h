/* -*- Mode: c++; c-basic-offset: 2; indent-tabs-mode: nil; tab-width: 40 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_fmradio_ipc_fmradiorequestparent_h__
#define mozilla_dom_fmradio_ipc_fmradiorequestparent_h__

#include "FMRadioCommon.h"
#include "mozilla/dom/fmradio/PFMRadioRequestParent.h"
#include "mozilla/dom/fmradio/PFMRadio.h"
#include "mozilla/dom/ContentChild.h"
#include "mozilla/unused.h"
#include "ReplyRunnable.h"

BEGIN_FMRADIO_NAMESPACE

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

private:
  class ParentReplyRunnable : public ReplyRunnable
  {
  public:
    ParentReplyRunnable(FMRadioRequestParent* aParent)
      : ReplyRunnable()
      , mParent(aParent)
    { }

    virtual ~ParentReplyRunnable() { }

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

  private:
    nsRefPtr<FMRadioRequestParent> mParent;
  };
};

END_FMRADIO_NAMESPACE

#endif // mozilla_dom_fmradio_ipc_fmradiorequestparent_h__
