/* -*- Mode: c++; c-basic-offset: 2; indent-tabs-mode: nil; tab-width: 40 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_fmradio_ipc_fmradiorequestparent_h__
#define mozilla_dom_fmradio_ipc_fmradiorequestparent_h__

#include "mozilla/dom/fmradio/PFMRadioRequestParent.h"

namespace mozilla {
namespace dom {
namespace fmradio {

class FMRadioRequestParent : public PFMRadioRequestParent
{
public:
  FMRadioRequestParent();
  virtual ~FMRadioRequestParent();

  virtual void ActorDestroy(ActorDestroyReason aWhy);
};

} // namespace fmradio
} // namespace dom
} // namespace mozilla

#endif // mozilla_dom_fmradio_ipc_fmradiorequestparent_h__

