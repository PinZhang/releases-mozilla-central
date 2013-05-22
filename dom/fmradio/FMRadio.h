/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_fmradio_FMRadio_h
#define mozilla_dom_fmradio_FMRadio_h

#include "nsDOMEventTargetHelper.h"
#include "nsCycleCollectionParticipant.h"
#include "mozilla/HalTypes.h"

class nsPIDOMWindow;
class nsIScriptContext;

namespace mozilla {
namespace dom {

class DOMRequest;

namespace fmradio {

class FMRadioChild;

class FMRadio MOZ_FINAL : public nsDOMEventTargetHelper
                        , public hal::SwitchObserver

{
public:
  FMRadio();
  ~FMRadio();

  void Init(nsPIDOMWindow *aWindow);
  void Shutdown();

  /* Observer Interface */
  virtual void Notify(const hal::SwitchEvent& aEvent);

  /* WebIDL Interface */
  nsPIDOMWindow * GetParentObject() const
  {
    return GetOwner();
  }

  virtual JSObject* WrapObject(JSContext* aCx,
                               JS::Handle<JSObject*> aScope) MOZ_OVERRIDE;

  bool Enabled() const;

  bool AntennaAvailable() const;

  Nullable<double> GetFrequency() const;

  double FrequencyUpperBound() const;

  double FrequencyLowerBound() const;

  double ChannelWidth() const;

  already_AddRefed<DOMRequest> Enable(double aFrequency);

  already_AddRefed<DOMRequest> Disable();

  already_AddRefed<DOMRequest> SetFrequency(double aFrequency);

  already_AddRefed<DOMRequest> SeekUp();

  already_AddRefed<DOMRequest> SeekDown();

  already_AddRefed<DOMRequest> CancelSeek();

  IMPL_EVENT_HANDLER(enabled);
  IMPL_EVENT_HANDLER(disabled);
  IMPL_EVENT_HANDLER(antennaavailablechange);
  IMPL_EVENT_HANDLER(frequencychange);

private:
  hal::SwitchState mHeadphoneState;
  bool mHasInternalAntenna;
};

} // namespace fmradio
} // namespace dom
} // namespace mozilla

#endif // mozilla_dom_fmradio_FMRadio_h

