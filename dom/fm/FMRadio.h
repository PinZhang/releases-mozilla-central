/* -*- Mode: c++; c-basic-offset: 2; indent-tabs-mode: nil; tab-width: 40 -*- */
/* vim: set ts=2 et sw=2 tw=40: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_fm_radio_h__
#define mozilla_dom_fm_radio_h__

#include "nsCOMPtr.h"
#include "mozilla/HalTypes.h"
#include "nsDOMEventTargetHelper.h"
#include "nsIFMRadio.h"
#include "AudioChannelService.h"

#define NS_FMRADIO_CONTRACTID "@mozilla.org/fmradio;1"
// {DC31F7C8-A94B-4989-B505-3AF4244732B2}
#define NS_FMRADIO_CID { 0xdc31f7c8, 0xa94b, 0x4989, \
      { 0xb5, 0x5, 0x3a, 0xf4, 0x24, 0x47, 0x32, 0xb2 }}

namespace mozilla {
namespace dom {
namespace fm {

/* Header file */
class FMRadio : public nsDOMEventTargetHelper
              , public nsIFMRadio
              , public hal::FMRadioObserver
              , public hal::SwitchObserver
              , public nsIAudioChannelAgentCallback
{
public:
  NS_DECL_ISUPPORTS_INHERITED
  NS_DECL_NSIFMRADIO
  NS_DECL_NSIAUDIOCHANNELAGENTCALLBACK

  NS_REALLY_FORWARD_NSIDOMEVENTTARGET(nsDOMEventTargetHelper)
  FMRadio();
  virtual void Notify(const hal::FMRadioOperationInformation& info);
  virtual void Notify(const hal::SwitchEvent& aEvent);

private:
  ~FMRadio();

  nsCOMPtr<nsIAudioChannelAgent> mAudioChannelAgent;
  hal::SwitchState mHeadphoneState;
  bool mHasInternalAntenna;
  bool mHidden;
};

} // namespace fm
} // namespace dom
} // namespace mozilla
#endif

