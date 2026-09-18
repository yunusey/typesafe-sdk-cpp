# AGENTS.md — TypeSafe C++ SDK

Guidance for humans and AI agents working in this repository. Read this before
writing SDK code or integration code.

## What this is

An unofficial C++23 client for the [TypeSafe AI](https://typesafe.ai) API. It is
a hand port of the unofficial Rust SDK (`../typesafe-sdk-rust`) and targets the
same wire contract: `POST /v1/systemone` and `GET /v1/models`. Callers send
named questions about some state and read back typed answers.

Two endpoints only. No code generation. Hand-written types plus a contract test
suite that runs against an in-process mock HTTP server.

## Ground rules

- **Language**: C++23. Compiler is **clang** (`llvmPackages_18` via `flake.nix`).
- **Build**: CMake + Ninja.
- **JSON**: `nlohmann/json`. The SDK type alias is `typesafe::Json`
  (`nlohmann::ordered_json`, so object key order is preserved on the wire like
  the Rust SDK's `preserve_order`).
- **HTTP**: libcurl (easy interface). The client is **synchronous/blocking**;
  C++ has no standard async runtime, so unlike Rust there is no separate
  `blocking` module — the one `Client` blocks.
- **Async**: opt-in, runtime-agnostic coroutine layer in `<typesafe/coro.hpp>`
  (not included by the umbrella header). `typesafe::coro::system_one` / `models`
  return a `Task<T>`; the *caller* supplies the runtime via a `Scheduler`
  (any invocable taking `std::function<void()>`). The library never starts a
  runtime — it offloads the blocking call to the caller's scheduler, mirroring
  Rust's "library is async, caller brings the executor" split. Bundled
  `ThreadPoolExecutor` / `InlineExecutor` are conveniences only.
- **Errors**: no exceptions across the public API. Fallible calls return
  `std::expected<T, typesafe::Error>` (aliased `typesafe::Result<T>`). `Error`
  is a sum type (`std::variant`) mirroring the Rust `Error` enum.
- **Dependencies live in `flake.nix`.** Do not add ad-hoc system deps; add them
  to the flake instead.

## Best-practice patterns used

- **`std::expected` everywhere** for fallible operations instead of throwing.
- **Typestate builder**: `Client::builder()` returns a `ClientBuilder<NoApiKey>`.
  Only `ClientBuilder<ApiKeySet>` has `build()`. `Client::builder().build()`
  therefore fails to compile — a missing key is a compile error, exactly like
  the Rust typestate builder.
- **`std::variant` sum types** for `Error`, `Answer`, and the internal question
  representation, matched with `std::visit` / `std::get_if`.
- **RAII** for libcurl handles and the global init, and for the test mock
  server's socket + thread.
- **Value semantics** for request/response types; no raw owning pointers.

## Layout

```
include/typesafe/    public headers (umbrella: typesafe/typesafe.hpp)
src/                 implementation (+ internal-only http/request/logging)
examples/            cookbook programs + support/{fixtures,harness}
tests/               GTest contract + example integration + mock HTTP server
scripts/             clang-format wrapper (`./scripts/format.sh`)
flake.nix            all dependencies + dev shell
CMakeLists.txt       build system (library, examples, tests)
```

## Public API surface

- `typesafe::Client` — `system_one`, `system_one_opts`, `models`, `models_opts`.
- `typesafe::ClientBuilder<State>` — typestate builder.
- `typesafe::Question` — `noul`, `noul_bare`, `choice`, `score`, `raw`,
  `from_value`, `with_noul_criteria`.
- `typesafe::NoulCriteria` — `.yes(...)`, `.no(...)`.
- `typesafe::SystemOneResponse` — `.noul(name)`, `.choice(name)`,
  `.score(name)`, `.answers`, `.model`, `.usage`, `.request_id()`, `.raw_body()`.
- `typesafe::ListModelsResponse` — `.models`, `.request_id()`, `.raw_body()`.
- `typesafe::Error`, `typesafe::ApiError`, `typesafe::ApiErrorKind`,
  `typesafe::ErrorBody`.
- `typesafe::RetryPolicy`, `typesafe::RetryStatuses`.

## Wire contract (must stay in parity with the Rust SDK)

Questions serialize as:

- noul: `{"type":"noul"[, "instructions":...][, "criteria":{...}]}`
- choice: `{"type":"choice", "instructions":..., "criteria":{label: desc|null}}`
- score: `{"type":"score", "instructions":..., "criteria":[...]}` (non-empty)

Answers decode by `type`: `noul` → probability `noul` (a double, **not** a
bool); `choice` → `choice` label + `confidence` + `probabilities`; `score` →
`score` + `confidence` + integer-keyed `legend` and `probabilities`. Unknown
answer types are skipped (kept in `raw_body()`).

A 200 body that fails schema validation becomes `ApiErrorKind::ResponseValidation`
with a `field_path` such as `answers.n.noul`.

## Config resolution

Constructor values win over environment. Whitespace-only env values are ignored.

| Setting | Env | Default |
| --- | --- | --- |
| API key | `TYPESAFE_API_KEY` | required |
| Base URL | `TYPESAFE_BASE_URL` | `https://api.typesafe.ai` |
| Model | `TYPESAFE_DEFAULT_MODEL` | `jev-latest` |
| Timeout | — | 10s per attempt |
| Log level | `TYPESAFE_LOG_LEVEL` | unset (`debug`/`info`/`warn`/`error`/`off`) |

Secret headers (`authorization`, `x-api-key`, `cookie`, anything with `token`
or `secret`) are redacted in debug logs.

## Retries

Default: 2 retries after the first attempt. Retry on 408, 429, 5xx, connection
errors, and timeouts. Exponential backoff 0.5s→5s with 0.25 jitter and a 30s
budget. Honor `retry-after-ms` and `retry-after`. `RetryPolicy::disabled()`
turns retries off. The backoff table matches the Rust/Python SDK exactly and is
covered by a unit test.

## Testing

- `tests/support/mock_server.hpp` is a tiny in-process HTTP/1.1 server (POSIX
  sockets, background thread). It is the C++ analogue of the Rust suite's
  wiremock: register stubs (method + path + optional header match), respond with
  status/body/headers, replay recorded requests, and support "N times then
  fallback" for the retry test.
- `tests/contract_test.cpp` mirrors `typesafe-sdk-rust/tests/contract.rs`.
- `tests/examples_test.cpp` mirrors `tests/examples_integration.rs` using the
  same canned fixtures as the examples.
- Framework is **GTest**.

Run:

```bash
cmake -G Ninja -B build -DTYPESAFE_BUILD_TESTS=ON -DTYPESAFE_BUILD_EXAMPLES=ON
cmake --build build
ctest --test-dir build --output-on-failure
./scripts/format.sh --check
```

GitHub Actions (`.github/workflows/ci.yml`) runs format and tests on every pull request.

## When adding code

1. Keep the two-endpoint surface small. No speculative features.
2. Fallible → `Result<T>`. Never throw across the public API.
3. New behavior needs a mirror of the corresponding Rust test.
4. New deps go in `flake.nix` and `CMakeLists.txt`.
5. Keep object key order deterministic (`Json` is `ordered_json`).
6. Format C++ with `./scripts/format.sh` (style is `.clang-format`). Use
   `--check` to fail if anything would change.
