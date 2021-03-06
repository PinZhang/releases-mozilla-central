/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef AbstractMediaDecoder_h_
#define AbstractMediaDecoder_h_

#include "nsISupports.h"

namespace mozilla
{

namespace layers
{
  class ImageContainer;
}
class MediaResource;
class ReentrantMonitor;
class VideoFrameContainer;

/**
 * The AbstractMediaDecoder class describes the public interface for a media decoder
 * and is used by the MediaReader classes.
 */
class AbstractMediaDecoder : public nsISupports
{
public:
  // Returns the monitor for other threads to synchronise access to
  // state.
  virtual ReentrantMonitor& GetReentrantMonitor() = 0;

  // Returns true if the decoder is shut down.
  virtual bool IsShutdown() const = 0;

  virtual bool OnStateMachineThread() const = 0;

  virtual bool OnDecodeThread() const = 0;

  // Get the current MediaResource being used. Its URI will be returned
  // by currentSrc. Returns what was passed to Load(), if Load() has been called.
  virtual MediaResource* GetResource() const = 0;

  // Called by the decode thread to keep track of the number of bytes read
  // from the resource.
  virtual void NotifyBytesConsumed(int64_t aBytes) = 0;

  // Increments the parsed and decoded frame counters by the passed in counts.
  // Can be called on any thread.
  virtual void NotifyDecodedFrames(uint32_t aParsed, uint32_t aDecoded) = 0;

  // Returns the end time of the last sample in the media. Note that a media
  // can have a non-zero start time, so the end time may not necessarily be
  // the same as the duration (i.e. duration is (end_time - start_time)).
  virtual int64_t GetEndMediaTime() const = 0;

  // Return the duration of the media in microseconds.
  virtual int64_t GetMediaDuration() = 0;

  // Set the duration of the media in microseconds.
  virtual void SetMediaDuration(int64_t aDuration) = 0;

  virtual VideoFrameContainer* GetVideoFrameContainer() = 0;
  virtual mozilla::layers::ImageContainer* GetImageContainer() = 0;

  // Return true if seeking is supported.
  virtual bool IsMediaSeekable() = 0;

  // Set the media end time in microseconds
  virtual void SetMediaEndTime(int64_t aTime) = 0;

  // Make the decoder state machine update the playback position. Called by
  // the reader on the decoder thread (Assertions for this checked by
  // mDecoderStateMachine). This must be called with the decode monitor
  // held.
  virtual void UpdatePlaybackPosition(int64_t aTime) = 0;

  // Called when the metadata from the media file has been read by the reader.
  // Call on the decode thread only.
  virtual void OnReadMetadataCompleted() = 0;

  // Stack based class to assist in notifying the frame statistics of
  // parsed and decoded frames. Use inside video demux & decode functions
  // to ensure all parsed and decoded frames are reported on all return paths.
  class AutoNotifyDecoded {
  public:
    AutoNotifyDecoded(AbstractMediaDecoder* aDecoder, uint32_t& aParsed, uint32_t& aDecoded)
      : mDecoder(aDecoder), mParsed(aParsed), mDecoded(aDecoded) {}
    ~AutoNotifyDecoded() {
      mDecoder->NotifyDecodedFrames(mParsed, mDecoded);
    }
  private:
    AbstractMediaDecoder* mDecoder;
    uint32_t& mParsed;
    uint32_t& mDecoded;
  };
};

}

#endif

