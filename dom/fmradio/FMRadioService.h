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

class ReplyRunnable : public nsCancelableRunnable
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

/**
 * The FMRadio Service Interface for FMRadio.
 *
 * All the requests coming from the content page will be redirected to the
 * concrete class object respectively.
 *
 * There are two concrete classes which implement the interface:
 *  - FMRadioService
 *    It's used in the main process, implements all the logics about FM Radio.
 *
 *  - FMRadioChildService
 *    It's used in OOP subprocess, it's a kind of proxy which just send all
 *    the requests to main process through IPC channel.
 *
 * Consider navigator.mozFMRadio.enable(), here is the call sequences:
 *  - OOP
 *    (1) Call navigator.mozFMRadio.enable().
 *    (2) Return a DOMRequest object, and call FMRadioChildService.Enable() with
 *        a ReplyRunnable object.
 *    (3) Send IPC message to main process.
 *    (4) Call FMRadioService::Enable() with a ReplyRunnable object.
 *    (5) Call hal::EnableFMRadio().
 *    (6) Notify FMRadioService object when FM radio HW is enabled.
 *    (7) Dispatch the ReplyRunnable object created in (4).
 *    (8) Send IPC message back to child process.
 *    (9) Dispatch the ReplyRunnable object created in (2).
 *   (10) Fire success callback of the DOMRequest Object created in (2).
 *                     _ _ _ _ _ _ _ _ _ _ _ _ _ _
 *                    |            OOP            |
 *                    |                           |
 *   Page  FMRadio    |  FMRadioChildService  IPC |    FMRadioService   Hal
 *    | (1)  |        |          |             |  |           |          |
 *    |----->|    (2) |          |             |  |           |          |
 *    |      |--------|--------->|      (3)    |  |           |          |
 *    |      |        |          |-----------> |  |   (4)     |          |
 *    |      |        |          |             |--|---------->|  (5)     |
 *    |      |        |          |             |  |           |--------->|
 *    |      |        |          |             |  |           |  (6)     |
 *    |      |        |          |             |  |   (7)     |<---------|
 *    |      |        |          |      (8)    |<-|-----------|          |
 *    |      |    (9) |          |<----------- |  |           |          |
 *    | (10) |<-------|----------|             |  |           |          |
 *    |<-----|        |          |             |  |           |          |
 *                    |                           |
 *                    |_ _ _ _ _ _ _ _ _ _ _ _ _ _|
 *  - non-OOP
 *    In non-OOP model, we don't need to send messages between processes, so
 *    the call sequences are much more simpler, it almost just follows the
 *    sequences presented in OOP model: (1) (2) (5) (6) (9) and (10).
 *
 */
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

  /**
   * Register handler to receive the FM Radio events, including:
   *   - StateChangedEvent
   *   - FrequencyChangedEvent
   *
   * Called by FMRadio and FMRadioParent in OOP model.
   */
  virtual void RegisterHandler(FMRadioEventObserver* aHandler) = 0;
  virtual void UnregisterHandler(FMRadioEventObserver* aHandler) = 0;

protected:
  nsAutoRefCnt mRefCnt;
};

class ReadRilSettingTask;

class FMRadioService : public IFMRadioService
                     , public hal::FMRadioObserver
                     , public nsIObserver
{
  friend class ReadRilSettingTask;

public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSIOBSERVER

  /**
   * Static method to return the singleton instance.
   *
   * If it's in the child process, we will get an object of FMRadioChildService.
   */
  static IFMRadioService* Get();

  void UpdatePowerState();
  void UpdateFrequency();

  /* FMRadioObserver */
  virtual void Notify(const hal::FMRadioOperationInformation& info);

  /* IFMRadioService */
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

private:
  FMRadioService();
  ~FMRadioService();

  void DoDisable();
  int32_t RoundFrequency(int32_t aFrequencyInKHz);

private:
  bool mEnabled;
  int32_t mFrequencyInKHz;
  /* Indicates if the FM radio is currently being disabled */
  bool mDisabling;
  /* Indicates if the FM radio is currently being enabled */
  bool mEnabling;
  /* Indicates if the FM radio is currently seeking */
  bool mSeeking;

  bool mHasReadRilSetting;
  bool mRilDisabled;

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
