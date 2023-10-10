/*
* The MIT License (MIT)
*
* Copyright (c) 2017 vmolsa <ville.molsa@gmail.com> (http://github.com/vmolsa)
*
* Permission is hereby granted, free of charge, to any person obtaining a copy
* of this software and associated documentation files (the "Software"), to deal
* in the Software without restriction, including without limitation the rights
* to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
* copies of the Software, and to permit persons to whom the Software is
* furnished to do so, subject to the following conditions:
*
* The above copyright notice and this permission notice shall be included in
* all copies or substantial portions of the Software.
*
* THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
* IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
* FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
* AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
* LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
* OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
* THE SOFTWARE.
*
*/

#ifndef CRTC_VIDEOCAPTURER_H
#define CRTC_VIDEOCAPTURER_H

#include <list>
#include "crtc.h"
#include "worker.h"
#include "rtc_base/time_utils.h"
#include "rtc_base/timestamp_aligner.h"
#include "rtc_base/thread.h"
#include "rtc_base/synchronization/mutex.h"
#include "rtc_base/task_utils/repeating_task.h"
#include "media/base/video_source_base.h"
#include "media/base/video_adapter.h"
#include <api/video/i420_buffer.h>
#include "api/task_queue/default_task_queue_factory.h"
#include "third_party/blink/public/web/modules/mediastream/media_stream_video_source.h"
#include "third_party/blink/renderer/modules/mediastream/media_stream_utils.h"


#define AddVideoFormat(formats, x, y) \
  formats.push_back(cricket::VideoFormat(x, y, cricket::VideoFormat::FpsToInterval(120), cricket::FOURCC_I420)); \
  formats.push_back(cricket::VideoFormat(x, y, cricket::VideoFormat::FpsToInterval(60), cricket::FOURCC_I420)); \
  formats.push_back(cricket::VideoFormat(x, y, cricket::VideoFormat::FpsToInterval(30), cricket::FOURCC_I420)); 

namespace crtc {
	class VideoCapturer : public blink::MediaStreamVideoSource {
	public:
		explicit VideoCapturer() :
			blink::MediaStreamVideoSource()
			_drainNeeded(false),
			_taskFactory(webrtc::CreateDefaultTaskQueueFactory()),
			_queue(_taskFactory->CreateTaskQueue("VideoCapturer", webrtc::TaskQueueFactory::Priority::HIGH))
		{
			std::vector<cricket::VideoFormat> formats;

			AddVideoFormat(formats, 4096, 2160);
			AddVideoFormat(formats, 3840, 2160);
			AddVideoFormat(formats, 3440, 1440);
			AddVideoFormat(formats, 2560, 2048);
			AddVideoFormat(formats, 2560, 1600);
			AddVideoFormat(formats, 2560, 1440);
			AddVideoFormat(formats, 2560, 1080);
			AddVideoFormat(formats, 2048, 1536);
			AddVideoFormat(formats, 2048, 1080);
			AddVideoFormat(formats, 1920, 1200);
			AddVideoFormat(formats, 1920, 1080);
			AddVideoFormat(formats, 1680, 1050);
			AddVideoFormat(formats, 1600, 1200);
			AddVideoFormat(formats, 1600, 900);
			AddVideoFormat(formats, 1440, 1080);
			AddVideoFormat(formats, 1440, 960);
			AddVideoFormat(formats, 1440, 900);
			AddVideoFormat(formats, 1400, 1050);
			AddVideoFormat(formats, 1366, 768);
			AddVideoFormat(formats, 1280, 1024);
			AddVideoFormat(formats, 1280, 960);
			AddVideoFormat(formats, 1280, 854);
			AddVideoFormat(formats, 1280, 800);
			AddVideoFormat(formats, 1280, 768);
			AddVideoFormat(formats, 1280, 720);
			AddVideoFormat(formats, 1152, 864);
			AddVideoFormat(formats, 1152, 768);
			AddVideoFormat(formats, 1024, 768);
			AddVideoFormat(formats, 1024, 600);
			AddVideoFormat(formats, 1024, 576);
			AddVideoFormat(formats, 854, 480);
			AddVideoFormat(formats, 800, 600);
			AddVideoFormat(formats, 800, 480);
			AddVideoFormat(formats, 768, 576);
			AddVideoFormat(formats, 640, 480);
			AddVideoFormat(formats, 640, 360);
			AddVideoFormat(formats, 480, 320);
			AddVideoFormat(formats, 352, 288);
			AddVideoFormat(formats, 320, 240);
			AddVideoFormat(formats, 320, 200);
			AddVideoFormat(formats, 160, 120);

			frame_task_ = webrtc::RepeatingTaskHandle::DelayedStart(
				_queue.Get(),
				webrtc::TimeDelta::Seconds(1) / GetCurrentConfiguredFramerate(),
				[this] {
					OnTime();
					//InsertFrame();
					return webrtc::TimeDelta::Seconds(1) / 60;
				},
				webrtc::TaskQueueBase::DelayPrecision::kHigh);
		}

		~VideoCapturer() {
			while (!_queue.empty()) {
				Queue pending = _queue.front();
				_queue.pop_front();
				pending.callback(Error::New("VideoSource ended", __FILE__, __LINE__));
			}
		}

