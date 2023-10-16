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

#include "rtcpeerconnection.h"
#include "rtcdatachannel.h"
#include "mediastream.h"

using namespace crtc;

std::unique_ptr<rtc::Thread> RTCPeerConnectionInternal::network_thread;
std::unique_ptr<rtc::Thread> RTCPeerConnectionInternal::worker_thread;
rtc::scoped_refptr<webrtc::AudioDeviceModule> RTCPeerConnectionInternal::audio_device;
rtc::scoped_refptr<webrtc::PeerConnectionFactoryInterface> RTCPeerConnectionInternal::factory;

void RTCPeerConnectionInternal::Init() {
	network_thread = rtc::Thread::CreateWithSocketServer();
	network_thread->SetName("network", nullptr);

	if (!network_thread->Start()) {

	}

	worker_thread = rtc::Thread::Create();
	worker_thread->SetName("worker", nullptr);

	if (!worker_thread->Start()) {

	}

	audio_device = webrtc::AudioDeviceModule::Create(webrtc::AudioDeviceModule::kDummyAudio, nullptr);

	if (!audio_device->Initialized()) {
		audio_device->Init();
	}


	/*factory = webrtc::CreatePeerConnectionFactory(
	  network_thread.get(),
	  worker_thread.get(),
	  rtc::ThreadManager::Instance()->CurrentThread(),
	  audio_device.get(),
	  nullptr, // cricket::WebRtcVideoEncoderFactory*
	  nullptr); // cricket::WebRtcVideoDecoderFactory*
	  */

	factory = webrtc::CreatePeerConnectionFactory(
		network_thread.get(),
		worker_thread.get(),
		rtc::ThreadManager::Instance()->CurrentThread(),
		std::move(audio_device),
		nullptr, //  rtc::scoped_refptr<AudioEncoderFactory> audio_encoder_factory,
		nullptr, //rtc::scoped_refptr<AudioDecoderFactory> audio_decoder_factory,
		nullptr, //std::unique_ptr<VideoEncoderFactory> video_encoder_factory,
		nullptr,
		nullptr, //rtc::scoped_refptr<AudioMixer> audio_mixer,
		nullptr, //rtc::scoped_refptr<AudioProcessing> audio_processing,
		nullptr, //std::unique_ptr<AudioFrameProcessor> owned_audio_frame_processor,
		nullptr); //std::unique_ptr<FieldTrialsView> field_trials = nullptr)
}

void RTCPeerConnectionInternal::Dispose() {
	network_thread->Stop();
	worker_thread->Stop();

	factory.release();
	audio_device.release();
	network_thread.release();
	worker_thread.release();
}

RTCPeerConnectionInternal::RTCPeerConnectionInternal(const RTCConfiguration& config) : _factory(factory) {
	webrtc::PeerConnectionInterface::RTCConfiguration cfg(webrtc::PeerConnectionInterface::RTCConfigurationType::kAggressive);

	Let<Error> error = ParseConfiguration(config, &cfg);

	if (error.IsEmpty()) {
		//_socket = _factory->CreatePeerConnection(cfg, nullptr, nullptr, this);
		webrtc::PeerConnectionDependencies pc_dependencies(this);
		auto error_or_peer_connection = _factory->CreatePeerConnectionOrError(cfg, std::move(pc_dependencies));
		if (!error_or_peer_connection.ok())
		{
			return;
		}
		_socket = std::move(error_or_peer_connection.value());
	}
}

RTCPeerConnectionInternal::~RTCPeerConnectionInternal() {
	if (_socket->signaling_state() != webrtc::PeerConnectionInterface::kClosed) {
		_socket->Close();
	}
}

Let<RTCDataChannel> RTCPeerConnectionInternal::CreateDataChannel(const std::string& label, const RTCDataChannelInit& options) {
	webrtc::DataChannelInit init;

	init.ordered = options.ordered;
	init.maxRetransmitTime = options.maxPacketLifeTime;
	init.maxRetransmits = options.maxRetransmits;
	init.protocol = options.protocol;
	init.negotiated = options.negotiated;
	init.id = options.id;

	//rtc::scoped_refptr<webrtc::DataChannelInterface> channel = _socket->CreateDataChannel(label, &init);
	auto error_or_datachannel = _socket->CreateDataChannelOrError(label, &init);
	if (!error_or_datachannel.ok())
	{
		return Let<RTCDataChannel>();
	}

	return Let<RTCDataChannelInternal>::New(std::move(error_or_datachannel.value()));	
}

