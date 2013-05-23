/* -*- Mode: c++; c-basic-offset: 2; indent-tabs-mode: nil; tab-width: 40 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "FMRadioChildService.h"
#include "FMRadioChild.h"
#include "mozilla/dom/ContentChild.h"
#include "mozilla/dom/fmradio/FMRadioChild.h"
#include "mozilla/dom/fmradio/FMRadioRequestChild.h"

#undef LOG
#define LOG(args...) FM_LOG("PFMRadioChildService", args)

BEGIN_FMRADIO_NAMESPACE

FMRadioChild* gFMRadioChild;
// TODO release static object
FMRadioChildService* gFMRadioChildService;

FMRadioChildService::FMRadioChildService()
{
  LOG("Constructor");
}

FMRadioChildService::~FMRadioChildService()
{
  LOG("Destructor");
  gFMRadioChildService = nullptr;
  // The FMRadioChild object will be released in ContentChild::DeallocPFMRadio
  gFMRadioChild = nullptr;
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

END_FMRADIO_NAMESPACE
