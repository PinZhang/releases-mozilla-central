/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "FMRadio.h"
#include "mozilla/dom/FMRadioBinding.h"
#include "nsContentUtils.h"

#define ENABLED_EVENT_NAME                NS_LITERAL_STRING("enabled");
#define DISABLED_EVENT_NAME               NS_LITERAL_STRING("disabled");
#define FREQUENCYCHANGE_EVENT_NAME        NS_LITERAL_STRING("frequencychange");
#define ANTENNAAVAILABLECHANGE_EVENT_NAME NS_LITERAL_STRING("antennaavailablechange");

namespace mozilla {
namespace dom {
namespace fmradio {

FMRadio::FMRadio()
{
  SetIsDOMBinding();
}

FMRadio::~FMRadio()
{

}

void
FMRadio::Init(nsPIDOMWindow *aWindow)
{
  BindToOwner(aWindow);
}

void
FMRadio::Shutdown()
{

}

JSObject*
FMRadio::WrapObject(JSContext* aCx, JS::Handle<JSObject*> aScope)
{
  return FMRadioBinding::Wrap(aCx, aScope, this);
}

bool
FMRadio::Enabled() const
{
  return false;
}

bool
FMRadio::AntennaAvailable() const
{
  return false;
}

Nullable<double>
FMRadio::GetFrequency() const
{
  double frequency = 88.8;
  return (Nullable<double>)frequency;
}

double
FMRadio::FrequencyUpperBound() const
{
  double upperBound = 108.0;
  return upperBound;
}

double
FMRadio::FrequencyLowerBound() const
{
  double lowerBound = 87.5;
  return lowerBound;
}

double
FMRadio::ChannelWidth() const
{
  double channelWidth = 0.5;
  return channelWidth;
}

already_AddRefed<DOMRequest>
FMRadio::Enable()
{
  return NULL;
}

already_AddRefed<DOMRequest>
FMRadio::Disable()
{
  return NULL;
}

already_AddRefed<DOMRequest>
FMRadio::SetFrequency(double frequency)
{
  return NULL;
}

already_AddRefed<DOMRequest>
FMRadio::SeekUp()
{
  return NULL;
}

already_AddRefed<DOMRequest>
FMRadio::SeekDown()
{
  return NULL;
}

already_AddRefed<DOMRequest>
FMRadio::CancelSeek()
{
  return NULL;
}

} // namespace fmradio
} // namespace dom
} // namespace mozilla

