
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

/**
\mainpage WebRTC C++ library
WebRTC (Web Real-Time Communication) is a collection of communications protocols and application programming interfaces that enable real-time communication over peer-to-peer connections.
This allows web browsers to not only request resources from backend servers, but also real-time information from browsers of other users.

This enables applications such as video conferencing, file transfer, chat, or desktop sharing without the need of either internal or external plugins.

WebRTC is being standardized by the World Wide Web Consortium (W3C) and the Internet Engineering Task Force (IETF).
The reference implementation is released as free software under the terms of a BSD license.

WebRTC uses Real-Time Protocol to transfer audio and video.
\sa https://webrtc.org/
*/

/** @file */

#ifndef INCLUDE_CRTC_H_
#define INCLUDE_CRTC_H_

#include <memory>
#include <vector>
#include "utils.hpp"

namespace crtc {

	class CRTC_EXPORT Atomic {
		explicit Atomic() = delete;
		Atomic(const Atomic&) = delete;
		Atomic& operator=(const Atomic&) = delete;

	public:
		static intptr_t Increment(intptr_t* arg);
		static intptr_t Decrement(intptr_t* arg);
		static intptr_t AcquireLoad(intptr_t* arg);
	};

	class CRTC_EXPORT Time {
		explicit Time() = delete;
		Time(const Time&) = delete;
		Time& operator=(const Time&) = delete;

	public:
		static int64_t Now(); // returns current time in milliseconds
		static int64_t Diff(int64_t begin, int64_t end = Now()); // returns milliseconds
		static double Since(int64_t begin, int64_t end = Now()); // returns seconds
	};


	typedef std::function<void()> Callback;

	class CRTC_EXPORT Async {
		explicit Async() = delete;
		Async(const Async&) = delete;
		Async& operator=(const Async&) = delete;
	public:
		static void Call(Callback callback, int delayMs = 0);
	};

	/// \sa https://developer.mozilla.org/en/docs/Web/API/Window/SetImmediate

	template <typename F, typename... Args> static inline void SetImmediate(F&& func, Args... args) {
		std::function<void(Args...)> callback(func);

		Async::Call(Callback([=]() {
			callback(args...);
			}, [=]() {
				callback(args...);
				}));
	}

	/// \sa https://developer.mozilla.org/en-US/docs/Web/API/WindowTimers/setTimeout

	template <typename F, typename... Args> static inline void SetTimeout(F&& func, int delay, Args... args) {
		std::function<void(Args... args)> callback(func);

		Async::Call(Callback([=]() {
			callback(args...);
			}, [=]() {
				callback(args...);
				}), (delay > 0) ? delay : 0);
	}

	/// \sa https://developer.mozilla.org/en/docs/Web/JavaScript/Reference/Global_Objects/Error

	class CRTC_EXPORT Error {
	public:
		explicit Error() { }
		virtual ~Error() { }

		static std::shared_ptr<Error> New(String message, String fileName = String(reinterpret_cast<const char*>(__FILE__)), int lineNumber = __LINE__);

		virtual String Message() const = 0;
		virtual String FileName() const = 0;
		virtual int LineNumber() const = 0;

		virtual String ToString() const = 0;
	};

	typedef synchronized_callback<std::shared_ptr<Error>> ErrorCallback;

	/// \sa https://developer.mozilla.org/en/docs/Web/JavaScript/Reference/Global_Objects/Promise

