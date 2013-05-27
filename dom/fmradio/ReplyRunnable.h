/* -*- Mode: c++; c-basic-offset: 2; indent-tabs-mode: nil; tab-width: 40 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_fmradio_replyrunnable_h__
#define mozilla_dom_fmradio_replyrunnable_h__

#include "FMRadioCommon.h"

BEGIN_FMRADIO_NAMESPACE

class ReplyRunnable : public nsRunnable
{
public:
  ReplyRunnable() {}
  virtual ~ReplyRunnable() {}

  void SetReply(const FMRadioResponseType& aResponseType)
  {
    mResponseType = aResponseType;
  }

protected:
  FMRadioResponseType mResponseType;
};

END_FMRADIO_NAMESPACE

#endif // mozilla_dom_fmradio_replyrunnable_h__