Let<Promise<void> > RTCPeerConnectionInternal::AddIceCandidate(const RTCPeerConnection::RTCIceCandidate& candidate) {
	return Promise<void>::New([=](const Promise<void>::FullFilledCallback& resolve, const Promise<void>::RejectedCallback& reject) {
		webrtc::SdpParseError error;
		webrtc::IceCandidateInterface* ice = webrtc::CreateIceCandidate(candidate.sdpMid, candidate.sdpMLineIndex, candidate.candidate, &error);

		if (ice) {
			if (!_socket->pending_remote_description() && !_socket->current_remote_description()) {
				_pending_candidates.push_back(Callback([=]() {
					if (_socket->AddIceCandidate(ice)) {
						return resolve();
					}

					if (error.description.empty()) {
						if (!_socket->pending_remote_description() && !_socket->current_remote_description()) {
							return reject(Error::New("ICE candidates can't be added without any remote session description.", __FILE__, __LINE__));
						}

						return reject(Error::New("Candidate cannot be used.", __FILE__, __LINE__));
					}
					}, [=]() {
						reject(Error::New("Candidate cannot be used.", __FILE__, __LINE__));
						}));
			}
			else {
				if (_socket->AddIceCandidate(ice)) {
					return resolve();
				}

				if (error.description.empty()) {
					if (!_socket->pending_remote_description() && !_socket->current_remote_description()) {
						return reject(Error::New("ICE candidates can't be added without any remote session description.", __FILE__, __LINE__));
					}

					return reject(Error::New("Candidate cannot be used.", __FILE__, __LINE__));
				}
			}
		}

		return reject(Error::New(error.description, __FILE__, __LINE__));
		});
}

void RTCPeerConnectionInternal::AddStream(const Let<MediaStream>& stream) {
	_socket->AddStream(reinterpret_cast<webrtc::MediaStreamInterface*>(stream->GetStream()));
}

/*
Let<RTCPeerConnection::RTCRtpSender> RTCPeerConnectionInternal::AddTrack(const Let<MediaStreamTrack> &track, const Let<MediaStream> &stream) {
  // TODO(): Implement this
  return Let<RTCPeerConnection::RTCRtpSender>();
}
*/

Let<Promise<RTCPeerConnection::RTCSessionDescription>> RTCPeerConnectionInternal::CreateAnswer(const RTCPeerConnection::RTCAnswerOptions& options) {
	Let<RTCPeerConnection> self(this);

	return Promise<RTCPeerConnection::RTCSessionDescription>::New([=](
		const Promise<RTCPeerConnection::RTCSessionDescription>::FullFilledCallback& resolve,
		const Promise<RTCPeerConnection::RTCSessionDescription>::RejectedCallback& reject)
		{
			rtc::scoped_refptr<CreateOfferAnswerObserver> observer = rtc::make_ref_counted<CreateOfferAnswerObserver>(resolve, reject);
			webrtc::PeerConnectionInterface::RTCOfferAnswerOptions answer_options(
				true, // offer_to_receive_video
				true, // offer_to_receive_audio
				options.voiceActivityDetection, // voice_activity_detection
				false, // ice_restart 
				true  // use_rtp_mux
			);

			if (observer.get()) {
				_socket->CreateAnswer(observer.get(), answer_options);
			}
			else {
				reject(Error::New("CreateOfferAnswerObserver Failed", __FILE__, __LINE__));
			}
		});
}

Let<Promise<RTCPeerConnection::RTCSessionDescription>> RTCPeerConnectionInternal::CreateOffer(const RTCPeerConnection::RTCOfferOptions& options) {
	Let<RTCPeerConnection> self(this);

	return Promise<RTCPeerConnection::RTCSessionDescription>::New([=](
		const Promise<RTCPeerConnection::RTCSessionDescription>::FullFilledCallback& resolve,
		const Promise<RTCPeerConnection::RTCSessionDescription>::RejectedCallback& reject)
		{
			rtc::scoped_refptr<CreateOfferAnswerObserver> observer = rtc::make_ref_counted<CreateOfferAnswerObserver>(resolve, reject);
			webrtc::PeerConnectionInterface::RTCOfferAnswerOptions offer_options(
				true, // offer_to_receive_video
				true, // offer_to_receive_audio
				options.voiceActivityDetection, // voice_activity_detection
				options.iceRestart, // ice_restart 
				true  // use_rtp_mux
			);

			if (observer.get()) {
				_socket->CreateOffer(observer.get(), offer_options);
			}
			else {
				reject(Error::New("CreateOfferAnswerObserver Failed", __FILE__, __LINE__));
			}
		});
}