		inline void Start(const cricket::VideoFormat& format) {
			SetCaptureFormat(&format);
			_clock->Start(format.interval / rtc::kNumNanosecsPerMillisec);
			return cricket::CaptureState::CS_RUNNING;
		}

		inline void Stop() {
			webrtc::MutexLock lock(&lock_);

			SetCaptureFormat(NULL);
			SetCaptureState(cricket::CaptureState::CS_STOPPED);
			_clock->Stop();

			while (!_queue.empty()) {
				Queue pending = _queue.front();
				_queue.pop_front();
				pending.callback(Error::New("VideoSource ended", __FILE__, __LINE__));
			}
		}

		inline bool IsRunning() override {
			return (capture_state() == cricket::CaptureState::CS_RUNNING);
		}

		inline bool IsScreencast() const override {
			return false;
		}

		inline bool GetPreferredFourccs(std::vector<uint32_t>* fourccs) override {
			fourccs->push_back(cricket::FOURCC_I420);
			return true;
		}

		sigslot::signal0<> Drain;

		inline void Write(const Let<ImageBuffer>& i420p_frame, ErrorCallback callback) {
			webrtc::MutexLock lock(&lock_);

			cricket::CaptureState state = capture_state();

			if (state == cricket::CS_STARTING || state == cricket::CS_RUNNING) {
				_queue.push_back(Queue(i420p_frame, callback));
			}
			else {
				callback(Error::New("VideoSource ended", __FILE__, __LINE__));
			}
		}

		inline int Width() {
			const cricket::VideoFormat* format = GetCaptureFormat();

			if (format) {
				return format->width;
			}

			return 0;
		}

		inline int Height() {
			const cricket::VideoFormat* format = GetCaptureFormat();

			if (format) {
				return format->height;
			}

			return 0;
		}

		inline float Fps() {
			const cricket::VideoFormat* format = GetCaptureFormat();

			if (format) {
				return cricket::VideoFormat::IntervalToFps(format->interval);
			}

			return 0;
		}

	protected:
		class Queue {
		public:
			explicit Queue()
			{ }

			Queue(const Let<ImageBuffer>& i420p_frame, const ErrorCallback& errorCallback) :
				frame(i420p_frame),
				callback(errorCallback),
				timestamp(rtc::TimeNanos())
			{ }

			Let<ImageBuffer> frame;
			ErrorCallback callback;
			int64_t timestamp;
		};

		webrtc::Mutex lock_;
		bool _drainNeeded;
		webrtc::RepeatingTaskHandle frame_task_;
		cricket::VideoAdapter video_adapter_;

		std::unique_ptr<webrtc::TaskQueueFactory> _taskFactory;
		// Must be the last field, so it will be deconstructed first as tasks
		// in the TaskQueue access other fields of the instance of this class.
		rtc::TaskQueue _queue;

		inline Let<Error> Write(const rtc::scoped_refptr<webrtc::VideoFrameBuffer>& buffer, int64_t timestamp) {
			if (buffer.get()) {
				int width = buffer->width();
				int height = buffer->height();
				int adapted_width;
				int adapted_height;
				int crop_width;
				int crop_height;

				int64_t camera_time_us = timestamp / rtc::kNumNanosecsPerMicrosec;
				int64_t system_time_us = rtc::TimeNanos() / rtc::kNumNanosecsPerMicrosec;
				int64_t translated_time_us;

				Let<Error> error;

				if (!video_adapter_.AdaptFrameResolution(
					width, height, camera_time_us,
					&crop_width, &crop_height, &adapted_width, &adapted_height)) {
					// Drop frame in order to respect frame rate constraint.
					return;
				}

				if (width != adapted_width || height != adapted_height) {
					rtc::scoped_refptr<webrtc::I420Buffer> i420_buffer(webrtc::I420Buffer::Create(adapted_width, adapted_height));
					i420_buffer->ScaleFrom(*buffer->ToI420());

					return WriteFrame(webrtc::VideoFrame(i420_buffer, webrtc::kVideoRotation_0, translated_time_us), width, height);
				}
				else {
					return WriteFrame(webrtc::VideoFrame(buffer, webrtc::kVideoRotation_0, translated_time_us), width, height);
				}
			}
			else {
				if (IsRunning()) {
					return Error::New("Invalid VideoFrame buffer", __FILE__, __LINE__);
				}
			}

			return Error::New("VideoSource ended", __FILE__, __LINE__);
		}

		inline Let<Error> WriteFrame(const webrtc::VideoFrame& frame, int width, int height) {
			if (IsRunning()) {
				OnFrame(frame, width, height);
				return Let<Error>();
			}

			return Error::New("VideoSource ended", __FILE__, __LINE__);
		}

		inline void OnTime() {
			if ((capture_state() == cricket::CaptureState::CS_RUNNING)) {
				Queue pending;
				{
					webrtc::MutexLock lock(&lock_);

					if (!_queue.empty()) {
						pending = _queue.front();
						_queue.pop_front();
					}
					else {
						if (_drainNeeded) {
							_drainNeeded = false;
							Drain();
						}
						return;
					}
				}

				pending.callback(Write(WrapImageBuffer::New(pending.frame), pending.timestamp));
				{
					webrtc::MutexLock lock(&lock_);
					if (!_queue.empty()) {
						_drainNeeded = true;
					}
				}
			}
		}
	};
}

#endif