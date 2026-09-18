#pragma once

// Opt-in coroutine layer; not included by <typesafe/typesafe.hpp>.
// Caller supplies a Scheduler. I/O is still blocking libcurl offloaded onto it.

#include <concepts>
#include <condition_variable>
#include <coroutine>
#include <exception>
#include <functional>
#include <memory>
#include <mutex>
#include <optional>
#include <queue>
#include <semaphore>
#include <thread>
#include <type_traits>
#include <utility>
#include <variant>
#include <vector>

#include "typesafe/answer.hpp"
#include "typesafe/client.hpp"
#include "typesafe/common.hpp"
#include "typesafe/question.hpp"

namespace typesafe::coro
{

template <class S>
concept Scheduler = std::invocable<S &, std::function<void()>>;

template <class T> class [[nodiscard]] Task
{
  public:
    struct promise_type
    {
        std::variant<std::monostate, T, std::exception_ptr> result;
        std::coroutine_handle<> continuation;

        Task get_return_object()
        {
            return Task{std::coroutine_handle<promise_type>::from_promise(*this)};
        }
        std::suspend_always initial_suspend() noexcept
        {
            return {};
        }

        struct final_awaiter
        {
            bool await_ready() noexcept
            {
                return false;
            }
            std::coroutine_handle<> await_suspend(std::coroutine_handle<promise_type> self) noexcept
            {
                auto cont = self.promise().continuation;
                return cont ? cont : std::noop_coroutine();
            }
            void await_resume() noexcept
            {
            }
        };
        final_awaiter final_suspend() noexcept
        {
            return {};
        }

        void return_value(T value)
        {
            result.template emplace<1>(std::move(value));
        }
        void unhandled_exception()
        {
            result.template emplace<2>(std::current_exception());
        }
    };

    using handle = std::coroutine_handle<promise_type>;

    Task() = default;
    explicit Task(handle h) : handle_(h)
    {
    }
    Task(Task &&other) noexcept : handle_(std::exchange(other.handle_, {}))
    {
    }
    Task &operator=(Task &&other) noexcept
    {
        if (this != &other)
        {
            if (handle_)
            {
                handle_.destroy();
            }
            handle_ = std::exchange(other.handle_, {});
        }
        return *this;
    }
    ~Task()
    {
        if (handle_)
        {
            handle_.destroy();
        }
    }

    bool await_ready() const noexcept
    {
        return false;
    }
    handle await_suspend(std::coroutine_handle<> continuation) noexcept
    {
        handle_.promise().continuation = continuation;
        return handle_; // symmetric transfer
    }
    T await_resume()
    {
        auto &result = handle_.promise().result;
        if (result.index() == 2)
        {
            std::rethrow_exception(std::get<2>(result));
        }
        return std::move(std::get<1>(result));
    }

  private:
    handle handle_{};
};

namespace detail
{

struct detached_task
{
    struct promise_type
    {
        detached_task get_return_object()
        {
            return {};
        }
        std::suspend_never initial_suspend() noexcept
        {
            return {};
        }
        std::suspend_never final_suspend() noexcept
        {
            return {};
        }
        void return_void()
        {
        }
        void unhandled_exception()
        {
            std::terminate();
        }
    };
};

template <class T> struct offload_state
{
    std::mutex mutex;
    bool done = false;
    std::optional<T> value;
    std::coroutine_handle<> waiter;
};

template <class T> struct offload_awaitable
{
    std::shared_ptr<offload_state<T>> state;

    bool await_ready() const
    {
        std::lock_guard lock(state->mutex);
        return state->done;
    }
    bool await_suspend(std::coroutine_handle<> handle)
    {
        std::lock_guard lock(state->mutex);
        if (state->done)
        {
            return false; // finished before suspend; resume inline
        }
        state->waiter = handle;
        return true;
    }
    T await_resume()
    {
        return std::move(*state->value);
    }
};

} // namespace detail

// Starts immediately so concurrent offload() calls overlap.
template <Scheduler S, class F> auto offload(S &scheduler, F func) -> detail::offload_awaitable<std::invoke_result_t<F>>
{
    using T = std::invoke_result_t<F>;
    auto state = std::make_shared<detail::offload_state<T>>();
    scheduler(std::function<void()>([state, func = std::move(func)]() mutable {
        T value = func();
        std::coroutine_handle<> waiter;
        {
            std::lock_guard lock(state->mutex);
            state->value.emplace(std::move(value));
            state->done = true;
            waiter = state->waiter;
        }
        if (waiter)
        {
            waiter.resume();
        }
    }));
    return detail::offload_awaitable<T>{std::move(state)};
}

template <class T> T sync_wait(Task<T> task)
{
    std::binary_semaphore done{0};
    std::optional<T> out;
    [](Task<T> inner, std::optional<T> &slot, std::binary_semaphore &signal) -> detail::detached_task {
        slot = co_await inner;
        signal.release();
    }(std::move(task), out, done);
    done.acquire();
    return std::move(*out);
}

// client and scheduler must outlive the task.
template <Scheduler S>
Task<Result<SystemOneResponse>> system_one(S &scheduler, const Client &client, Json state,
                                           typesafe::detail::QuestionList questions, SystemOneOpts opts = {})
{
    co_return co_await offload(scheduler, [&client, state = std::move(state), questions = std::move(questions),
                                           opts = std::move(opts)]() mutable {
        return client.system_one_opts(std::move(state), std::move(questions), std::move(opts));
    });
}

template <Scheduler S> Task<Result<ListModelsResponse>> models(S &scheduler, const Client &client, ModelsOpts opts = {})
{
    co_return co_await offload(
        scheduler, [&client, opts = std::move(opts)]() mutable { return client.models_opts(std::move(opts)); });
}

struct InlineExecutor
{
    void operator()(std::function<void()> work) const
    {
        work();
    }
};

class ThreadPoolExecutor
{
  public:
    explicit ThreadPoolExecutor(unsigned threads = std::thread::hardware_concurrency())
    {
        if (threads == 0)
        {
            threads = 1;
        }
        for (unsigned i = 0; i < threads; ++i)
        {
            workers_.emplace_back([this] { worker_loop(); });
        }
    }

    ThreadPoolExecutor(const ThreadPoolExecutor &) = delete;
    ThreadPoolExecutor &operator=(const ThreadPoolExecutor &) = delete;

    ~ThreadPoolExecutor()
    {
        {
            std::lock_guard lock(mutex_);
            stopping_ = true;
        }
        cv_.notify_all();
        for (auto &worker : workers_)
        {
            if (worker.joinable())
            {
                worker.join();
            }
        }
    }

    void operator()(std::function<void()> work)
    {
        {
            std::lock_guard lock(mutex_);
            queue_.push(std::move(work));
        }
        cv_.notify_one();
    }

  private:
    void worker_loop()
    {
        while (true)
        {
            std::function<void()> work;
            {
                std::unique_lock lock(mutex_);
                cv_.wait(lock, [this] { return stopping_ || !queue_.empty(); });
                if (stopping_ && queue_.empty())
                {
                    return;
                }
                work = std::move(queue_.front());
                queue_.pop();
            }
            work();
        }
    }

    std::mutex mutex_;
    std::condition_variable cv_;
    std::queue<std::function<void()>> queue_;
    std::vector<std::thread> workers_;
    bool stopping_ = false;
};

} // namespace typesafe::coro
