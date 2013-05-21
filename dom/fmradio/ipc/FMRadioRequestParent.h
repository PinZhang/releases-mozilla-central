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

  class CancelableRunnable : public nsRunnable
  {
  public:
    // FIXME set aParams with default value
    CancelableRunnable(FMRadioRequestParent* aParent,
                       FMRadioRequestType aRequestType = FMRadioRequestType())
      : mParent(aParent)
      , mCanceled(false)
      , mRequestType(aRequestType) { }

    virtual ~CancelableRunnable() { }

    NS_IMETHOD Run()
    {
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

    virtual nsresult CancelableRun() = 0;

  protected:
    nsRefPtr<FMRadioRequestParent> mParent;
    FMRadioRequestType mRequestType;

  private:
    bool mCanceled;
  };

  class PostErrorEvent : public CancelableRunnable
  {
  public:
    PostErrorEvent(FMRadioRequestParent* aParent, const char* aError);
    virtual ~PostErrorEvent();
    virtual nsresult CancelableRun();
  private:
    nsString mError;
  };

  class PostSuccessEvent : public CancelableRunnable
  {
  public:
    PostSuccessEvent(FMRadioRequestParent* aParent);
    virtual ~PostSuccessEvent();
    virtual nsresult CancelableRun();
  };

  class EnableEvent : public CancelableRunnable
  {
  public:
    EnableEvent(FMRadioRequestParent* aParent,
                EnableRequest aRequest);
    virtual ~EnableEvent();
    virtual nsresult CancelableRun();
  };

  class DisableEvent : public CancelableRunnable
  {
  public:
    DisableEvent(FMRadioRequestParent* aParent);
    virtual ~DisableEvent();
    virtual nsresult CancelableRun();
  };

  class SetFrequencyEvent : public CancelableRunnable
  {
  public:
    SetFrequencyEvent(FMRadioRequestParent* aParent,
                      SetFrequencyRequest aRequest);
    virtual ~SetFrequencyEvent();
    virtual nsresult CancelableRun();
  };
};

} // namespace fmradio
} // namespace dom
} // namespace mozilla

#endif // mozilla_dom_fmradio_ipc_fmradiorequestparent_h__

