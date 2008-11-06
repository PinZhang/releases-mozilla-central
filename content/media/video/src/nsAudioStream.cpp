/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is Mozilla code.
 *
 * The Initial Developer of the Original Code is the Mozilla Corporation.
 * Portions created by the Initial Developer are Copyright (C) 2007
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *  Chris Double <chris.double@double.co.nz>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */
#include <stdio.h>
#include <math.h>
#include "prlog.h"
#include "prmem.h"
#include "nsAutoPtr.h"
#include "nsAudioStream.h"
extern "C" {
#include "sydneyaudio/sydney_audio.h"
}

#ifdef PR_LOGGING
PRLogModuleInfo* gAudioStreamLog = nsnull;
#endif

void nsAudioStream::InitLibrary()
{
#ifdef PR_LOGGING
  gAudioStreamLog = PR_NewLogModule("nsAudioStream");
#endif
}

void nsAudioStream::ShutdownLibrary()
{
}

nsAudioStream::nsAudioStream() :
  mVolume(1.0),
  mAudioHandle(0),
  mRate(0),
  mChannels(0)
{
}

void nsAudioStream::Init(PRInt32 aNumChannels, PRInt32 aRate)
{
  mRate = aRate;
  mChannels = aNumChannels;
  if (sa_stream_create_pcm(reinterpret_cast<sa_stream_t**>(&mAudioHandle),
                           NULL, 
                           SA_MODE_WRONLY, 
                           SA_PCM_FORMAT_S16_LE,
                           aRate,
                           aNumChannels) != SA_SUCCESS) {
    mAudioHandle = nsnull;
    PR_LOG(gAudioStreamLog, PR_LOG_ERROR, ("nsAudioStream: sa_stream_create_pcm error"));
    return;
  }
  
  if (sa_stream_open(reinterpret_cast<sa_stream_t*>(mAudioHandle)) != SA_SUCCESS) {
    sa_stream_destroy((sa_stream_t*)mAudioHandle);
    mAudioHandle = nsnull;
    PR_LOG(gAudioStreamLog, PR_LOG_ERROR, ("nsAudioStream: sa_stream_open error"));
    return;
  }
}

void nsAudioStream::Shutdown()
{
  if (!mAudioHandle) 
    return;

  sa_stream_destroy(reinterpret_cast<sa_stream_t*>(mAudioHandle));
  mAudioHandle = nsnull;
}

void nsAudioStream::Write(float* aBuf, PRUint32 aCount)
{
  if (!mAudioHandle)
    return;

  // Convert array of floats, to an array of signed shorts
  nsAutoArrayPtr<short> s_data(new short[aCount]);

  if (s_data) {
    for (PRUint32 i=0; i <  aCount; ++i) {
      float scaled_value = floorf(0.5 + 32768 * aBuf[i] * mVolume);
      if (aBuf[i] < 0.0) {
        s_data[i] = (scaled_value < -32768.0) ? 
          -32768 : 
          short(scaled_value);
      }
      else {
        s_data[i] = (scaled_value > 32767.0) ? 
          32767 : 
          short(scaled_value);
      }
    }
    
    if (sa_stream_write(reinterpret_cast<sa_stream_t*>(mAudioHandle), s_data.get(), aCount * sizeof(short)) != SA_SUCCESS) {
      PR_LOG(gAudioStreamLog, PR_LOG_ERROR, ("nsAudioStream: sa_stream_write error"));
      Shutdown();
    }     
  }
}

PRInt32 nsAudioStream::Available()
{
  if (!mAudioHandle)
    return 0;

  size_t s = 0; 
  sa_stream_get_write_size(reinterpret_cast<sa_stream_t*>(mAudioHandle), &s);
  return s / sizeof(short);
}

float nsAudioStream::GetVolume()
{
  return mVolume;
}

void nsAudioStream::SetVolume(float aVolume)
{
  mVolume = aVolume;
}
