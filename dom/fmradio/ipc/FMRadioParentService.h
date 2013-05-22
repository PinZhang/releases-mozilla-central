/* -*- Mode: c++; c-basic-offset: 2; indent-tabs-mode: nil; tab-width: 40 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_fmradio_ipc_fmradioparentservice_h__
#define mozilla_dom_fmradio_ipc_fmradioparentservice_h__

#include "FMRadioCommon.h"
#include "mozilla/dom/fmradio/FMRadioRequestParent.h"

namespace mozilla {
namespace dom {
namespace fmradio {

class FMRadioParent;
class FMRadioRequestParent;

class FMRadioParentService : public hal::FMRadioObserver
{
  friend class FMRadioParent;
  friend class FMRadioRequestParent;

public:
  typedef FMRadioRequestParent::ReplyRunnable ReplyRunnable;

  static FMRadioParentService* Get();

  void Enable(double aFrequency, ReplyRunnable* aRunnable);
  void Disable(ReplyRunnable* aRunnable);
  void SetFrequency(double frequency, ReplyRunnable* aRunnable);
  void Seek(bool upward, ReplyRunnable* aRunnable);
  void CancelSeek(ReplyRunnable* aRunnable);

  /* Observer Interface */
  virtual void Notify(const hal::FMRadioOperationInformation& info);

private:
  FMRadioParentService();
  ~FMRadioParentService();

  void UpdatePowerState();
  void UpdateFrequency();

private:
  bool mEnabled;
  int32_t mFrequency; // frequency in KHz

};

} // namespace fmradio
} // namespace dom
} // namespace mozilla

#endif // mozilla_dom_fmradio_ipc_fmradioparentservice_h__
