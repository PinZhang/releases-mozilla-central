/* -*- Mode: c++; c-basic-offset: 2; indent-tabs-mode: nil; tab-width: 40 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef FMRADIOCOMMON_H_
#define FMRADIOCOMMON_H_

#undef FM_LOG
#if defined(MOZ_WIDGET_GONK)
#include <android/log.h>
#define FM_LOG(FMRADIO_LOG_INFO, args...) \
  __android_log_print(ANDROID_LOG_INFO, \
                      FMRADIO_LOG_INFO, \
                      ## args)
#else
#define FM_LOG(args...)
#endif

#undef LOG
#define LOG(args...) FM_LOG("FMRadio", args)

#endif /* FMRADIOCOMMON_H_ */