/*
Let<Promise<RTCPeerConnection::RTCCertificate>> RTCPeerConnectionInternal::GenerateCertificate() {
  Let<RTCPeerConnection> self(this);

  return Promise<RTCPeerConnection::RTCCertificate>::New([=](
	const Promise<RTCPeerConnection::RTCCertificate>::FullFilledCallback &resolve,
	const Promise<RTCPeerConnection::RTCCertificate>::RejectedCallback &reject)
  {
	  // TODO(): Implement this

  });
}
*/

MediaStreams RTCPeerConnectionInternal::GetLocalStreams() {
	MediaStreams streams;
	rtc::scoped_refptr<webrtc::StreamCollectionInterface> lstreams(_socket->local_streams());

	for (size_t index = 0; index < lstreams->count(); index++) {
		Let<MediaStream> stream = MediaStreamInternal::New(lstreams->at(index));

		if (!stream.IsEmpty()) {
			streams.push_back(stream);
		}
	}

	return streams;
}

MediaStreams RTCPeerConnectionInternal::GetRemoteStreams() {
	MediaStreams streams;
	rtc::scoped_refptr<webrtc::StreamCollectionInterface> rstreams(_socket->remote_streams());

	for (size_t index = 0; index < rstreams->count(); index++) {
		Let<MediaStream> stream = MediaStreamInternal::New(rstreams->at(index));

		if (!stream.IsEmpty()) {
			streams.push_back(stream);
		}
	}

	return streams;
}

void RTCPeerConnectionInternal::RemoveStream(const Let<MediaStream>& stream) {
	_socket->RemoveStream(reinterpret_cast<webrtc::MediaStreamInterface*>(stream->GetStream()));
}

/*
void RTCPeerConnectionInternal::RemoveTrack(const Let<RTCPeerConnection::RTCRtpSender> &sender) {
  // TODO(): Implement this
}
*/

void RTCPeerConnectionInternal::SetConfiguration(const RTCPeerConnection::RTCConfiguration& config) {
	webrtc::PeerConnectionInterface::RTCConfiguration cfg(webrtc::PeerConnectionInterface::RTCConfigurationType::kAggressive);

	Let<Error> error = ParseConfiguration(config, &cfg);

	if (error.IsEmpty()) {
		_socket->SetConfiguration(cfg);
	}
}

Let<Promise<void> > RTCPeerConnectionInternal::SetLocalDescription(const RTCPeerConnection::RTCSessionDescription& sdp) {
	Let<RTCPeerConnection> self(this);

	return Promise<void>::New([=](
		const Promise<void>::FullFilledCallback& resolve,
		const Promise<void>::RejectedCallback& reject)
		{
			webrtc::SessionDescriptionInterface* desc = nullptr;
			Let<Error> error = SDP2SDP(sdp, &desc);

			if (error.IsEmpty()) {
				rtc::scoped_refptr<SetSessionDescriptionObserver> observer = rtc::make_ref_counted<SetSessionDescriptionObserver>(resolve, reject);
				_socket->SetLocalDescription(observer.get(), desc);
			}
			else {
				reject(error);
			}
		});
}

Let<Promise<void> > RTCPeerConnectionInternal::SetRemoteDescription(const RTCPeerConnection::RTCSessionDescription& sdp) {
	Let<RTCPeerConnection> self(this);

	return Promise<void>::New([=](
		const Promise<void>::FullFilledCallback& resolve,
		const Promise<void>::RejectedCallback& reject)
		{
			webrtc::SessionDescriptionInterface* desc = nullptr;
			Let<Error> error = SDP2SDP(sdp, &desc);

			if (error.IsEmpty()) {
				auto promise = Promise<void>::New([=](const Promise<void>::FullFilledCallback& res, const Promise<void>::RejectedCallback& rej) {
					rtc::scoped_refptr<SetSessionDescriptionObserver> observer = rtc::make_ref_counted<SetSessionDescriptionObserver>(res, rej);
					_socket->SetRemoteDescription(observer.get(), desc);
					})->Then([=]() {
						if (_pending_candidates.size()) {
							for (const auto& callback : _pending_candidates) {
								callback();
							}

							_pending_candidates.clear();
						}

						resolve();
						})->Catch([=](const Let<Error>& error) {
							reject(error);
							});

			}
			else {
				reject(error);
			}
		});
}