	template <typename... Args> class Promise {
		Promise<Args...>(const Promise<Args...>&) = delete;
		Promise<Args...>& operator=(const Promise<Args...>&) = delete;

	public:
		typedef std::function<void(Args...)> FullFilledCallback;
		typedef Callback FinallyCallback;
		typedef std::function<void(std::shared_ptr<Error>)> RejectedCallback;
		typedef std::function<void(const FullFilledCallback&, const RejectedCallback&)> ExecutorCallback;

		explicit Promise() { }
		virtual ~Promise() { }

		inline static std::shared_ptr<Promise<Args...>> New(const ExecutorCallback& executor) {
			auto self = std::make_shared<Promise<Args...>>();

			RejectedCallback reject([=](const std::shared_ptr<Error>& error) {
				if (self) {
					for (const auto& callback : self->_onreject) {
						callback(error);
					}

					for (const auto& callback : self->_onfinally) {
						callback();
					}

					self->_onfinally.clear();
					self->_onreject.clear();
					self->_onresolve.clear();
				}
				});

			RejectedCallback asyncReject([=](const std::shared_ptr<Error>& error) {
				Async::Call([=]() { reject(error); }); 
				});

			FullFilledCallback resolve([=](Args... args) {
				Async::Call([=]() {
					if (self) {
						for (const auto& callback : self->_onresolve) {
							callback(std::move(args)...);
						}

						for (const auto& callback : self->_onfinally) {
							callback();
						}

						self->_onfinally.clear();
						self->_onreject.clear();
						self->_onresolve.clear();
					}
					});
				});

			if (executor) {
				executor(resolve, asyncReject);
			}
			else {
				asyncReject(Error::New("Invalid Executor Callback.", __FILE__, __LINE__));
			}

			return self;
		}

		inline Promise<Args...>* Then(const FullFilledCallback& callback) {
			_onresolve.push_back(callback);
			return this;
		}

		inline Promise<Args...>* Catch(const RejectedCallback& callback) {
			_onreject.push_back(callback);
			return this;
		}

		inline Promise<Args...>* Finally(const FinallyCallback& callback) {
			_onfinally.push_back(callback);
			return this;
		}

	private:
		std::vector<FullFilledCallback> _onresolve;
		std::vector<RejectedCallback> _onreject;
		std::vector<FinallyCallback> _onfinally;
	};

	//template<> class Promise<void> : public Promise<> { };

	/// \sa https://developer.mozilla.org/en/docs/Web/JavaScript/Reference/Global_Objects/ArrayBuffer

	class CRTC_EXPORT ArrayBuffer {
		ArrayBuffer(const ArrayBuffer&) = delete;
		ArrayBuffer& operator=(const ArrayBuffer&) = delete;

	public:
		explicit ArrayBuffer() { }
		virtual ~ArrayBuffer() { }

		static std::shared_ptr<ArrayBuffer> New(size_t byteLength = 0);
		static std::shared_ptr<ArrayBuffer> New(const String& data);
		static std::shared_ptr<ArrayBuffer> New(const uint8_t* data, size_t byteLength = 0);

		virtual size_t ByteLength() const = 0;

		virtual std::shared_ptr<ArrayBuffer> Slice(size_t begin = 0, size_t end = 0) const = 0;

		virtual uint8_t* Data() = 0;
		virtual const uint8_t* Data() const = 0;

		virtual String ToString() const = 0;
	};

	/// \sa https://developer.mozilla.org/en-US/docs/Web/JavaScript/Reference/Global_Objects/TypedArray

	template <typename T> class CRTC_EXPORT TypedArray {
	public:
		explicit TypedArray(size_t length = 0) :
			TypedArray(ArrayBuffer::New(length * sizeof(T)))
		{ }

		TypedArray(const T* data, size_t length = 0) :
			TypedArray(ArrayBuffer::New(data, length * sizeof(T)))
		{ }

		TypedArray(const TypedArray<T>& typedArray) :
			_empty(0),
			_data(typedArray._data),
			_length(typedArray._length),
			_byteOffset(typedArray._byteOffset),
			_byteLength(typedArray._byteLength),
			_buffer(typedArray._buffer)
		{ }

		template <typename N> TypedArray(const TypedArray<N>& typedArray) :
			TypedArray(typedArray.Buffer())
		{ }

		TypedArray(const std::shared_ptr<ArrayBuffer>& buffer, size_t byteOffset = 0, size_t byteLength = 0) :
			_empty(0),
			_data(nullptr),
			_length(0),
			_byteOffset(0),
			_byteLength(0),
			_buffer(buffer)
		{
			if (!_buffer) {
				_buffer = ArrayBuffer::New(byteLength - byteOffset);
			}

			byteLength = (!byteLength) ? _buffer->ByteLength() : byteLength;
			byteLength -= byteOffset;

			if (byteLength && (byteLength % sizeof(T)) == 0) {
				_data = reinterpret_cast<T*>(buffer->Data() + byteOffset);
				_byteOffset = byteOffset;
				_byteLength = byteLength;
				_length = byteLength / sizeof(T);
			}
		}

		inline size_t Length() const {
			return _length;
		}

		inline size_t ByteOffset() const {
			return _byteOffset;
		}

		inline size_t ByteLength() const {
			return _byteLength;
		}

		inline std::shared_ptr<ArrayBuffer> Buffer() const {
			return _buffer;
		}

		inline std::shared_ptr<ArrayBuffer> Slice(size_t begin = 0, size_t end = 0) const {
			if (_length) {
				return _buffer->Slice(begin * sizeof(T), end * sizeof(T));
			}

			return ArrayBuffer::New();
		}

		inline T* Data() {
			return (_length) ? _data : nullptr;
		}

		inline const T* Data() const {
			return (_length) ? _data : nullptr;
		}

		inline T& Get(const size_t index) {
			if (index < _length) {
				return _data[index];
			}

			return _empty;
		}

		inline const T& Get(const size_t index) const {
			if (index < _length) {
				return _data[index];
			}

			return TypedArray<T>::empty;
		}

		inline void Set(const size_t index, const T& value) {
			if (index < _length) {
				_data[index] = index;
			}
		}

		inline T& operator[](const size_t index) {
			if (index < _length) {
				return _data[index];
			}

			return _empty;
		}

		inline const T& operator[](const size_t index) const {
			if (index < _length) {
				return _data[index];
			}

			return TypedArray<T>::empty;
		}

	protected:
		static const T empty = 0;

		T _empty;
		T* _data;
		size_t _length;
		size_t _byteOffset;
		size_t _byteLength;
		std::shared_ptr<ArrayBuffer> _buffer;
	};

