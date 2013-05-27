/* -*- Mode: c++; c-basic-offset: 2; indent-tabs-mode: nil; tab-width: 40 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_fmradio_ipc_fmradioservice_h__
#define mozilla_dom_fmradio_ipc_fmradioservice_h__

#include "mozilla/dom/fmradio/PFMRadioRequest.h"
#include "FMRadioCommon.h"
#include "mozilla/Hal.h"

BEGIN_FMRADIO_NAMESPACE

class ReplyRunnable : public nsRunnable
{
public:
  ReplyRunnable() : mResponseType(SuccessResponse()) {}
  virtual ~ReplyRunnable() {}

  void
  SetReply(const FMRadioResponseType& aResponseType)
  {
    mResponseType = aResponseType;
  }

protected:
  FMRadioResponseType mResponseType;
};

class IFMRadioService
{
public:
  virtual ~IFMRadioService() { }

  NS_IMETHOD_(nsrefcnt) AddRef(void);
  NS_IMETHOD_(nsrefcnt) Release(void);

  virtual bool IsEnabled() = 0;
  virtual double GetFrequency() = 0;
  virtual double GetFrequencyUpperBound() = 0;
  virtual double GetFrequencyLowerBound() = 0;
  virtual double GetChannelWidth() = 0;

  virtual void Enable(double aFrequency, ReplyRunnable* aRunnable) = 0;
  virtual void Disable(ReplyRunnable* aRunnable) = 0;
  virtual void SetFrequency(double frequency, ReplyRunnable* aRunnable) = 0;
  virtual void Seek(bool upward, ReplyRunnable* aRunnable) = 0;
  virtual void CancelSeek(ReplyRunnable* aRunnable) = 0;

  virtual void DistributeEvent(const FMRadioEventType& aType) = 0;

  virtual void RegisterHandler(FMRadioEventObserver* aHandler) = 0;
  virtual void UnregisterHandler(FMRadioEventObserver* aHandler) = 0;

protected:
  nsAutoRefCnt mRefCnt;
};

class FMRadioService : public IFMRadioService
                     , public hal::FMRadioObserver
{
public:
  static IFMRadioService* Get();

  /* Observer Interface */
  virtual void Notify(const hal::FMRadioOperationInformation& info);

  /* IFMRadioService interfaces */
  virtual bool IsEnabled();
  virtual double GetFrequency();
  virtual double GetFrequencyUpperBound();
  virtual double GetFrequencyLowerBound();
  virtual double GetChannelWidth();

  virtual void Enable(double aFrequency, ReplyRunnable* aRunnable);
  virtual void Disable(ReplyRunnable* aRunnable);
  virtual void SetFrequency(double frequency, ReplyRunnable* aRunnable);
  virtual void Seek(bool upward, ReplyRunnable* aRunnable);
  virtual void CancelSeek(ReplyRunnable* aRunnable);

  virtual void DistributeEvent(const FMRadioEventType& aType);

  virtual void RegisterHandler(FMRadioEventObserver* aHandler);
  virtual void UnregisterHandler(FMRadioEventObserver* aHandler);

  void UpdatePowerState();
  void UpdateFrequency();

private:
  FMRadioService();
  ~FMRadioService();

  void DoDisable();

  /**
   * Round the frequency to match the range of frequency and the channel width.
   * If the given frequency is out of range, return 0.
   * For example:
   *  - lower: 87.5MHz, upper: 108MHz, channel width: 0.2MHz
   *    87600 is rounded to 87700
   *    87580 is rounded to 87500
   *    109000 is not rounded, null will be returned
   */
  int32_t RoundFrequency(int32_t aFrequencyInKHz);

private:
  bool mEnabled;
  int32_t mFrequencyInKHz;
  /* Indicates if the FM radio is currently being disabled */
  bool mDisabling;
  /* Indicates if the FM radio is currently being enabled */
  bool mEnabling;
  bool mSeeking;

  double mUpperBoundInMHz;
  double mLowerBoundInMHz;
  double mChannelWidthInMHz;

  nsRefPtr<ReplyRunnable> mDisableRequest;
  nsRefPtr<ReplyRunnable> mEnableRequest;
  nsRefPtr<ReplyRunnable> mSeekRequest;

private:
  static nsRefPtr<FMRadioService> sFMRadioService;
  static FMRadioEventObserverList* sObserverList;
};

END_FMRADIO_NAMESPACE

#endif // mozilla_dom_fmradio_ipc_fmradioservice_h__
