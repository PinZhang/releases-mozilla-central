/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/fmradio/PFMRadioChild.h"
#include "FMRadioChild.h"

#undef LOG
#if defined(MOZ_WIDGET_GONK)
#include <android/log.h>
#define LOG(args...)  __android_log_print(ANDROID_LOG_INFO, \
                                          "PFMRadioChild", \
                                          ## args)
#else
#define LOG(args...)
#endif

namespace mozilla {
namespace dom {
namespace fmradio {

FMRadioChild::FMRadioChild()
{
  LOG("Constructor");
  MOZ_COUNT_CTOR(FMRadioChild);
}

FMRadioChild::~FMRadioChild()
{
  LOG("Destructor");
  MOZ_COUNT_DTOR(FMRadioChild);
}

bool
FMRadioChild::RecvEnabled(const bool& aEnabled)
{
  return true;
}

bool
FMRadioChild::RecvFrequencyChanged(const double& aFrequency)
{
  return true;
}

bool
FMRadioChild::Recv__delete__()
{
  LOG("Recv__delete__");
  return true;
}

PFMRadioRequestChild*
FMRadioChild::AllocPFMRadioRequest(const FMRadioRequestType& aRequestType)
{
  MOZ_NOT_REACHED("Caller is supposed to manually construct a request");
  return nullptr;
}

bool
FMRadioChild::DeallocPFMRadioRequest(PFMRadioRequestChild* aActor)
{
  delete aActor;
  return true;
}

NS_IMPL_THREADSAFE_ADDREF(FMRadioChild)
NS_IMPL_THREADSAFE_RELEASE(FMRadioChild)

} // namespace fmradio
} // namespace dom
} // namespace mozilla

