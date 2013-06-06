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

  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(IFMRadioService)

  virtual bool IsEnabled() const = 0;
  virtual double GetFrequency() const = 0;
  virtual double GetFrequencyUpperBound() const = 0;
  virtual double GetFrequencyLowerBound() const = 0;
  virtual double GetChannelWidth() const = 0;

  virtual void Enable(double aFrequency, ReplyRunnable* aRunnable) = 0;
  virtual void Disable(ReplyRunnable* aRunnable) = 0;
  virtual void SetFrequency(double frequency, ReplyRunnable* aRunnable) = 0;
  virtual void Seek(bool upward, ReplyRunnable* aRunnable) = 0;
  virtual void CancelSeek(ReplyRunnable* aRunnable) = 0;

  virtual void NotifyFrequencyChanged(double aFrequency) = 0;
  virtual void NotifyEnabledChanged(bool aEnabled, double aFrequency) = 0;

  /**
   * Register handler to receive the FM Radio events, including:
   *   - StateChangedEvent
   *   - FrequencyChangedEvent
   *
   * Called by FMRadio and FMRadioParent in OOP model.
   */
  virtual void AddObserver(FMRadioEventObserver* aObserver) = 0;
  virtual void RemoveObserver(FMRadioEventObserver* aObserver) = 0;
};

class ReadRilSettingTask;

enum FMRadioState
{
  Disabled,
  Disabling,
  Enabling,
  Enabled,
  Seeking
};

class FMRadioService : public IFMRadioService
                     , public hal::FMRadioObserver
{
  friend class ReadRilSettingTask;

public:
  /**
   * Static method to return the singleton instance.
   *
   * If it's in the child process, we will get an object of FMRadioChildService.
   */
  static IFMRadioService* Singleton();

  void UpdatePowerState();
  void UpdateFrequency();

  /* FMRadioObserver */
  virtual void Notify(const hal::FMRadioOperationInformation& info) MOZ_OVERRIDE;

  /* IFMRadioService */
  virtual bool IsEnabled() const MOZ_OVERRIDE;
  virtual double GetFrequency() const MOZ_OVERRIDE;
  virtual double GetFrequencyUpperBound() const MOZ_OVERRIDE;
  virtual double GetFrequencyLowerBound() const MOZ_OVERRIDE;
  virtual double GetChannelWidth() const MOZ_OVERRIDE;

  virtual void Enable(double aFrequency, ReplyRunnable* aRunnable) MOZ_OVERRIDE;
  virtual void Disable(ReplyRunnable* aRunnable) MOZ_OVERRIDE;
  virtual void SetFrequency(double frequency, ReplyRunnable* aRunnable) MOZ_OVERRIDE;
  virtual void Seek(bool upward, ReplyRunnable* aRunnable) MOZ_OVERRIDE;
  virtual void CancelSeek(ReplyRunnable* aRunnable) MOZ_OVERRIDE;

  virtual void NotifyFrequencyChanged(double aFrequency) MOZ_OVERRIDE;
  virtual void NotifyEnabledChanged(bool aEnabled,
                                    double aFrequency) MOZ_OVERRIDE;

  virtual void AddObserver(FMRadioEventObserver* aObserver) MOZ_OVERRIDE;
  virtual void RemoveObserver(FMRadioEventObserver* aObserver) MOZ_OVERRIDE;

private:
  class RilSettingsObserver MOZ_FINAL : public nsIObserver
  {
  public:
    NS_DECL_ISUPPORTS
    NS_DECL_NSIOBSERVER

    RilSettingsObserver(FMRadioService* aService)
      : mService(aService) { }
    ~RilSettingsObserver() { }

  private:
    // We don't add reference for FMRadioService object, or there is no chance
    // to release it.
    FMRadioService* mService;
  };

private:
  FMRadioService();
  ~FMRadioService();

  void DoDisable();
  int32_t RoundFrequency(int32_t aFrequencyInKHz);
  void SetState(FMRadioState aState);

private:
  bool mEnabled;

  int32_t mFrequencyInKHz;

  FMRadioState mState;

  bool mHasReadRilSetting;
  bool mRilDisabled;

  double mUpperBoundInMHz;
  double mLowerBoundInMHz;
  double mChannelWidthInMHz;

  nsCOMPtr<nsIObserver> mSettingsObserver;

  nsRefPtr<ReplyRunnable> mPendingRequest;

  FMRadioEventObserverList mObserverList;

private:
  static nsRefPtr<FMRadioService> sFMRadioService;
};

END_FMRADIO_NAMESPACE

#endif // mozilla_dom_fmradio_ipc_fmradioservice_h__
