/* -*- Mode: c++; c-basic-offset: 2; indent-tabs-mode: nil; tab-width: 40 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_fmradio_ipc_fmradiochild_h__
#define mozilla_dom_fmradio_ipc_fmradiochild_h__

#include "FMRadioCommon.h"
#include "mozilla/dom/fmradio/PFMRadioChild.h"

BEGIN_FMRADIO_NAMESPACE

class FMRadioChild MOZ_FINAL : public PFMRadioChild
{
public:
  FMRadioChild();
  ~FMRadioChild();

  virtual bool
  Recv__delete__() MOZ_OVERRIDE;

  virtual bool
  RecvNotify(const FMRadioEventType& aType) MOZ_OVERRIDE;

  virtual PFMRadioRequestChild*
  AllocPFMRadioRequest(const FMRadioRequestType& aRequestType) MOZ_OVERRIDE;

  virtual bool
  DeallocPFMRadioRequest(PFMRadioRequestChild* aActor) MOZ_OVERRIDE;
};

END_FMRADIO_NAMESPACE

#endif // mozilla_dom_fmradio_ipc_fmradiochild_h__

