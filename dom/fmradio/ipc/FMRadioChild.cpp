/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/fmradio/PFMRadioChild.h"
#include "FMRadioChild.h"

#undef LOG
#define LOG(args...) FM_LOG("PFMRadioChild", args)

BEGIN_FMRADIO_NAMESPACE

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
  LOG("RecvEnabled");
  return true;
}

bool
FMRadioChild::RecvFrequencyChanged(const double& aFrequency)
{
  LOG("RecvFrequencyChanged");
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

END_FMRADIO_NAMESPACE
