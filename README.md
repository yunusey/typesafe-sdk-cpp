# TypeSafe C++ SDK

> [!CAUTION]
> This is an unofficial plugin that has been mostly written with the help of AI.
> I am planning to fix things up as I experiment with it.
>
> Huge thanks to [@codeitlikemiley](https://github.com/codeitlikemiley) for the Rust
> client which I based this SDK heavily on.

Unofficial C++23 client for the [TypeSafe AI](https://typesafe.ai) API. It is a
hand port of the [unofficial Rust SDK](https://github.com/codeitlikemiley/typesafe-sdk-rust/tree/main)
and targets the same wire contract (`POST /v1/systemone`, `GET /v1/models`). Callers send named
questions about some state and read back typed answers.

**Heavy emphasis on modern C++!**


## For AI agents

Read [`AGENTS.md`](AGENTS.md) before writing SDK or integration code. Copy from
[`examples/system_one.cpp`](examples/system_one.cpp).

## Requirements

- C++23 compiler (**clang**, provided by `flake.nix`)
- CMake >= 3.24 and Ninja
- [libcurl](https://curl.se/libcurl/) for HTTP
- [`nlohmann/json`](https://github.com/nlohmann/json) for JSON
- [GoogleTest](https://github.com/google/googletest) for the test suite

All of these are pinned in [`flake.nix`](flake.nix).

## Quick start (Nix)

```bash
nix develop                      # drops you into the dev shell (clang + deps)
cmake -G Ninja -B build -DTYPESAFE_BUILD_TESTS=ON -DTYPESAFE_BUILD_EXAMPLES=ON
cmake --build build
ctest --test-dir build --output-on-failure
```

Format C++ sources with clang-format (style is [`.clang-format`](.clang-format)):

```bash
./scripts/format.sh          # rewrite in place
./scripts/format.sh --check  # fail if any file would change
```

Or build the whole thing (with tests) as a Nix package:

```bash
nix build
```

## Call System One

Set `TYPESAFE_API_KEY` in your environment, then ask named questions about a
piece of state. The client is synchronous — every call blocks.

```cpp
#include <iostream>
#include <typesafe/typesafe.hpp>

int main()
{
    auto client = typesafe::Client::from_env();
    if (!client)
    {
        std::cerr << client.error().to_string() << '\n';
        return 1;
    }

    typesafe::Json state = {{"document", "I was charged twice. Please fix this ASAP."}};
    auto response = client->system_one(
        state,
        {
            {"billing", typesafe::Question::noul("Is this ticket about billing?")},
            {"tone", typesafe::Question::choice(
                         "What is the customer's tone?",
                         {{"calm", std::nullopt}, {"frustrated", std::nullopt}, {"angry", std::nullopt}})},
            {"urgency", typesafe::Question::score("How urgent is this ticket?", {"can wait", "this week", "today"})},
        });
    if (!response)
    {
        std::cerr << response.error().to_string() << '\n';
        return 1;
    }

    std::cout << "billing=" << response->noul("billing")->noul << '\n';
    std::cout << "tone=" << response->choice("tone")->choice << '\n';
    std::cout << "urgency=" << response->score("urgency")->score << '\n';
}
```

`state` can be a string, JSON object, or JSON array. Questions are noul (yes/no
probability), choice (one label), or score (an ordered rubric). Answers use the
same names. List models with `client->models()`.

## Async with coroutines (bring your own runtime)

The core client is synchronous, but the opt-in header `<typesafe/coro.hpp>`
exposes coroutine-returning operations. Following the Rust SDK's philosophy, the
library does **not** own or start a runtime — you choose it by passing a
`Scheduler` (anything invocable with a `std::function<void()>`). A bundled
`ThreadPoolExecutor` and `InlineExecutor` are provided for convenience; plug in
Asio/stdexec/your own just as easily.

```cpp
#include <typesafe/coro.hpp>
namespace coro = typesafe::coro;

coro::ThreadPoolExecutor scheduler{4}; // or: [&](auto fn){ asio::post(ctx, fn); }

auto task =
    coro::system_one(scheduler, *client, "ticket text", {{"billing", typesafe::Question::noul("About billing?")}});
auto response = coro::sync_wait(std::move(task)); // or co_await from your own coroutine
```

Multiple `coro::system_one(...)` calls started before you `co_await` them run
concurrently on the scheduler. See [`examples/async_fan_out.cpp`](examples/async_fan_out.cpp).
The transport is still blocking libcurl, so each offloaded call occupies a
scheduler slot until it returns; a future fully non-blocking client would drive
libcurl's `curl_multi` from the caller's event loop.

## Design at a glance

- **No exceptions across the public API.** Fallible calls return
  `std::expected<T, typesafe::Error>` (aliased `typesafe::Result<T>`).
- **Typestate builder.** `typesafe::Client::builder().build()` does not compile;
  `build()` exists only after `api_key(...)`.
- **`nlohmann::ordered_json`** keeps request object key order deterministic.
- **libcurl** transport; **`std::variant`** sum types for `Error` and `Answer`.

## Configure the client

Constructor values win over environment variables. Whitespace-only environment
values are ignored.

| Setting | Environment | Default |
| --- | --- | --- |
| API key | `TYPESAFE_API_KEY` | required |
| Base URL | `TYPESAFE_BASE_URL` | `https://api.typesafe.ai` |
| Model | `TYPESAFE_DEFAULT_MODEL` | `jev-latest` |
| Timeout | — | 10 seconds per attempt |
| Log level | `TYPESAFE_LOG_LEVEL` | unset (`debug`/`info`/`warn`/`error`/`off`) |

```cpp
auto client = typesafe::Client::builder()
                  .api_key("sk-...")
                  .model("jev-latest")
                  .timeout(std::chrono::seconds(20))
                  .retry(typesafe::RetryPolicy::disabled())
                  .build();
```

Per-call overrides live on `SystemOneOpts` (`model`, `timeout`, `retry`,
`extra_headers`, `extra_body`) and `ModelsOpts`. `extra_body` is a shallow
last-write-wins merge over `state`, `model`, and `questions`.

## Errors and retries

`typesafe::Error` is a sum type. HTTP failures carry an `ApiError` with a `kind`
such as `BadRequest`, `Authentication`, or `RateLimited`. A 200 body that does
not match the schema is `ApiErrorKind::ResponseValidation` and names the field
path.

Default retries: 2 after the first attempt, on 408/429/5xx plus connection and
timeout errors. Exponential backoff 0.5s→5s with 0.25 jitter and a 30s budget.
`retry-after-ms` and `retry-after` are honored.

## Cookbook examples

See [`examples/README.md`](examples/README.md). Each example runs against the
in-process mock server by default, or the live API when `TYPESAFE_LIVE=1`.

## Tests

```bash
ctest --test-dir build --output-on-failure
```

Pull requests run the same checks in GitHub Actions (`.github/workflows/ci.yml`):
`./scripts/format.sh --check`, then configure, build, and `ctest`.

`tests/contract_test.cpp` mirrors the Rust `tests/contract.rs`;
`tests/examples_test.cpp` mirrors `tests/examples_integration.rs`;
`tests/unit_test.cpp` covers the backoff table, retry status set, error message
extraction, endpoint formatting, question serialization, and header redaction.
The suite uses **GTest** and an in-process HTTP mock server
(`testing/mock_server.hpp`) in place of the Rust suite's wiremock.

## Parity notes

Behavior targets the Rust SDK, which in turn matches the Python `typesafe-sdk`
0.6.0 contract and OpenAPI 0.2.0. Look up one answer with `noul("name")`,
`choice("name")`, or `score("name")`; scan mixed types through `answers`. Treat
`noul` as a probability (a `double`), not a boolean. The single `Client` is
synchronous; async is a thin, runtime-agnostic coroutine layer on top
(`<typesafe/coro.hpp>`) rather than a separate client type.
