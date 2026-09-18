# Cookbook examples

Runnable C++ ports of the patterns and cookbooks from
[docs.typesafe.ai](https://docs.typesafe.ai/llms.txt), mirroring the Rust SDK's
`examples/`.

## Mock vs live

| Mode | How to run |
| --- | --- |
| **Mock** (default, no API key) | `./build/examples/NAME` |
| **Live** (your account) | `TYPESAFE_LIVE=1 TYPESAFE_API_KEY=... ./build/examples/NAME` |

Mock mode starts the in-process HTTP mock server (`testing/mock_server.hpp`,
the C++ analogue of wiremock) with canned JSON from
`examples/support/fixtures.hpp`. Live mode calls `https://api.typesafe.ai` with
your credentials.

The example integration test (`tests/examples_test.cpp`) exercises the same
fixtures under `ctest`.

## Examples

| Example | Doc link |
| --- | --- |
| `system_one` | [Quick start](https://docs.typesafe.ai/introduction/quickstart.md) |
| `fan_out` | [Speculative fan-out](https://docs.typesafe.ai/patterns/fan-out.md) |
| `confidence_routing` | [Confidence](https://docs.typesafe.ai/confidence.md) |
| `guardrails` | [LLM guardrails](https://docs.typesafe.ai/cookbooks/llm_guardrails.md) |
| `citation_check` | [Citation check](https://docs.typesafe.ai/cookbooks/citation_check.md) |
| `structured_state` | [State](https://docs.typesafe.ai/concepts/state.md) |
| `composite_scoring` | [Composite scoring](https://docs.typesafe.ai/patterns/composite-scoring.md) |
| `intent_routing` | [Intent routing](https://docs.typesafe.ai/patterns/intent-routing.md) |
| `noul_uncertainty` | [Self-consistency nouls](https://docs.typesafe.ai/cookbooks/consistency_noul_cookbook.md) |
| `blocking_triage` | Synchronous triage (the C++ client always blocks) |
| `async_fan_out` | C++20 coroutines layered on the sync client for concurrent fan-out |

### Note on `async_fan_out`

The core client is synchronous (blocking libcurl), but the library ships an
opt-in coroutine layer in `<typesafe/coro.hpp>`: `coro::system_one` /
`coro::models` return a `Task<T>`, and the *caller* supplies the runtime via a
`Scheduler`. `async_fan_out` starts three calls on a `coro::ThreadPoolExecutor`
and `co_await`s them, so the round-trips overlap. Swap the executor for an
Asio/stdexec/custom scheduler with no other changes. A fully non-blocking client
would instead drive libcurl's `curl_multi` from the caller's event loop.

## Build and run all mocks

```bash
cmake -G Ninja -B build -DTYPESAFE_BUILD_EXAMPLES=ON
cmake --build build
for ex in system_one fan_out confidence_routing guardrails citation_check \
  structured_state composite_scoring intent_routing noul_uncertainty blocking_triage; do
  ./build/examples/"$ex"
done
```
