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
#include "audiosink.h"

using namespace crtc;

AudioSinkInternal::AudioSinkInternal(const Let<MediaStreamTrackInternal>& track) :
	MediaStreamTrackInternal(track),
	_event(Let<Event>::New()),
	_audio_track(static_cast<webrtc::AudioTrackInterface*>(track->GetTrack().get()))
{
	_audio_track->AddSink(this);
	if (!_audio_track->enabled()) {
		_audio_track->set_enabled(true);
	}
}

AudioSinkInternal::~AudioSinkInternal() {
	Stop();
}

Let<AudioSink> AudioSink::New(const Let<MediaStreamTrack>& mediaStreamTrack) {
	if (mediaStreamTrack.IsEmpty() ||
		mediaStreamTrack->Kind() != MediaStreamTrack::kAudio ||
		mediaStreamTrack->ReadyState() != MediaStreamTrack::kLive)
	{
		return Let<AudioSink>();
	}

	Let<MediaStreamTrackInternal> track(mediaStreamTrack->Clone());
	Let<AudioSinkInternal> self(Let<AudioSinkInternal>::New(track));

	return self;
}

bool AudioSinkInternal::Enabled() const {
	return MediaStreamTrackInternal::Enabled();
}

bool AudioSinkInternal::Muted() const {
	return MediaStreamTrackInternal::Muted();
}

bool AudioSinkInternal::Remote() const {
	return MediaStreamTrackInternal::Remote();
}

std::string AudioSinkInternal::Id() const {
	return MediaStreamTrackInternal::Id();
}

MediaStreamTrack::Type AudioSinkInternal::Kind() const {
	return MediaStreamTrackInternal::Kind();
}

MediaStreamTrack::State AudioSinkInternal::ReadyState() const {
	return MediaStreamTrackInternal::ReadyState();
}

Let<MediaStreamTrack> AudioSinkInternal::Clone() {
	return MediaStreamTrackInternal::Clone();
}

bool AudioSinkInternal::IsRunning() const {
	return (!_event.IsEmpty());
}

void AudioSinkInternal::Stop() {
	if (!_event.IsEmpty()) {
		_audio_track->RemoveSink(this);
		_event.Dispose();
	}
}

void AudioSinkInternal::OnEnded() {
	Stop();
}

void AudioSinkInternal::OnData(const void* audio_data, int bits_per_sample, int sample_rate, size_t number_of_channels, size_t number_of_frames) {

}

AudioSink::AudioSink() {

}

AudioSink::~AudioSink() {

}