#ifndef CRTC_PROMISE_H
#define CRTC_PROMISE_H

#include <memory>
#include <condition_variable>
#include <mutex>

namespace crtc {
	/// \sa https://developer.mozilla.org/en/docs/Web/JavaScript/Reference/Global_Objects/Promise

	template <typename... Args> class Promise {
		Promise<Args...>(const Promise<Args...>&) = delete;
		Promise<Args...>& operator=(const Promise<Args...>&) = delete;

	public:
		typedef std::function<void(Args...)> FullFilledCallback;
		typedef std::function<void()> FinallyCallback;
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

					self->_completed = true;

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

						self->_completed = true;

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

		inline Promise<Args...>* WaitForResult() {
			std::unique_lock<std::mutex> lk(_mtx);
			_cnd.wait(lk, [=] { return _completed; });
			return this;
		}

	private:
		std::condition_variable _cnd;
		std::mutex _mtx;
		bool _completed;

		std::vector<FullFilledCallback> _onresolve;
		std::vector<RejectedCallback> _onreject;
		std::vector<FinallyCallback> _onfinally;
	};
}

#endif