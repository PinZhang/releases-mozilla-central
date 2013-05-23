/* -*- Mode: c++; c-basic-offset: 2; indent-tabs-mode: nil; tab-width: 40 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_fmradio_ipc_fmradioservice_h__
#define mozilla_dom_fmradio_ipc_fmradioservice_h__

#include "FMRadioCommon.h"
#include "mozilla/dom/fmradio/FMRadioRequestParent.h"

BEGIN_FMRADIO_NAMESPACE

class FMRadioParent;
class FMRadioRequestParent;
class ReplyRunnable;

class FMRadioService : public hal::FMRadioObserver
{
  friend class FMRadioParent;
  friend class FMRadioRequestParent;

public:
  static FMRadioService* Get();

  void Enable(double aFrequency, ReplyRunnable* aRunnable);
  void Disable(ReplyRunnable* aRunnable);
  void SetFrequency(double frequency, ReplyRunnable* aRunnable);
  void Seek(bool upward, ReplyRunnable* aRunnable);
  void CancelSeek(ReplyRunnable* aRunnable);

  void DistributeEvent(const FMRadioEventType& aType);

  /* Observer Interface */
  virtual void Notify(const hal::FMRadioOperationInformation& info);

  void RegisterObserver(FMRadioEventObserver* aHandler);
  void UnregisterObserver(FMRadioEventObserver* aHandler);

private:
  FMRadioService();
  ~FMRadioService();

  void UpdatePowerState();
  void UpdateFrequency();

private:
  bool mEnabled;
  int32_t mFrequency; // frequency in KHz
};

END_FMRADIO_NAMESPACE

#endif // mozilla_dom_fmradio_ipc_fmradioservice_h__