	/// \sa https://developer.mozilla.org/en-US/docs/Web/JavaScript/Reference/Global_Objects/Int8Array

	typedef TypedArray<int8_t> Int8Array;

	/// \sa https://developer.mozilla.org/en-US/docs/Web/JavaScript/Reference/Global_Objects/Uint8Array 

	typedef TypedArray<uint8_t> Uint8Array;

	/// \sa https://developer.mozilla.org/en-US/docs/Web/JavaScript/Reference/Global_Objects/Int16Array 

	typedef TypedArray<int16_t> Int16Array;

	/// \sa https://developer.mozilla.org/en-US/docs/Web/JavaScript/Reference/Global_Objects/Uint16Array 

	typedef TypedArray<uint16_t> Uint16Array;

	/// \sa https://developer.mozilla.org/en-US/docs/Web/JavaScript/Reference/Global_Objects/Int32Array 

	typedef TypedArray<int32_t> Int32Array;

	/// \sa https://developer.mozilla.org/en-US/docs/Web/JavaScript/Reference/Global_Objects/Uint32Array 

	typedef TypedArray<uint32_t> Uint32Array;

	class CRTC_EXPORT Module {
		explicit Module() = delete;
		Module(const Module&) = delete;
		Module& operator=(const Module&) = delete;

	public:
		static void Init();
		static bool DispatchEvents(bool kForever = false);
		static void Dispose();
		static void RegisterAsyncCallback(const Callback& callback);
		static void UnregisterAsyncCallback();
	};

	class CRTC_EXPORT VideoFrame {
		VideoFrame(const VideoFrame&) = delete;
		VideoFrame& operator=(const VideoFrame&) = delete;

	public:
		explicit VideoFrame() { }
		virtual ~VideoFrame() { }

		virtual uint8_t* Data() = 0;
		virtual const uint8_t* Data() const = 0;
		virtual size_t ByteLength() const = 0;
		virtual uint32_t TimeStamp() const;

	protected:
		uint32_t _timestamp = 0;
	};

	/// \sa https://developer.mozilla.org/en-US/docs/Web/API/MediaStreamTrack
	class CRTC_EXPORT MediaStreamTrack {
		MediaStreamTrack(const MediaStreamTrack&) = delete;
		MediaStreamTrack& operator=(const MediaStreamTrack&) = delete;

	public:
		enum Type {
			kAudio,
			kVideo,
		};

		enum State {
			kLive,
			kEnded,
		};

		explicit MediaStreamTrack();
		virtual ~MediaStreamTrack();

		virtual bool Enabled() const = 0;
		virtual bool Muted() const = 0;
		virtual bool Remote() const = 0;
		virtual String Id() const = 0;

		virtual Type Kind() const = 0;
		virtual State ReadyState() const = 0;

		/// \sa https://developer.mozilla.org/en-US/docs/Web/API/MediaStreamTrack/clone

		virtual std::shared_ptr<MediaStreamTrack> Clone() = 0;

