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
  class ParentReplyRunnable : public ReplyRunnable
  {
  public:
    ParentReplyRunnable(FMRadioRequestParent* aParent)
      : ReplyRunnable()
      , mParent(aParent)
    {
      mCanceled = !(mParent->AddRunnable(this));
    }

    virtual ~ParentReplyRunnable() { }

    NS_IMETHOD Run()
    {
      NS_ASSERTION(NS_IsMainThread(), "Wrong thread!");

      nsresult rv = NS_OK;

      if (!mCanceled)
      {
        rv = CancelableRun();
      }

      return rv;
    }

    nsresult CancelableRun()
    {
      unused << mParent->Send__delete__(mParent, mResponseType);
      return NS_OK;
    }

    void Cancel()
    {
      mCanceled = true;
    }

  private:
    nsRefPtr<FMRadioRequestParent> mParent;
    bool mCanceled;
  };

private:
  bool AddRunnable(ParentReplyRunnable* aRunnable)
  {
    MutexAutoLock lock(mMutex);
    if (mActorDestroyed)
      return false;

    mRunnables.AppendElement(aRunnable);
    return true;
  }

  void RemoveRunnable(ParentReplyRunnable* aRunnable)
  {
    MutexAutoLock lock(mMutex);
    mRunnables.RemoveElement(aRunnable);
  }

private:
  nsAutoRefCnt mRefCnt;
  FMRadioRequestType mRequestType;
  Mutex mMutex;
  bool mActorDestroyed;
  nsTArray<nsRefPtr<ParentReplyRunnable> > mRunnables;
};

END_FMRADIO_NAMESPACE

#endif // mozilla_dom_fmradio_ipc_fmradiorequestparent_h__
