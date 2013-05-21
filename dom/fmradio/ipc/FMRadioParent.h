/* -*- Mode: c++; c-basic-offset: 2; indent-tabs-mode: nil; tab-width: 40 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_fmradio_ipc_fmradioparent_h__
#define mozilla_dom_fmradio_ipc_fmradioparent_h__

#include "mozilla/dom/fmradio/PFMRadioParent.h"
#include "mozilla/dom/ContentChild.h"

namespace mozilla {
namespace dom {
namespace fmradio {

class PFMRadioRequestParent;

class FMRadioParent : public PFMRadioParent
{
public:
  FMRadioParent();

  virtual ~FMRadioParent();
  virtual void ActorDestroy(ActorDestroyReason aWhy);

  virtual PFMRadioRequestParent*
  AllocPFMRadioRequest(const FMRadioRequestType& aRequestType) MOZ_OVERRIDE;

  virtual bool
  DeallocPFMRadioRequest(PFMRadioRequestParent* aActor) MOZ_OVERRIDE;
};

} // namespace fmradio
} // namespace dom
} // namespace mozilla

#endif // mozilla_dom_fmradio_ipc_fmradioparent_h__