		Callback onstarted;
		Callback onended;
		Callback onmute;
		Callback onunmute;

		synchronized_callback<const void*, int, int, size_t, size_t> onAudio;
		synchronized_callback<std::shared_ptr<VideoFrame>> onVideo;
		synchronized_callback<> onFrameDrop;
	};

	typedef std::vector<std::shared_ptr<MediaStreamTrack>> MediaStreamTracks;

	/// \sa https://developer.mozilla.org/en-US/docs/Web/API/MediaStream

	class CRTC_EXPORT MediaStream {
		MediaStream(const MediaStream&) = delete;
		MediaStream& operator=(const MediaStream&) = delete;

	public:
		virtual String Id() const = 0;

		/// \sa https://developer.mozilla.org/en-US/docs/Web/API/MediaStream/addTrack

		virtual void AddTrack(const std::shared_ptr<MediaStreamTrack>& track) = 0;
		virtual void RemoveTrack(const std::shared_ptr<MediaStreamTrack>& track) = 0;

		/// \sa https://developer.mozilla.org/en-US/docs/Web/API/MediaStream/getTrackById

		virtual std::shared_ptr<MediaStreamTrack> GetTrackById(const String& id) const = 0;

		virtual intptr_t GetStream() = 0;

		virtual MediaStreamTracks GetAudioTracks() const = 0;
		virtual MediaStreamTracks GetVideoTracks() const = 0;

		/// \sa https://developer.mozilla.org/en-US/docs/Web/API/MediaStream/clone

		virtual std::shared_ptr<MediaStream> Clone() = 0;

		synchronized_callback<const std::shared_ptr<MediaStreamTrack>&> onaddtrack;
		synchronized_callback<const std::shared_ptr<MediaStreamTrack>&> onremovetrack;

	protected:
		explicit MediaStream();
		~MediaStream();
	};

	typedef std::vector<std::shared_ptr<MediaStream>> MediaStreams;

	class CRTC_EXPORT AudioBuffer : virtual public ArrayBuffer {
		AudioBuffer(const AudioBuffer&) = delete;
		AudioBuffer& operator=(const AudioBuffer&) = delete;

	public:
		explicit AudioBuffer() { }
		virtual ~AudioBuffer() { }

		static std::shared_ptr<AudioBuffer> New(int channels = 2, int sampleRate = 44100, int bitsPerSample = 8, int frames = 1);
		static std::shared_ptr<AudioBuffer> New(const std::shared_ptr<ArrayBuffer>& buffer, int channels = 2, int sampleRate = 44100, int bitsPerSample = 8, int frames = 1);

		virtual int Channels() const = 0;
		virtual int SampleRate() const = 0;
		virtual int BitsPerSample() const = 0;
		virtual int Frames() const = 0;
	};

	class CRTC_EXPORT AudioSource : virtual public MediaStream {
		AudioSource(const AudioSource&) = delete;
		AudioSource& operator=(const AudioSource&) = delete;

	public:
		explicit AudioSource();
		virtual ~AudioSource();

		static std::shared_ptr<AudioSource> New();

		virtual bool IsRunning() const = 0;
		virtual void Stop() = 0;

		virtual void Write(const std::shared_ptr<AudioBuffer>& buffer, ErrorCallback callback = ErrorCallback()) = 0;

		Callback ondrain;
	};

	class CRTC_EXPORT ImageBuffer : public ArrayBuffer {
		ImageBuffer(const ImageBuffer&) = delete;
		ImageBuffer& operator=(const ImageBuffer&) = delete;

	public:
		explicit ImageBuffer() { }
		virtual ~ImageBuffer() { }

		static std::shared_ptr<ImageBuffer> New(int width, int height);
		static std::shared_ptr<ImageBuffer> New(const std::shared_ptr<ArrayBuffer>& buffer, int width, int height);

		static size_t ByteLength(int height, int stride_y, int stride_u, int stride_v);
		static size_t ByteLength(int width, int height);

		virtual int Width() const = 0;
		virtual int Height() const = 0;

		virtual const uint8_t* DataY() const = 0;
		virtual const uint8_t* DataU() const = 0;
		virtual const uint8_t* DataV() const = 0;

		virtual int StrideY() const = 0;
		virtual int StrideU() const = 0;
		virtual int StrideV() const = 0;
	};

