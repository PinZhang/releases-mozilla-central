/* -*- Mode: c++; c-basic-offset: 2; indent-tabs-mode: nil; tab-width: 40 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_fmradio_ipc_fmradiochild_h__
#define mozilla_dom_fmradio_ipc_fmradiochild_h__

#include "nsCycleCollectionParticipant.h"

namespace mozilla {
namespace dom {
namespace fmradio {

class FMRadioRequestChild;

class FMRadioChild : nsIRunnable
                   , public PFMRadioChild
{
public:
  FMRadioChild();
  virtual ~FMRadioChild();

  NS_DECL_CYCLE_COLLECTING_ISUPPORTS
  NS_DECL_CYCLE_COLLECTION_CLASS(FMRadioChild)

  NS_IMETHOD Run()
  {
    return NS_OK;
  }

  virtual bool
  Recv__delete__() MOZ_OVERRIDE;

  virtual bool
  RecvEnabled(const bool& aEnabled) MOZ_OVERRIDE;

  virtual bool
  RecvFrequencyChanged(const double& aFrequency) MOZ_OVERRIDE;

  virtual PFMRadioRequestChild*
  AllocPFMRadioRequest(const FMRadioRequestType& aRequestType) MOZ_OVERRIDE;

  virtual bool
  DeallocPFMRadioRequest(PFMRadioRequestChild* aActor) MOZ_OVERRIDE;
};

} // namespace fmradio
} // namespace dom
} // namespace mozilla
#endif // mozilla_dom_fmradio_ipc_fmradiochild_h__

