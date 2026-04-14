#pragma once

#include "future.h"

#include <coroutine>
#include <exception>

// Coroutine promise_type for TFuture<T>
namespace std {

template <typename T, typename... Args>
struct coroutine_traits<TFuture<T>, Args...> {
    struct promise_type {
        TPromise<T> promise;

        TFuture<T> get_return_object() {
            return promise.GetFuture();
        }

        std::suspend_never initial_suspend() noexcept { return {}; }
        std::suspend_never final_suspend() noexcept { return {}; }

        void return_value(T value) {
            promise.SetValue(std::move(value));
        }

        void unhandled_exception() {
            promise.SetException(std::current_exception());
        }
    };
};

template <typename... Args>
struct coroutine_traits<TFuture<void>, Args...> {
    struct promise_type {
        TPromise<void> promise;

        TFuture<void> get_return_object() {
            return promise.GetFuture();
        }

        std::suspend_never initial_suspend() noexcept { return {}; }
        std::suspend_never final_suspend() noexcept { return {}; }

        void return_void() {
            promise.SetValue();
        }

        void unhandled_exception() {
            promise.SetException(std::current_exception());
        }
    };
};

} // namespace std

// NOTE: a bare `operator co_await(TFuture<T>&&)` was intentionally removed.
// It would resume the coroutine inline on whatever thread sets the promise
// (typically an IO pool thread), breaking the single-threaded-per-context
// invariant required by the task queue. Always use TSuspendWithFuture from
// task_queue.h to co_await a TFuture — it enqueues the resumption on the
// correct task queue thread.
