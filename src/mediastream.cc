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

#include "crtc.h"
#include "mediastream.h"
#include <api/make_ref_counted.h>

using namespace crtc;

std::shared_ptr<MediaStream> MediaStreamInternal::New(webrtc::MediaStreamInterface* stream) {
	if (stream) {
		return std::make_shared<MediaStreamInternal>(stream);
	}

	return nullptr;
}

std::shared_ptr<MediaStream> MediaStreamInternal::New(rtc::scoped_refptr<webrtc::MediaStreamInterface> stream) {
	if (stream.get()) {
		return std::make_shared<MediaStreamInternal>(stream);
	}

	return nullptr;
}

MediaStreamInternal::MediaStreamInternal(webrtc::MediaStreamInterface* stream) :
	_stream(stream),
	_audio_tracks(stream->GetAudioTracks()),
	_video_tracks(stream->GetVideoTracks())
{
	_stream->RegisterObserver(this);
}

MediaStreamInternal::MediaStreamInternal(rtc::scoped_refptr<webrtc::MediaStreamInterface> stream) :
	_stream(stream),
	_audio_tracks(stream->GetAudioTracks()),
	_video_tracks(stream->GetVideoTracks())
{
	_stream->RegisterObserver(this);
}

MediaStreamInternal::~MediaStreamInternal() {
	_stream->UnregisterObserver(this);
}

String MediaStreamInternal::Id() const {
	return String(_stream->id().c_str());
}


void MediaStreamInternal::AddTrack(const std::shared_ptr<MediaStreamTrack>& track) {
	(void)track;
	//TODO
	/*
	auto _track = static_cast<MediaStreamTrackInternal*>(track.get());

	if (track) {
		if (track->Kind() == MediaStreamTrack::kAudio) {
			if (!_stream->AddTrack(static_cast<rtc::scoped_refptr<webrtc::AudioTrackInterface>>(_track->GetTrack()))) {
				// TODO(): Handle Error!
			}
		}
		else {
			if (!_stream->AddTrack(static_cast<rtc::scoped_refptr<webrtc::VideoTrackInterface>>(_track->GetTrack()))) {
				// TODO(): Handle Error!
			}
		}
	}
	*/
}


void MediaStreamInternal::RemoveTrack(const std::shared_ptr<MediaStreamTrack>& track) {
	(void)track;
	//TODO
	/*
	rtc::scoped_refptr<webrtc::MediaStreamTrackInterface> _track = rtc::make_ref_counted<MediaStreamTrackInternal>(track);

	if (_track.get()) {
		if (track->Kind() == MediaStreamTrack::kAudio) {
			if (!_stream->RemoveTrack(rtc::make_ref_counted<webrtc::AudioTrackInterface>(static_cast<webrtc::AudioTrackInterface*>(_track.get())))) {
				// TODO(): Handle Error!
			}
		}
		else {
			if (!_stream->RemoveTrack(rtc::make_ref_counted<webrtc::VideoTrackInterface>(static_cast<webrtc::VideoTrackInterface*>(_track.get())))) {
				// TODO(): Handle Error!
			}
		}
	}
	*/
}

std::shared_ptr<MediaStreamTrack> MediaStreamInternal::GetTrackById(const String& id) const {
	rtc::scoped_refptr<webrtc::MediaStreamTrackInterface> track = _stream->FindAudioTrack(std::string(id));

	if (!track.get()) {
		track = _stream->FindVideoTrack(std::string(id));
	}

	if (track.get()) {
		return MediaStreamTrackInternal::New(track.get());
	}

	return nullptr;
}

intptr_t MediaStreamInternal::GetStream()
{
	return reinterpret_cast<intptr_t>(_stream.get());
}

MediaStreamTracks MediaStreamInternal::GetAudioTracks() const {
	MediaStreamTracks tracks;

	auto audio_tracks(_stream->GetAudioTracks());

	for (const auto& track : audio_tracks) {
		tracks.push_back(MediaStreamTrackInternal::New(track.get()));
	}

	return tracks;
}

MediaStreamTracks MediaStreamInternal::GetVideoTracks() const {
	MediaStreamTracks tracks;

	auto video_tracks(_stream->GetVideoTracks());

	for (const auto& track : video_tracks) {
		tracks.push_back(MediaStreamTrackInternal::New(track.get()));
	}

	return tracks;
}

std::shared_ptr<MediaStream> MediaStreamInternal::Clone() {
	return std::make_shared<MediaStreamInternal>(_stream);
}

void MediaStreamInternal::OnChanged() {
	webrtc::AudioTrackVector audio_tracks = _stream->GetAudioTracks();
	webrtc::VideoTrackVector video_tracks = _stream->GetVideoTracks();

	for (const auto& cached_track : _audio_tracks) {
		auto it = std::find_if(audio_tracks.begin(), audio_tracks.end(), [cached_track](const webrtc::AudioTrackVector::value_type& new_track) {
			return new_track->id().compare(cached_track->id()) == 0;
			});

		if (it == audio_tracks.end()) {
			OnRemoveTrack(MediaStreamTrackInternal::New(cached_track.get()));
		}
	}

	for (const auto& new_track : audio_tracks) {
		auto it = std::find_if(_audio_tracks.begin(), _audio_tracks.end(), [new_track](const webrtc::AudioTrackVector::value_type& cached_track) {
			return new_track->id().compare(cached_track->id()) == 0;
			});

		if (it == _audio_tracks.end()) {
			OnAddTrack(MediaStreamTrackInternal::New(new_track.get()));
		}
	}

	for (const auto& cached_track : _video_tracks) {
		auto it = std::find_if(video_tracks.begin(), video_tracks.end(), [cached_track](const webrtc::VideoTrackVector::value_type& new_track) {
			return new_track->id().compare(cached_track->id()) == 0;
			});

		if (it == video_tracks.end()) {
			OnRemoveTrack(MediaStreamTrackInternal::New(cached_track.get()));
		}
	}

	for (const auto& new_track : video_tracks) {
		auto it = std::find_if(_video_tracks.begin(), _video_tracks.end(), [new_track](const webrtc::VideoTrackVector::value_type& cached_track) {
			return new_track->id().compare(cached_track->id()) == 0;
			});

		if (it == _video_tracks.end()) {
			OnAddTrack(MediaStreamTrackInternal::New(new_track.get()));
		}
	}

	_audio_tracks = audio_tracks;
	_video_tracks = video_tracks;
}

MediaStream::MediaStream() {

}

MediaStream::~MediaStream() {

}