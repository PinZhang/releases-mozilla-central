// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/message_pump_gonk.h"

#include <fcntl.h>
#include <math.h>

#include "base/eintr_wrapper.h"
#include "base/lazy_instance.h"
#include "base/logging.h"
#include "base/platform_thread.h"
#include <sys/epoll.h>


namespace base {

namespace {
  static int signalfds[2];
  static int epollfd;
  void waitForEvent() {
    int event_count = 0;
    epoll_event events[8] = {{ 0 }};
    if ((event_count = epoll_wait(epollfd, events, 8,  -1)) <= 0) {
      return;
    }

    int rc = 0;
    uint8_t buffer[2];
    do {
      rc = read(signalfds[0], buffer, sizeof(buffer));
    } while (rc > 0 && errno != EAGAIN && errno != EINTR);
  }
}

MessagePumpGonk::MessagePumpGonk(MessagePumpForUI &aPump)
  : pump(aPump)
{
}

MessagePumpGonk::~MessagePumpGonk()
{
}

MessagePumpForUI::MessagePumpForUI()
  : state_(NULL)
  , pump(*this)
{
  if (Init() != NS_OK) {
    NS_ERROR("Fail to init the MessagePumpForUI for a plugin process on gonk platform.");
  }
}

MessagePumpForUI::~MessagePumpForUI() {
}

nsresult MessagePumpForUI::Init() {
  epollfd = epoll_create(16);
  NS_ENSURE_TRUE(epollfd >= 0, NS_ERROR_UNEXPECTED);

  int ret = pipe2(signalfds, O_NONBLOCK);
  NS_ENSURE_FALSE(ret, NS_ERROR_UNEXPECTED);

  epoll_event event = {
    EPOLLIN,
    { 0 }
  };

  epoll_ctl(epollfd, EPOLL_CTL_ADD, signalfds[0], &event);

  return NS_OK;
}

void MessagePumpForUI::Run(Delegate* delegate) {
  RunState state;
  state.delegate = delegate;
  state.should_quit = false;
  state.run_depth = state_ ? state_->run_depth + 1 : 1;

  RunState* previous_state = state_;
  state_ = &state;


  while (!state_->should_quit) {
    waitForEvent();
    if (work_scheduled) {
      work_scheduled = false;
      state.more_work_is_plausible = true;
      HandleDispatch();
    }
  }

  state_ = previous_state;
}

void MessagePumpForUI::HandleDispatch() {
  if (state_->should_quit)
    return;

  // If there are more works to do, we need to loop until all works are done.
  while (state_->more_work_is_plausible) {
    state_->more_work_is_plausible = false;
    state_->more_work_is_plausible |= state_->delegate->DoWork();

    if (state_->should_quit)
      return;

    state_->more_work_is_plausible |= state_->delegate->DoDelayedWork(&delayed_work_time_);

    if (state_->should_quit)
      return;

    // Don't do idle work if we think there are more important things
    // that we could be doing.
    if (state_->more_work_is_plausible)
      continue;

    state_->more_work_is_plausible |= state_->delegate->DoIdleWork();

    if (state_->should_quit)
      return;
  }
}

void MessagePumpForUI::Quit() {
  if (state_) {
    state_->should_quit = true;
  } else {
    NOTREACHED() << "Quit called outside Run!";
  }
}

void MessagePumpForUI::ScheduleWork() {
  // This can be called on any thread, so we don't want to touch any state
  // variables as we would then need locks all over.  This ensures that if
  // we are sleeping in a poll that we will wake up.
  work_scheduled = true;
  write(signalfds[1], "w", 1);
}

void MessagePumpForUI::ScheduleDelayedWork(const TimeTicks& delayed_work_time) {
  // We need to wake up the loop in case the poll timeout needs to be
  // adjusted.  This will cause us to try to do work, but that's ok.
  delayed_work_time_ = delayed_work_time;
  ScheduleWork();
}

}  // namespace base
