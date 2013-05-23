/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/fmradio/PFMRadioRequestChild.h"
#include "FMRadioRequestChild.h"

#undef LOG
#define LOG(args...) FM_LOG("PFMRadioRequestChild", args)

BEGIN_FMRADIO_NAMESPACE

FMRadioRequestChild::FMRadioRequestChild(DOMRequest* aRequest)
  : mRequest(aRequest)
{
  LOG("Constructor");
  MOZ_COUNT_CTOR(FMRadioRequestChild);
}

FMRadioRequestChild::~FMRadioRequestChild()
{
  LOG("DESTRUCTOR");
  MOZ_COUNT_DTOR(FMRadioRequestChild);
}

bool
FMRadioRequestChild::Recv__delete__(const FMRadioResponseType& aType)
{
  switch (aType.type())
  {
    case FMRadioResponseType::TErrorResponse:
    {
      ErrorResponse response = aType;
      mRequest->FireError(response.error());
      break;
    }
    case FMRadioResponseType::TSuccessResponse:
    {
      SuccessResponse response = aType;
      // FIXME create a meaningfull result
      JS::Value result = JS_NumberValue(1);
      mRequest->FireSuccess(result);
      break;
    }
    default:
      NS_RUNTIMEABORT("not reached");
      break;
  }
  return true;
}

END_FMRADIO_NAMESPACE