void RTCPeerConnectionInternal::Close() {
	if (_socket->signaling_state() != webrtc::PeerConnectionInterface::kClosed) {
		_socket->Close();
	}
}


RTCPeerConnection::RTCSessionDescription RTCPeerConnectionInternal::CurrentLocalDescription() {
	RTCPeerConnection::RTCSessionDescription sdp;
	SDP2SDP(_socket->current_local_description(), &sdp);
	return sdp;
}

RTCPeerConnection::RTCSessionDescription RTCPeerConnectionInternal::CurrentRemoteDescription() {
	RTCPeerConnection::RTCSessionDescription sdp;
	SDP2SDP(_socket->current_remote_description(), &sdp);
	return sdp;
}

RTCPeerConnection::RTCSessionDescription RTCPeerConnectionInternal::LocalDescription() {
	RTCPeerConnection::RTCSessionDescription sdp;
	SDP2SDP(_socket->local_description(), &sdp);
	return sdp;
}

RTCPeerConnection::RTCSessionDescription RTCPeerConnectionInternal::PendingLocalDescription() {
	RTCPeerConnection::RTCSessionDescription sdp;
	SDP2SDP(_socket->pending_local_description(), &sdp);
	return sdp;
}

RTCPeerConnection::RTCSessionDescription RTCPeerConnectionInternal::PendingRemoteDescription() {
	RTCPeerConnection::RTCSessionDescription sdp;
	SDP2SDP(_socket->pending_remote_description(), &sdp);
	return sdp;
}

RTCPeerConnection::RTCSessionDescription RTCPeerConnectionInternal::RemoteDescription() {
	RTCPeerConnection::RTCSessionDescription sdp;
	SDP2SDP(_socket->remote_description(), &sdp);
	return sdp;
}

RTCPeerConnection::RTCIceConnectionState RTCPeerConnectionInternal::IceConnectionState() {
	switch (_socket->ice_connection_state()) {
	case webrtc::PeerConnectionInterface::kIceConnectionNew:
		return RTCPeerConnection::kNew;
	case webrtc::PeerConnectionInterface::kIceConnectionChecking:
		return RTCPeerConnection::kChecking;
	case webrtc::PeerConnectionInterface::kIceConnectionConnected:
	case webrtc::PeerConnectionInterface::kIceConnectionMax:
		return RTCPeerConnection::kConnected;
	case webrtc::PeerConnectionInterface::kIceConnectionCompleted:
		return RTCPeerConnection::kCompleted;
	case webrtc::PeerConnectionInterface::kIceConnectionFailed:
		return RTCPeerConnection::kFailed;
	case webrtc::PeerConnectionInterface::kIceConnectionDisconnected:
		return RTCPeerConnection::kDisconnected;
	case webrtc::PeerConnectionInterface::kIceConnectionClosed:
		return RTCPeerConnection::kClosed;
	}

	return RTCPeerConnection::kNew;
}

RTCPeerConnection::RTCIceGatheringState RTCPeerConnectionInternal::IceGatheringState() {
	switch (_socket->ice_gathering_state()) {
	case webrtc::PeerConnectionInterface::kIceGatheringNew:
		return RTCPeerConnection::kNewGathering;
	case webrtc::PeerConnectionInterface::kIceGatheringGathering:
		return RTCPeerConnection::kGathering;
	case webrtc::PeerConnectionInterface::kIceGatheringComplete:
		return RTCPeerConnection::kComplete;
	}

	return RTCPeerConnection::kNewGathering;
}

RTCPeerConnection::RTCSignalingState RTCPeerConnectionInternal::SignalingState() {
	switch (_socket->signaling_state()) {
	case webrtc::PeerConnectionInterface::kStable:
		return RTCPeerConnection::kStable;
	case webrtc::PeerConnectionInterface::kHaveLocalOffer:
		return RTCPeerConnection::kHaveLocalOffer;
	case webrtc::PeerConnectionInterface::kHaveLocalPrAnswer:
		return RTCPeerConnection::kHaveLocalPrAnswer;
	case webrtc::PeerConnectionInterface::kHaveRemoteOffer:
		return RTCPeerConnection::kHaveRemoteOffer;
	case webrtc::PeerConnectionInterface::kHaveRemotePrAnswer:
		return RTCPeerConnection::kHaveRemotePrAnswer;
	case webrtc::PeerConnectionInterface::kClosed:
		return RTCPeerConnection::kSignalingClosed;
	}

	return RTCPeerConnection::kStable;
}