	class CRTC_EXPORT VideoSource : virtual public MediaStream {
		VideoSource(const VideoSource&) = delete;
		VideoSource& operator=(const VideoSource&) = delete;

	public:
		explicit VideoSource();
		virtual ~VideoSource();

		static std::shared_ptr<VideoSource> New(int width = 1280, int height = 720, float fps = 30);

		virtual bool IsRunning() const = 0;
		virtual void Stop() = 0;

		virtual int Width() const = 0;
		virtual int Height() const = 0;
		virtual float Fps() const = 0;

		virtual void Write(const std::shared_ptr<ImageBuffer>& frame, ErrorCallback callback = ErrorCallback()) = 0;

		Callback ondrain;
	};

	/// \sa https://developer.mozilla.org/en/docs/Web/API/RTCDataChannel

	class CRTC_EXPORT RTCDataChannel {
		RTCDataChannel(const RTCDataChannel&) = delete;
		RTCDataChannel& operator=(const RTCDataChannel&) = delete;

	public:
		typedef synchronized_callback<std::shared_ptr<ArrayBuffer>, bool> MessageCallback;

		enum State {
			kConnecting,
			kOpen,
			kClosing,
			kClosed
		};

		explicit RTCDataChannel();
		virtual ~RTCDataChannel();

		/// \sa https://developer.mozilla.org/en-US/docs/Web/API/RTCDataChannel/id

		virtual int Id() = 0;

		/// \sa https://developer.mozilla.org/en-US/docs/Web/API/RTCDataChannel/label

		virtual String Label() = 0;

		/// \sa https://developer.mozilla.org/en-US/docs/Web/API/RTCDataChannel/bufferedAmount

		virtual uint64_t BufferedAmount() = 0;

		/// \sa https://developer.mozilla.org/en-US/docs/Web/API/RTCDataChannel/bufferedAmountLowThreshold

		virtual uint64_t BufferedAmountLowThreshold() = 0;
		virtual void SetBufferedAmountLowThreshold(uint64_t threshold = 0) = 0;

		/// \sa https://developer.mozilla.org/en-US/docs/Web/API/RTCDataChannel/maxPacketLifeTime

		virtual uint16_t MaxPacketLifeTime() = 0;

		/// \sa https://developer.mozilla.org/en-US/docs/Web/API/RTCDataChannel/maxRetransmits

		virtual uint16_t MaxRetransmits() = 0;

		/// \sa https://developer.mozilla.org/en-US/docs/Web/API/RTCDataChannel/negotiated

		virtual bool Negotiated() = 0;

		/// \sa https://developer.mozilla.org/en-US/docs/Web/API/RTCDataChannel/ordered

		virtual bool Ordered() = 0;

		/// \sa https://developer.mozilla.org/en-US/docs/Web/API/RTCDataChannel/protocol

		virtual String Protocol() = 0;

		/// \sa https://developer.mozilla.org/en-US/docs/Web/API/RTCDataChannel/readyState

		virtual State ReadyState() = 0;

		/// \sa https://developer.mozilla.org/en-US/docs/Web/API/RTCDataChannel/close

		virtual void Close() = 0;

		/// \sa https://developer.mozilla.org/en-US/docs/Web/API/RTCDataChannel/send

		virtual void Send(const std::shared_ptr<ArrayBuffer>& data, bool binary = true) = 0;

		virtual void Send(const unsigned char* data, size_t length, bool binary = true) = 0;

		/// \sa https://developer.mozilla.org/en-US/docs/Web/API/RTCDataChannel/onbufferedamountlow

		Callback onbufferedamountlow;

		/// \sa https://developer.mozilla.org/en-US/docs/Web/API/RTCDataChannel/onclose

		Callback onclose;

		/// \sa https://developer.mozilla.org/en-US/docs/Web/API/RTCDataChannel/onerror

		ErrorCallback onerror;

		/// \sa https://developer.mozilla.org/en-US/docs/Web/API/RTCDataChannel/onmessage

		MessageCallback onmessage;

		/// \sa https://developer.mozilla.org/en-US/docs/Web/API/RTCDataChannel/onopen

		Callback onopen;
	};

	/// \sa https://developer.mozilla.org/en-US/docs/Web/API/RTCPeerConnection

	class CRTC_EXPORT RTCPeerConnection {
		RTCPeerConnection(const RTCPeerConnection&) = delete;
		RTCPeerConnection& operator=(const RTCPeerConnection&) = delete;

