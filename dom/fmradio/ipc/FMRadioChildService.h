/* -*- Mode: c++; c-basic-offset: 2; indent-tabs-mode: nil; tab-width: 40 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_fmradio_ipc_fmradiochildservice_h__
#define mozilla_dom_fmradio_ipc_fmradiochildservice_h__

#include "FMRadioCommon.h"
#include "DOMRequest.h"

BEGIN_FMRADIO_NAMESPACE

class FMRadioChild;
class FMRadioRequestType;
class FMRadioEventType;

class FMRadioChildService
{
  friend class FMRadioChild;

public:
  static FMRadioChildService* Get();

  /* Called by FMRadioRequest */
  void SendRequest(DOMRequest* aRequest, FMRadioRequestType aType);

  /* Called by FMRadioChild when RecvNotify() is called*/
  void DistributeEvent(const FMRadioEventType& aType);

  /* Called when mozFMRadio is inited */
  void RegisterHandler(FMRadioEventObserver* aHandler);

  /* Called when mozFMRadio is shutdown */
  void UnregisterHandler(FMRadioEventObserver* aHandler);

protected:
  FMRadioChildService();
  ~FMRadioChildService();
};

END_FMRADIO_NAMESPACE

#endif // mozilla_dom_fmradio_ipc_fmradiochildservice_h__
