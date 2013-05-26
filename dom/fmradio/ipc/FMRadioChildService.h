/* -*- Mode: c++; c-basic-offset: 2; indent-tabs-mode: nil; tab-width: 40 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_fmradio_ipc_fmradiochildservice_h__
#define mozilla_dom_fmradio_ipc_fmradiochildservice_h__

#include "FMRadioCommon.h"
#include "DOMRequest.h"
#include "mozilla/dom/fmradio/FMRadioService.h"

BEGIN_FMRADIO_NAMESPACE

class FMRadioChild;
class FMRadioRequestType;

class FMRadioChildService : public IFMRadioService
{
  friend class FMRadioChild;

public:
  static FMRadioChildService* Get();

  void SendRequest(ReplyRunnable* aReplyRunnable, FMRadioRequestType aType);

  /* IFMRadioService interfaces */
  virtual bool IsEnabled();
  virtual double GetFrequency();

  virtual void Enable(double aFrequency, ReplyRunnable* aRunnable);
  virtual void Disable(ReplyRunnable* aRunnable);
  virtual void SetFrequency(double frequency, ReplyRunnable* aRunnable);
  virtual void Seek(bool upward, ReplyRunnable* aRunnable);
  virtual void CancelSeek(ReplyRunnable* aRunnable);

  virtual void DistributeEvent(const FMRadioEventType& aType);

  virtual void RegisterHandler(FMRadioEventObserver* aHandler);
  virtual void UnregisterHandler(FMRadioEventObserver* aHandler);

private:
  FMRadioChildService();
  ~FMRadioChildService();

  void Init();

  bool mEnabled;
  double mFrequency;

private:
  static FMRadioChild* sFMRadioChild;
  static FMRadioChildService* sFMRadioChildService;
  static FMRadioEventObserverList* sChildEventObserverList;
};

END_FMRADIO_NAMESPACE

#endif // mozilla_dom_fmradio_ipc_fmradiochildservice_h__
