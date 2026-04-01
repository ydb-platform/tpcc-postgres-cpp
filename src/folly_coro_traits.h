#pragma once

#include <folly/futures/Future.h>
#include <folly/futures/Promise.h>
#include <folly/Unit.h>
#include <folly/ExceptionWrapper.h>
#include <folly/executors/InlineExecutor.h>

#include <coroutine>
#include <exception>

// Coroutine promise_type for folly::SemiFuture<T> where T is not folly::Unit
namespace std {

template <typename T, typename... Args>
struct coroutine_traits<folly::SemiFuture<T>, Args...> {
    struct promise_type {
        folly::Promise<T> promise;

        folly::SemiFuture<T> get_return_object() {
            return promise.getSemiFuture();
        }

        std::suspend_never initial_suspend() noexcept { return {}; }
        std::suspend_never final_suspend() noexcept { return {}; }

        void return_value(T value) {
            promise.setValue(std::move(value));
        }

        void unhandled_exception() {
            promise.setException(
                folly::exception_wrapper(std::current_exception()));
        }
    };
};

template <typename... Args>
struct coroutine_traits<folly::SemiFuture<folly::Unit>, Args...> {
    struct promise_type {
        folly::Promise<folly::Unit> promise;

        folly::SemiFuture<folly::Unit> get_return_object() {
            return promise.getSemiFuture();
        }

        std::suspend_never initial_suspend() noexcept { return {}; }
        std::suspend_never final_suspend() noexcept { return {}; }

        void return_void() {
            promise.setValue(folly::unit);
        }

        void unhandled_exception() {
            promise.setException(
                folly::exception_wrapper(std::current_exception()));
        }
    };
};

} // namespace std

// Awaiter for co_await on folly::SemiFuture<T>
// Resumes the coroutine inline when the future completes.
template <typename T>
struct FollySemiFutureAwaiter {
    folly::SemiFuture<T> future;
    folly::Try<T> result;

    bool await_ready() {
        if (future.isReady()) {
            result = std::move(future).result();
            return true;
        }
        return false;
    }

    void await_suspend(std::coroutine_handle<> handle) {
        std::move(future)
            .via(&folly::InlineExecutor::instance())
            .thenTry([this, handle](folly::Try<T>&& t) mutable {
                result = std::move(t);
                handle.resume();
            });
    }

    T await_resume() {
        return std::move(result).value();
    }
};

template <typename T>
FollySemiFutureAwaiter<T> operator co_await(folly::SemiFuture<T>&& future) {
    return FollySemiFutureAwaiter<T>{std::move(future), {}};
}
