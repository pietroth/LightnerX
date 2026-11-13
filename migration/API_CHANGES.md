# API compatibility ledger

Baseline: `541ae1d39225ec9b747b9f888f96242a710e9073`.
Status: **anticipated decisions only; no API change implemented in Phase 1**.

The objective is to preserve function/class names, namespaces, parameter
passing, defaults, constants, ownership, error codes and behavior. Changes to
public standard-library type spellings are unavoidable because C++17 has no
`std::span` or `std::expected`. INER bytes and message semantics must not change.

## Anticipated compatibility decisions

| ID | Baseline | Planned C++17 form | Compatibility impact / verification |
| --- | --- | --- | --- |
| API-01 | `import lightnerx.communication.X;` / `import lightnerx.network.X;` | `#include <lightnerx/communication/X.hpp>` / `<lightnerx/network/X.hpp>` | Mechanical source migration; names remain in the same C++ namespaces. Explicit standard includes replace `import std`. All 14 modules have corresponding planned headers. |
| API-02 | `std::span<T, Extent>` and `std::as_bytes` | `lightnerx::compat::span<T, Extent>` and companion `as_bytes` | Exact type identity changes. Keep non-owning semantics and all required construction/access/subview behavior. Use one backport for C++17 and newer consumers, never a language-dependent alias to `std::span`. |
| API-03 | `std::expected<void, std::error_code>` and `std::unexpected` | `lightnerx::compat::expected<void, std::error_code>` and compatibility `unexpected` | Exact return/error-wrapper types change; retain success state, bool, `has_value`, `value`, ref-qualified `error`, copy/move/assignment and exact error codes. Do not replace results with bools, optional errors or exception-only APIs. |
| API-04 | `void ConnectionManager::forEach(const auto& callback) const` | `template<class Callback> void forEach(const Callback& callback) const` in the header | Syntax-only lowering of an abbreviated function template; same deduction, const-reference callback, traversal and exception propagation. No `std::function` conversion or new `noexcept`. |
| API-05 | C++26/module flags and parent import-std gate | Public C++17 target requirement and ordinary headers/sources | Consumers continue linking `LightnerX`; no experimental setup. Planned CMake minimum 3.21 requires validation. GNS public requirements and shared link target are preserved. |

API-02 affects `Message::payload`, both interpreter input functions, all five
outbound converters, Client/Server receive callbacks, and all three transport
functions. It also affects explicit callback/test/example parameter types.
The batcher's local view changes internally; `Target` and `OutboundMessage`
do not acquire a span member.

API-03 affects Client `start/connect/update/disconnect`, Server
`start/update/disconnectConnection`, transport
`sendToConnection/sendToConnections/broadcast`, batcher `queueMessage/flushAll`,
and the exported `network::detail::acquireNetworkOwner`. `networkInit` remains
bool; interpretation stays void; manager identity remains `optional<uint16_t>`.

Code using `auto result = ...`, `if (!result)`, `result.has_value()` and
`result.error()` should retain its usage after updating includes. Code explicitly
naming standard span/expected types must update those spellings. Rebuild all
consumers: old module artifacts/object files and new header-based objects are
not promised to be ABI-interchangeable. The new archive and its newer-standard
consumers must nevertheless agree on one stable compatibility-type ABI.

## Provider decision gate (Phase 2)

The proposed candidates are [tcb span](https://github.com/tcbrindle/span) and
[tl expected](https://github.com/TartanLlama/expected), supplied as pinned,
unmodified upstream headers with their licenses. Phase 1 inspected their
documentation but did not add or validate them. No revision is yet locked.

Before public conversion:

1. Record exact revisions, provenance, license files and stable configuration.
2. Verify all view/result behavior listed in `PLAN.md`, section 6. In particular,
   C++17 has class-template deduction but not alias-template deduction: preserve
   deduction guides with using-declarations where valid, or make template
   arguments explicit in migrated code.
3. Check differences from the actual standard return/view types. The expected
   candidate documents an older proposal, including `map`/`map_error` rather than
   assuming every standard monadic operation is identical. Required operations
   must be preserved through a compatible provider/adapter; any unavoidable
   residual difference needs a specific ledger entry before cutover. Do not
   silently claim full drop-in compatibility from successful repository tests.
4. Keep borrowed-buffer validity, invalid-access preconditions and exception
   guarantees explicit. No custom declarations or polyfills in namespace `std`.

## Interfaces and behavior explicitly retained

- **`queueMessage(OutboundMessage)`**, with a `Target` inside the message. The
  repository has no `queueMessage(Target)` overload to preserve or reintroduce.
  Keep `Target::single`, `group`, `broadcast(exceptions = {})`, its public fields,
  exact variant ordering and all three query functions.
- `OutboundMessage` remains a raw-pointer aggregate. No move-only redesign,
  automatic destructor/release, extra payload copy, changed capacity semantics
  or changed consumption-on-error contract.
- Every packed INER field, constructor, response status and binary layout;
  caller-encoded size; native byte order; parser acceptance/rejection including
  ignored approved/failure response payloads and delivered denied payloads.
- `Message` header variant ordering and borrowed payload lifetime; converter
  `bad_alloc`/`length_error`; callback order, mutable state, reentrancy and cleanup.
- Client/Server noncopyability/nonmovability, start/close/disconnect ownership,
  error codes, reference-vs-value callback parameters and noexcept qualifiers.
- ConnectionManager copy/move traits, strong copy assignment, ID domain and
  recycling, local mappings, GNS-handle admission and const lookup methods.
- Direct transport vectors remain by value; direct broadcast still requires its
  exceptions argument. Group/broadcast return the first recipient error;
  `flushAll` returns the last failed entry and attempts all entries after returned
  errors. No new defaults or argument reinterpretation.
- Public `g_bNetworkInitialized` remains an `extern bool` backed by one source
  definition. Preserve exported `network::detail` lifecycle/cleanup functions.
  Keep `invalidConnection`, `k_invalidConnection`, `k_invalidSocket`, and
  `k_invalidPollGroup` with their existing names, types and values.
- Client and transport headers include the runtime header to preserve current
  re-exports and a single `k_invalidConnection`. Event's unused Client import
  can be removed; the baseline probe does not expose `communication::Client`
  through an Event-only import.

Replacing `contains`/ranges algorithms, source extraction in test harnesses,
printing in examples and out-of-line implementation placement are internal/build
changes, not reasons to alter the public behavior. Dependency upgrades, protocol
changes, framework/executable creation and installed-package API additions are
outside this migration.

## Implemented changes

None. Add implemented entries with evidence and migration instructions as later
phases complete; retain the distinction between proposed and verified behavior.
