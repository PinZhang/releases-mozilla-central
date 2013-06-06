/* -*- Mode: c++; c-basic-offset: 2; indent-tabs-mode: nil; tab-width: 40 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_fmradio_ipc_fmradioparent_h__
#define mozilla_dom_fmradio_ipc_fmradioparent_h__

#include "FMRadioCommon.h"
#include "mozilla/dom/fmradio/PFMRadioParent.h"
#include "mozilla/dom/ContentChild.h"
#include "mozilla/HalTypes.h"

BEGIN_FMRADIO_NAMESPACE

class PFMRadioRequestParent;

class FMRadioParent MOZ_FINAL : public PFMRadioParent
                              , public FMRadioEventObserver
{
public:
  FMRadioParent();
  ~FMRadioParent();

  virtual void ActorDestroy(ActorDestroyReason aWhy) MOZ_OVERRIDE;

  virtual bool
  RecvIsEnabled(bool* aEnabled) MOZ_OVERRIDE;

  virtual bool
  RecvGetFrequency(double* aFrequency) MOZ_OVERRIDE;

  bool
  RecvGetSettings(Settings* aSettings) MOZ_OVERRIDE;

  virtual PFMRadioRequestParent*
  AllocPFMRadioRequest(const FMRadioRequestArgs& aArgs) MOZ_OVERRIDE;

  virtual bool
  DeallocPFMRadioRequest(PFMRadioRequestParent* aActor) MOZ_OVERRIDE;

  /* Observer Interface */
  virtual void Notify(const FMRadioEventArgs& aArgs) MOZ_OVERRIDE;
};

END_FMRADIO_NAMESPACE

#endif // mozilla_dom_fmradio_ipc_fmradioparent_h__