void RTCPeerConnectionInternal::OnSignalingChange(webrtc::PeerConnectionInterface::SignalingState new_state) {
	onsignalingstatechange();

	if (new_state == webrtc::PeerConnectionInterface::kClosed) {
		_event.Dispose();
	}
	else if (_event.IsEmpty()) {
		_event = Event::New();
	}
}

void RTCPeerConnectionInternal::OnAddStream(rtc::scoped_refptr<webrtc::MediaStreamInterface> stream) {
	onaddstream(MediaStreamInternal::New(stream.get()));
}

void RTCPeerConnectionInternal::OnRemoveStream(rtc::scoped_refptr<webrtc::MediaStreamInterface> stream) {
	onremovestream(MediaStreamInternal::New(stream.get()));
}

void RTCPeerConnectionInternal::OnTrack(rtc::scoped_refptr<webrtc::RtpTransceiverInterface> transceiver)
{
	onaddtrack(MediaStreamTrackInternal::New(transceiver->receiver()->track().get()));
}

void RTCPeerConnectionInternal::OnRemoveTrack(rtc::scoped_refptr<webrtc::RtpReceiverInterface> receiver)
{
	onremovetrack(MediaStreamTrackInternal::New(receiver->track().get()));
}

void RTCPeerConnectionInternal::OnDataChannel(rtc::scoped_refptr<webrtc::DataChannelInterface> data_channel) {
	if (data_channel.get()) {
		Let<RTCDataChannel> channel = Let<RTCDataChannelInternal>::New(data_channel);

		if (!channel.IsEmpty()) {
			ondatachannel(channel);
		}
	}
}

void RTCPeerConnectionInternal::OnRenegotiationNeeded() {
	onnegotiationneeded();
}

void RTCPeerConnectionInternal::OnIceConnectionChange(webrtc::PeerConnectionInterface::IceConnectionState new_state) {
	oniceconnectionstatechange();
}

void RTCPeerConnectionInternal::OnIceGatheringChange(webrtc::PeerConnectionInterface::IceGatheringState new_state) {
	onicegatheringstatechange();
}

void RTCPeerConnectionInternal::OnIceCandidate(const webrtc::IceCandidateInterface* candidate) {
	RTCPeerConnection::RTCIceCandidate iceCandidate;

	iceCandidate.sdpMid = candidate->sdp_mid();
	iceCandidate.sdpMLineIndex = candidate->sdp_mline_index();

	if (candidate->ToString(&iceCandidate.candidate)) {
		onicecandidate(iceCandidate);
	}
}

void RTCPeerConnectionInternal::OnIceCandidatesRemoved(const std::vector<cricket::Candidate>& candidates) {
	onicecandidatesremoved();
}

void RTCPeerConnectionInternal::OnIceConnectionReceivingChange(bool receiving) {
	//oniceconnectionstatechange();
}

// DEPRECATED -> //
/*
void RTCPeerConnectionInternal::OnAddStream(webrtc::MediaStreamInterface* stream) {

}

void RTCPeerConnectionInternal::OnDataChannel(webrtc::DataChannelInterface* data_channel) {

}

void RTCPeerConnectionInternal::OnRemoveStream(webrtc::MediaStreamInterface* stream) {

}
*/
// <- DEPRECATED //


Let<RTCPeerConnection> RTCPeerConnection::New(const RTCPeerConnection::RTCConfiguration& config) {
	return Let<RTCPeerConnectionInternal>::New(config);
}

RTCPeerConnection::RTCConfiguration::RTCConfiguration() :
	iceCandidatePoolSize(0),
	bundlePolicy(kMaxBundle),
	iceTransportPolicy(kAll),
	rtcpMuxPolicy(kRequire)
{ 
	RTCIceServer iceserver;
	iceserver.urls.push_back(std::string("stun:stun.l.google.com:19302"));
	iceServers.push_back(iceserver);
}

RTCPeerConnection::RTCConfiguration::~RTCConfiguration() {

}

RTCPeerConnection::RTCPeerConnection() {

}

RTCPeerConnection::~RTCPeerConnection() {

}