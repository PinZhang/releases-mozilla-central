/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_fmradio_FMRadio_h
#define mozilla_dom_fmradio_FMRadio_h

#include "FMRadioCommon.h"
#include "nsDOMEventTargetHelper.h"
#include "nsCycleCollectionParticipant.h"
#include "mozilla/HalTypes.h"
#include "DOMRequest.h"
#include "FMRadioService.h"

class nsPIDOMWindow;
class nsIScriptContext;

BEGIN_FMRADIO_NAMESPACE

class FMRadio MOZ_FINAL : public nsDOMEventTargetHelper
                        , public hal::SwitchObserver
                        , public FMRadioEventObserver
{
public:
  FMRadio();
  ~FMRadio();

  NS_REALLY_FORWARD_NSIDOMEVENTTARGET(nsDOMEventTargetHelper)

  void Init(nsPIDOMWindow *aWindow);
  void Shutdown();

  /* Observer Interface */
  virtual void Notify(const hal::SwitchEvent& aEvent) MOZ_OVERRIDE;
  virtual void Notify(const FMRadioEventArgs& aArgs) MOZ_OVERRIDE;

  /* WebIDL Interface */
  nsPIDOMWindow * GetParentObject() const
  {
    return GetOwner();
  }

  virtual JSObject* WrapObject(JSContext* aCx,
                               JS::Handle<JSObject*> aScope) MOZ_OVERRIDE;

  bool Enabled() const;

  bool AntennaAvailable() const;

  Nullable<double> GetFrequency() const;

  double FrequencyUpperBound() const;

  double FrequencyLowerBound() const;

  double ChannelWidth() const;

  already_AddRefed<DOMRequest> Enable(double aFrequency);

  already_AddRefed<DOMRequest> Disable();

  already_AddRefed<DOMRequest> SetFrequency(double aFrequency);

  already_AddRefed<DOMRequest> SeekUp();

  already_AddRefed<DOMRequest> SeekDown();

  already_AddRefed<DOMRequest> CancelSeek();

  IMPL_EVENT_HANDLER(enabled);
  IMPL_EVENT_HANDLER(disabled);
  IMPL_EVENT_HANDLER(antennaavailablechange);
  IMPL_EVENT_HANDLER(frequencychange);

  static already_AddRefed<FMRadio>
  CheckPermissionAndCreateInstance(nsPIDOMWindow* aWindow);

  void AddRunnable(ReplyRunnable* aRunnable)
  {
    mRunnables.AppendElement(aRunnable);
  }

  void RemoveRunnable(ReplyRunnable* aRunnable)
  {
    mRunnables.RemoveElement(aRunnable);
  }

private:
  hal::SwitchState mHeadphoneState;
  bool mHasInternalAntenna;
  nsTArray<nsRefPtr<ReplyRunnable> > mRunnables;
};

class FMRadioRequest MOZ_FINAL : public ReplyRunnable
                               , public DOMRequest
{
public:
  NS_DECL_ISUPPORTS_INHERITED
  NS_DECL_CYCLE_COLLECTION_CLASS_INHERITED(FMRadioRequest, DOMRequest)

  FMRadioRequest(nsPIDOMWindow* aWindow, FMRadio* aFMRadio)
    : DOMRequest(aWindow)
    , mFMRadio(aFMRadio)
    , mCanceled(false)
  {
    mFMRadio->AddRunnable(this);
  }

  ~FMRadioRequest() { }

  NS_IMETHOD Run()
  {
    NS_ASSERTION(NS_IsMainThread(), "Wrong thread!");

    if (!mCanceled) {
      CancelableRun();
      mFMRadio->RemoveRunnable(this);
    }

    return NS_OK;
  }

  nsresult CancelableRun()
  {
    switch (mResponseType.type()) {
      case FMRadioResponseType::TErrorResponse:
      {
        ErrorResponse response = mResponseType;
        FireError(response.error());
        break;
      }
      case FMRadioResponseType::TSuccessResponse:
      {
        FireSuccess(JS::Rooted<JS::Value>(AutoJSContext(), JSVAL_VOID));
        break;
      }
      default:
        NS_RUNTIMEABORT("not reached");
        break;
    }
    return NS_OK;
  }

  NS_IMETHOD Cancel()
  {
    mCanceled = true;
    return NS_OK;
  }

private:
  nsRefPtr<FMRadio> mFMRadio;
  bool mCanceled;
};

END_FMRADIO_NAMESPACE

#endif // mozilla_dom_fmradio_FMRadio_h
