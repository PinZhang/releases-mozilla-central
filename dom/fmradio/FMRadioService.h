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

class IFMRadioService
{
public:
  virtual ~IFMRadioService() { }

  virtual void Enable(double aFrequency, ReplyRunnable* aRunnable) = 0;
  virtual void Disable(ReplyRunnable* aRunnable) = 0;
  virtual void SetFrequency(double frequency, ReplyRunnable* aRunnable) = 0;
  virtual void Seek(bool upward, ReplyRunnable* aRunnable) = 0;
  virtual void CancelSeek(ReplyRunnable* aRunnable) = 0;

  virtual void DistributeEvent(const FMRadioEventType& aType) = 0;

  virtual void RegisterHandler(FMRadioEventObserver* aHandler) = 0;
  virtual void UnregisterHandler(FMRadioEventObserver* aHandler) = 0;
};

class FMRadioService : public IFMRadioService
                     , public hal::FMRadioObserver
{
  friend class FMRadioParent;
  friend class FMRadioRequestParent;

public:
  static IFMRadioService* Get();

  /* Observer Interface */
  virtual void Notify(const hal::FMRadioOperationInformation& info);

  /* IFMRadioService interfaces */
  virtual void Enable(double aFrequency, ReplyRunnable* aRunnable);
  virtual void Disable(ReplyRunnable* aRunnable);
  virtual void SetFrequency(double frequency, ReplyRunnable* aRunnable);
  virtual void Seek(bool upward, ReplyRunnable* aRunnable);
  virtual void CancelSeek(ReplyRunnable* aRunnable);

  virtual void DistributeEvent(const FMRadioEventType& aType);

  virtual void RegisterHandler(FMRadioEventObserver* aHandler);
  virtual void UnregisterHandler(FMRadioEventObserver* aHandler);

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