	public:
		/// \sa https://developer.mozilla.org/en-US/docs/Web/API/RTCPeerConnection/createDataChannel#RTCDataChannelInit_dictionary

		struct CRTC_EXPORT RTCDataChannelInit {
			RTCDataChannelInit() :
				id(-1),
				maxPacketLifeTime(-1),
				maxRetransmits(-1),
				ordered(true),
				negotiated(false),
				protocol()
			{ }

			int id;
			int maxPacketLifeTime;
			int maxRetransmits;
			bool ordered;
			bool negotiated;
			String protocol;
		};

		/// \sa https://developer.mozilla.org/en-US/docs/Web/API/RTCSessionDescription

		struct CRTC_EXPORT RTCSessionDescription {
			enum RTCSdpType {
				kAnswer,
				kOffer,
				kPranswer,
				kRollback,
			};

			RTCSdpType type;
			String sdp;
		};

		/// \sa https://developer.mozilla.org/en-US/docs/Web/API/RTCPeerConnection/signalingState

		enum RTCSignalingState {
			kStable,
			kHaveLocalOffer,
			kHaveLocalPrAnswer,
			kHaveRemoteOffer,
			kHaveRemotePrAnswer,
			kSignalingClosed,
		};

		/// \sa https://developer.mozilla.org/en-US/docs/Web/API/RTCPeerConnection/iceGatheringState

		enum RTCIceGatheringState {
			kNewGathering,
			kGathering,
			kComplete
		};

		/// \sa https://developer.mozilla.org/en-US/docs/Web/API/RTCPeerConnection/iceConnectionState

		enum RTCIceConnectionState {
			kNew,
			kChecking,
			kConnected,
			kCompleted,
			kFailed,
			kDisconnected,
			kClosed,
		};

		/// \sa https://developer.mozilla.org/en-US/docs/Web/API/RTCConfiguration#RTCBundlePolicy_enum

		enum RTCBundlePolicy {
			kBalanced,
			kMaxBundle,
			kMaxCompat
		};

		/// \sa https://developer.mozilla.org/en-US/docs/Web/API/RTCConfiguration#RTCIceTransportPolicy_enum

		enum RTCIceTransportPolicy {
			kRelay,
			kPublic,
			kAll
		};

		/// \sa https://developer.mozilla.org/en-US/docs/Web/API/RTCConfiguration#RTCRtcpMuxPolicy_enum

		enum RTCRtcpMuxPolicy {
			kNegotiate,
			kRequire,
		};

		// \sa https://developer.mozilla.org/en/docs/Web/API/RTCIceCandidate

		struct CRTC_EXPORT RTCIceCandidate {
			String candidate;
			String sdpMid;
			uint32_t sdpMLineIndex;
		};

		/// \sa https://developer.mozilla.org/en-US/docs/Web/API/RTCIceServer

		struct CRTC_EXPORT RTCIceServer {
			String credential;
			String credentialType;
			String username;
			std::vector<String> urls;
		};

		/// \sa https://developer.mozilla.org/en-US/docs/Web/API/RTCConfiguration

		struct CRTC_EXPORT RTCConfiguration {
			explicit RTCConfiguration();
			~RTCConfiguration();

			uint16_t iceCandidatePoolSize;
			RTCBundlePolicy bundlePolicy;
			std::vector<RTCIceServer> iceServers;
			RTCIceTransportPolicy iceTransportPolicy;
			RTCRtcpMuxPolicy rtcpMuxPolicy;
		};

		/// \sa https://developer.mozilla.org/en-US/docs/Web/API/RTCPeerConnection/createOffer#RTCOfferOptions_dictionary
		/// \sa https://w3c.github.io/webrtc-pc/#idl-def-rtcofferansweroptions

		struct CRTC_EXPORT RTCOfferAnswerOptions {
			bool voiceActivityDetection;
		};

		/// \sa https://w3c.github.io/webrtc-pc/#idl-def-rtcofferoptions

		struct CRTC_EXPORT RTCOfferOptions : RTCOfferAnswerOptions {
			bool iceRestart;
		};

		/// \sa https://w3c.github.io/webrtc-pc/#idl-def-rtcansweroptions

		struct CRTC_EXPORT RTCAnswerOptions : RTCOfferAnswerOptions {
		};

