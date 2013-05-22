/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "FMRadioParent.h"
#include "FMRadioRequestParent.h"

#undef LOG
#if defined(MOZ_WIDGET_GONK)
#include <android/log.h>
#define LOG(args...)  __android_log_print(ANDROID_LOG_INFO, \
                                          "PFMRadioParent", \
                                          ## args)
#else
#define LOG(args...)
#endif

// The pref indicates if the device has an internal antenna.
// If the pref is true, the antanna will be always available.
#define DOM_FM_ANTENNA_INTERNAL_PREF "dom.fmradio.antenna.internal"

namespace mozilla {
namespace dom {
namespace fmradio {

FMRadioParent::FMRadioParent()
{
  LOG("constructor");
  MOZ_COUNT_CTOR(FMRadioParent);
}

FMRadioParent::~FMRadioParent()
{
  LOG("destructor");
  MOZ_COUNT_DTOR(FMRadioParent);
}

void
FMRadioParent::ActorDestroy(ActorDestroyReason aWhy)
{
  LOG("ActorDestroy");
  return;
}

PFMRadioRequestParent*
FMRadioParent::AllocPFMRadioRequest(const FMRadioRequestType& aRequestType)
{
  LOG("AllocPFMRadioRequest");
  nsRefPtr<FMRadioRequestParent> result = new FMRadioRequestParent(aRequestType);
  result->Dispatch();
  return result.forget().get();
}

bool
FMRadioParent::DeallocPFMRadioRequest(PFMRadioRequestParent* aActor)
{
  LOG("DeallocPFMRadioRequest");
  FMRadioRequestParent* parent = static_cast<FMRadioRequestParent*>(aActor);
  NS_RELEASE(parent);
  return true;
}

} // namespace fmradio
} // namespace dom
} // namespace mozilla

