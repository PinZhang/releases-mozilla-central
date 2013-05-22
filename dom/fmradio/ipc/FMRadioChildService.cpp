/* -*- Mode: c++; c-basic-offset: 2; indent-tabs-mode: nil; tab-width: 40 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "FMRadioChildService.h"
#include "FMRadioChild.h"
#include "mozilla/dom/ContentChild.h"
#include "DOMRequest.h"
#include "mozilla/dom/fmradio/FMRadioChild.h"
#include "mozilla/dom/fmradio/FMRadioRequestChild.h"

#undef LOG
#if defined(MOZ_WIDGET_GONK)
#include <android/log.h>
#define LOG(args...)  __android_log_print(ANDROID_LOG_INFO, \
                                          "PFMRadioChildService", \
                                          ## args)
#else
#define LOG(args...)
#endif

namespace mozilla {
namespace dom {
namespace fmradio {

nsRefPtr<FMRadioChild> gFMRadioChild;
FMRadioChildService* gFMRadioChildService;

FMRadioChildService::FMRadioChildService()
{
  LOG("Constructor");
}

FMRadioChildService::~FMRadioChildService()
{
  // TODO releate static object
  LOG("Destructor");
  gFMRadioChildService = nullptr;
  NS_RELEASE(gFMRadioChild);
}

void
FMRadioChildService::SendRequest(DOMRequest* aRequest, FMRadioRequestType aType)
{
  MOZ_ASSERT(gFMRadioChild);

  PFMRadioRequestChild* child = new FMRadioRequestChild(aRequest);
  gFMRadioChild->SendPFMRadioRequestConstructor(child, aType);
  LOG("Request is sent.");
}

// static
FMRadioChildService*
FMRadioChildService::Get()
{
  MOZ_ASSERT(NS_IsMainThread());

  if (gFMRadioChildService) {
    LOG("Return cached gFMRadioChildService");
    return gFMRadioChildService;
  }

  LOG("Create gFMRadioChildService");
  MOZ_ASSERT(!gFMRadioChild);

  gFMRadioChild = new FMRadioChild();
  ContentChild::GetSingleton()->SendPFMRadioConstructor(gFMRadioChild);

  gFMRadioChildService = new FMRadioChildService();

  return gFMRadioChildService;
}

} // namespace fmradio
} // namespace dom
} // namespace mozilla