		typedef synchronized_callback<const std::shared_ptr<MediaStream>> StreamCallback;
		typedef synchronized_callback<const std::shared_ptr<MediaStreamTrack>> TrackCallback;
		typedef synchronized_callback<const std::shared_ptr<RTCDataChannel>> DataChannelCallback;
		typedef synchronized_callback<const RTCIceCandidate&> IceCandidateCallback;

		explicit RTCPeerConnection();
		virtual ~RTCPeerConnection();

		static std::shared_ptr<RTCPeerConnection> New(const RTCConfiguration& config = RTCConfiguration());

		virtual std::shared_ptr<RTCDataChannel> CreateDataChannel(const String& label, const RTCDataChannelInit& options = RTCDataChannelInit()) = 0;

		/// \sa https://developer.mozilla.org/en-US/docs/Web/API/RTCPeerConnection/addIceCandidate

		virtual std::shared_ptr<Promise<>> AddIceCandidate(const RTCIceCandidate& candidate) = 0;

		/// \sa https://developer.mozilla.org/en-US/docs/Web/API/RTCPeerConnection/addStream

		virtual void AddStream(const std::shared_ptr<MediaStream>& stream) = 0;

		/// \sa https://developer.mozilla.org/en-US/docs/Web/API/RTCPeerConnection/createAnswer

		virtual std::shared_ptr<Promise<RTCPeerConnection::RTCSessionDescription>> CreateAnswer(const RTCAnswerOptions& options = RTCAnswerOptions()) = 0;

		/// \sa https://developer.mozilla.org/en-US/docs/Web/API/RTCPeerConnection/createOffer

		virtual std::shared_ptr<Promise<RTCPeerConnection::RTCSessionDescription>> CreateOffer(const RTCOfferOptions& options = RTCOfferOptions()) = 0;

		/// \sa https://developer.mozilla.org/en-US/docs/Web/API/RTCPeerConnection/getLocalStreams

		virtual MediaStreams GetLocalStreams() = 0;

		/// \sa https://developer.mozilla.org/en-US/docs/Web/API/RTCPeerConnection/getRemoteStreams

		virtual MediaStreams GetRemoteStreams() = 0;

		/// \sa https://developer.mozilla.org/en-US/docs/Web/API/RTCPeerConnection/removeStream

		virtual void RemoveStream(const std::shared_ptr<MediaStream>& stream) = 0;

		/// \sa https://developer.mozilla.org/en-US/docs/Web/API/RTCPeerConnection/setConfiguration

		virtual void SetConfiguration(const RTCConfiguration& config) = 0;

		/// \sa https://developer.mozilla.org/en-US/docs/Web/API/RTCPeerConnection/setLocalDescription

		virtual std::shared_ptr<Promise<>> SetLocalDescription(const RTCSessionDescription& sdp) = 0;

		/// \sa https://developer.mozilla.org/en-US/docs/Web/API/RTCPeerConnection/setRemoteDescription

		virtual std::shared_ptr<Promise<>> SetRemoteDescription(const RTCSessionDescription& sdp) = 0;

		/// \sa https://developer.mozilla.org/en-US/docs/Web/API/RTCPeerConnection/close

		virtual void Close() = 0;

		virtual RTCSessionDescription CurrentLocalDescription() = 0;
		virtual RTCSessionDescription CurrentRemoteDescription() = 0;
		virtual RTCSessionDescription LocalDescription() = 0;
		virtual RTCSessionDescription PendingLocalDescription() = 0;
		virtual RTCSessionDescription PendingRemoteDescription() = 0;
		virtual RTCSessionDescription RemoteDescription() = 0;

		virtual RTCIceConnectionState IceConnectionState() = 0;
		virtual RTCIceGatheringState IceGatheringState() = 0;
		virtual RTCSignalingState SignalingState() = 0;

		Callback onnegotiationneeded;
		Callback onsignalingstatechange;
		Callback onicegatheringstatechange;
		Callback oniceconnectionstatechange;
		Callback onicecandidatesremoved;
		StreamCallback onaddstream;
		StreamCallback onremovestream;
		TrackCallback onaddtrack;
		TrackCallback onremovetrack;
		DataChannelCallback ondatachannel;
		IceCandidateCallback onicecandidate;
	};
} // namespace crtc

#endif // INCLUDE_CRTC_H_