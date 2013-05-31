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
#include "mozilla/dom/fmradio/PFMRadio.h"

BEGIN_FMRADIO_NAMESPACE

class FMRadioChild;
class FMRadioRequestType;

class FMRadioChildService MOZ_FINAL : public IFMRadioService
{
  friend class FMRadioChild;

public:
  static FMRadioChildService* Get();

  void SendRequest(ReplyRunnable* aReplyRunnable, FMRadioRequestType aType);

  /* IFMRadioService interfaces */
  virtual bool IsEnabled() MOZ_OVERRIDE;
  virtual double GetFrequency() MOZ_OVERRIDE;
  virtual double GetFrequencyUpperBound() MOZ_OVERRIDE;
  virtual double GetFrequencyLowerBound() MOZ_OVERRIDE;
  virtual double GetChannelWidth() MOZ_OVERRIDE;

  virtual void Enable(double aFrequency, ReplyRunnable* aRunnable) MOZ_OVERRIDE;
  virtual void Disable(ReplyRunnable* aRunnable) MOZ_OVERRIDE;
  virtual void SetFrequency(double frequency, ReplyRunnable* aRunnable) MOZ_OVERRIDE;
  virtual void Seek(bool upward, ReplyRunnable* aRunnable) MOZ_OVERRIDE;
  virtual void CancelSeek(ReplyRunnable* aRunnable) MOZ_OVERRIDE;

  virtual void DistributeEvent(const FMRadioEventType& aType) MOZ_OVERRIDE;

  virtual void RegisterHandler(FMRadioEventObserver* aHandler) MOZ_OVERRIDE;
  virtual void UnregisterHandler(FMRadioEventObserver* aHandler) MOZ_OVERRIDE;

private:
  FMRadioChildService();
  ~FMRadioChildService();

  void Init();

  bool mEnabled;
  double mFrequency;
  Settings mSettings;

private:
  static FMRadioChild* sFMRadioChild;
  static FMRadioChildService* sFMRadioChildService;
  static FMRadioEventObserverList* sChildEventObserverList;
};

END_FMRADIO_NAMESPACE

#endif // mozilla_dom_fmradio_ipc_fmradiochildservice_h__
