/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:expandtab:shiftwidth=2:tabstop=2:
*/
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/**
 *  This file is the Gonk implementation of plugin native window.
 */

#include "nsDebug.h"
#include "nsPluginNativeWindow.h"
#include "nsNPAPIPlugin.h"
#include "npapi.h"

#ifdef ANDROID
  #include <android/log.h>
  #define LOG(args...)    __android_log_print(ANDROID_LOG_INFO, "PluginNativeWindow" , ## args)
#else
  #define LOG(str)
#endif

class nsPluginNativeWindowGonk : public nsPluginNativeWindow {
public:
  nsPluginNativeWindowGonk();
  virtual ~nsPluginNativeWindowGonk();

  virtual nsresult CallSetWindow(nsRefPtr<nsNPAPIPluginInstance> &aPluginInstance);
private:

};

nsPluginNativeWindowGonk::nsPluginNativeWindowGonk() : nsPluginNativeWindow()
{
  // Initialize the struct fields.
  window = nullptr;
  x = 0;
  y = 0;
  width = 0;
  height = 0;
  memset(&clipRect, 0, sizeof(clipRect));
  type = NPWindowTypeWindow;
}

nsPluginNativeWindowGonk::~nsPluginNativeWindowGonk()
{

}

nsresult PLUG_NewPluginNativeWindow(nsPluginNativeWindow ** aPluginNativeWindow)
{
  NS_ENSURE_ARG_POINTER(aPluginNativeWindow);
  *aPluginNativeWindow = new nsPluginNativeWindowGonk();
  return *aPluginNativeWindow ? NS_OK : NS_ERROR_OUT_OF_MEMORY;
}

nsresult PLUG_DeletePluginNativeWindow(nsPluginNativeWindow * aPluginNativeWindow)
{
  NS_ENSURE_ARG_POINTER(aPluginNativeWindow);
  nsPluginNativeWindowGonk *p = (nsPluginNativeWindowGonk *)aPluginNativeWindow;
  delete p;
  return NS_OK;
}

nsresult nsPluginNativeWindowGonk::CallSetWindow(nsRefPtr<nsNPAPIPluginInstance> &aPluginInstance)
{
  // TODO: Always use asynchronous drawing for OOP windowless plugins, and plugins on Gonk is
  // OOP and only the windowless mode (window type NPWindowTypeDrawable) is implemented so far,
  // if synchronous rendering is desired, please do some work here.
  return NS_OK;
}
