/* -*- Mode: c++; c-basic-offset: 2; indent-tabs-mode: nil; tab-width: 40 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_fmradio_ipc_fmradiochildservice_h__
#define mozilla_dom_fmradio_ipc_fmradiochildservice_h__

#include "FMRadioCommon.h"
#include "DOMRequest.h"
#include "FMRadioService.h"
#include "mozilla/dom/fmradio/PFMRadioChild.h"

BEGIN_FMRADIO_NAMESPACE

class FMRadioChild;
class FMRadioRequestType;

class FMRadioChildService MOZ_FINAL : public IFMRadioService
                                    , public PFMRadioChild
{
  friend class FMRadioChild;

public:
  static FMRadioChildService* Singleton();

  void SendRequest(ReplyRunnable* aReplyRunnable, FMRadioRequestType aType);

  /* IFMRadioService */
  virtual bool IsEnabled() const MOZ_OVERRIDE;
  virtual double GetFrequency() const MOZ_OVERRIDE;
  virtual double GetFrequencyUpperBound() const MOZ_OVERRIDE;
  virtual double GetFrequencyLowerBound() const MOZ_OVERRIDE;
  virtual double GetChannelWidth() const MOZ_OVERRIDE;

  virtual void Enable(double aFrequency, ReplyRunnable* aRunnable) MOZ_OVERRIDE;
  virtual void Disable(ReplyRunnable* aRunnable) MOZ_OVERRIDE;
  virtual void SetFrequency(double frequency,
                            ReplyRunnable* aRunnable) MOZ_OVERRIDE;
  virtual void Seek(bool upward, ReplyRunnable* aRunnable) MOZ_OVERRIDE;
  virtual void CancelSeek(ReplyRunnable* aRunnable) MOZ_OVERRIDE;

  virtual void NotifyFrequencyChanged(double aFrequency) MOZ_OVERRIDE;
  virtual void NotifyEnabledChanged(bool aEnabled,
                                    double aFrequency) MOZ_OVERRIDE;

  virtual void RegisterHandler(FMRadioEventObserver* aHandler) MOZ_OVERRIDE;
  virtual void UnregisterHandler(FMRadioEventObserver* aHandler) MOZ_OVERRIDE;

  /* PFMRadioChild */
  virtual bool
  Recv__delete__() MOZ_OVERRIDE;

  virtual bool
  RecvNotifyFrequencyChanged(const double& aFrequency) MOZ_OVERRIDE;

  virtual bool
  RecvNotifyEnabledChanged(const bool& aEnabled,
                           const double& aFrequency) MOZ_OVERRIDE;

  virtual PFMRadioRequestChild*
  AllocPFMRadioRequest(const FMRadioRequestType& aRequestType) MOZ_OVERRIDE;

  virtual bool
  DeallocPFMRadioRequest(PFMRadioRequestChild* aActor) MOZ_OVERRIDE;

private:
  FMRadioChildService();
  ~FMRadioChildService();

  void Init();

  bool mEnabled;
  double mFrequency;
  Settings mSettings;

  FMRadioEventObserverList* mChildEventObserverList;

private:
  static FMRadioChildService* sFMRadioChildService;
};

END_FMRADIO_NAMESPACE

#endif // mozilla_dom_fmradio_ipc_fmradiochildservice_h__
