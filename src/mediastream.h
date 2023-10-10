
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

#ifndef CRTC_MEDIASTREAM_H
#define CRTC_MEDIASTREAM_H

#include "crtc.h"
#include "mediastreamtrack.h"

namespace crtc {
	class MediaStreamInternal : public MediaStream, public webrtc::ObserverInterface {
		friend class Let<MediaStreamInternal>;

	public:
		//static webrtc::MediaStreamInterface* New(rtc::scoped_refptr<webrtc::MediaStreamInterface> stream);
		static Let<MediaStream> New(webrtc::MediaStreamInterface* stream);
		static Let<MediaStream> New(rtc::scoped_refptr<webrtc::MediaStreamInterface> stream);

		std::string Id() const override;

		//void AddTrack(const Let<MediaStreamTrack>& track) override;
		//void RemoveTrack(const Let<MediaStreamTrack>& track) override;

		Let<MediaStreamTrack> GetTrackById(const std::string& id) const override;

		intptr_t GetStream() override;

		MediaStreamTracks GetAudioTracks() const override;
		MediaStreamTracks GetVideoTracks() const override;

		Let<MediaStream> Clone() override;

		void OnChanged() override;

	protected:
		explicit MediaStreamInternal(webrtc::MediaStreamInterface* stream);
		explicit MediaStreamInternal(rtc::scoped_refptr<webrtc::MediaStreamInterface> stream = nullptr);
		~MediaStreamInternal() override;

		virtual void OnAddTrack(const Let<MediaStreamTrack>& track) {
			onaddtrack(track);
		}

		virtual void OnRemoveTrack(const Let<MediaStreamTrack>& track) {
			onremovetrack(track);
		}

		rtc::scoped_refptr<webrtc::MediaStreamInterface> _stream;
		webrtc::AudioTrackVector _audio_tracks;
		webrtc::VideoTrackVector _video_tracks;
	};
}

#endif