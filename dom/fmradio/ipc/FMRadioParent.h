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

class FMRadioParent : public PFMRadioParent
                    , public FMRadioEventObserver
{
public:
  FMRadioParent();

  virtual ~FMRadioParent();
  virtual void ActorDestroy(ActorDestroyReason aWhy);

  virtual bool
  RecvIsEnabled(bool* aEnabled);

  virtual bool
  RecvGetFrequency(double* aFrequency);

  virtual PFMRadioRequestParent*
  AllocPFMRadioRequest(const FMRadioRequestType& aRequestType) MOZ_OVERRIDE;

  virtual bool
  DeallocPFMRadioRequest(PFMRadioRequestParent* aActor) MOZ_OVERRIDE;

  /* Observer Interface */
  virtual void Notify(const FMRadioEventType& aType);
};

END_FMRADIO_NAMESPACE

#endif // mozilla_dom_fmradio_ipc_fmradioparent_h__

