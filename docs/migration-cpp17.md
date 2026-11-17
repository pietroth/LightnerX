# C++17 migration and validation

The migration starts from `541ae1d39225ec9b747b9f888f96242a710e9073`, with GameNetworkingSockets pinned at `d534c19aa760df3fb75fd20db13ba1932b8a5463`. The original repository's 38 text files, including 14 module units and six Markdown documents, were reviewed before implementation. The dependency's public interfaces and build requirements were checked without changing its source.

## Architecture and migration sequence

The `communication` layer contains packed INER headers, notification registration, serialization and parsing. The `network` layer owns GNS initialization, client/server lifecycle, connection IDs, routing and a shared outbound queue. Applications connect the layers through synchronous callbacks and supply their own update loop.

Implementation proceeded through independent commits: capture original behavior, add and test compact span, add and test compact expected, replace modules with headers/sources, switch the library and consumers to C++17, extend regressions and CI, then update and compile the documentation.

All 14 module units now have declarations in `include/lightnerx/{communication,network}/*.hpp` and definitions in the corresponding `.cpp` files. Templates remain in headers. Network ownership and the outbound queue each retain one state shared across translation units. Public constants use inline variables so their addresses also remain shared. Public headers compile independently and tolerate repeated inclusion.

Consumer source changes are limited to including the corresponding headers, using `lightnerx::compact::span` and `lightnerx::compact::expected` with their associated helpers, and selecting the new CMake configuration. The custom types implement the C++20 `std::span` and C++23 `std::expected` interfaces within the [documented C++17 limits](compact.md). Application names, callback shapes, ownership, error codes and protocol behavior remain the same. Consumers must be rebuilt; binary ABI compatibility with module builds or native standard-library types is not promised.

CMake 4.5.0 and the public `cxx_std_17` requirement replace the experimental module build. Standard includes, iterator algorithms and explicit function templates replace newer language/library facilities. [Setup](setup.md) contains current commands.

## Preserved behavior

- Packed header sizes remain Event 4, Notification 4, Intention 6, Response 5 and UnsuccessfulResponse 7 bytes. Native-endian fields and authority size/type encoding remain unchanged.
- Parsing still requires one complete frame, rejects truncation and unknown statuses, and preserves the existing extra-byte handling for approved, failure, denied and partially approved responses.
- Group and broadcast sends attempt every selected destination and return the first error. Queue flush attempts all entries, releases messages and returns the last error. Queuing consumes ownership even when rejected.
- Client and Server remain noncopyable and nonmovable, with one active network owner per process. Callback registration, exceptions, recursive-update rejection and deferred server close retain their behavior.
- Connection IDs remain 1–65535 with independent mappings, recycling, strong copy assignment and bounded mutation during iteration.

## Recorded baseline and differential tests

The original C++26 revision was built separately with GCC/libstdc++ 15.2.0, CMake 4.2.3, Ninja 1.13.2, Protobuf 3.21.12 and OpenSSL 3.6.5. Its nine original tests passed without skips. CMake 4.2.3 is required only to reproduce that revision's experimental standard-library module configuration.

[Fixture provenance](../tests/fixtures/reference.json) records the platform, revision and toolchain. [The captured fixture](../tests/fixtures/cpp26-behavior.txt) includes error codes, serialized bytes and a deterministic parsing corpus with seed `0x4c58`: 13,275 invocations, 1,897 delivered messages and digest `14252850176607638377`. The C++17 executable must reproduce the entire output byte for byte. Runtime handles, addresses, ephemeral ports, timings and localized error strings are excluded. The fixture was recorded on Linux x86_64 with little-endian fields; it is not a claim of cross-endian wire compatibility.

The compact tests run the same sources against native types in separate C++20/23 executables and compare outputs. Compile-time checks and invalid-program tests cover construction constraints and API contracts. Real UDP tests also pair the original server with the new client and the original client with the new server.

## Local results

Validation ran on Linux with CMake 4.5.0, GCC 14.2, Clang 19.1 and, for native comparisons, GCC/libstdc++ 15.2. The builds use C++17 except for the explicit native oracle targets and the separately built original revision.

| Configuration | Result |
| --- | --- |
| GCC 14 Debug | 15/15 passed |
| Clang 19 Debug | 15/15 passed |
| GCC 14 Release | 15/15 passed |
| Clang 19 Release | 15/15 passed |
| GCC 14 ASan, UBSan and leak detection | 15/15 passed |
| GCC 15 native comparisons and both cross-version UDP directions | 22/22 passed |

All 56 C++ documentation blocks are compiled with C++17: complete examples retain their own includes, fragments receive explicit application contexts, and API excerpts are checked against the real declarations. The two compact examples also execute; the open-ended network example loops are compiled and linked only. Shell snippets pass syntax checking, and local document links are checked.

These results include no skipped tests. Build directories retain `results.xml`, `tests.log` and CTest's `Testing/Temporary/LastTest.log`; generated logs are not committed. The [CI workflow](../.github/workflows/cpp17.yml) repeats Linux compiler/configuration checks and the separate native/original-reference comparison, and uploads test artifacts.

GameNetworkingSockets is not sanitizer-instrumented and disables RTTI. GNS-linked targets therefore exclude only UBSan's `vptr` check. The independent compact and mock tests retain all selected checks. Windows and macOS were not validated. See [sanitizer instructions](setup.md#sanitizers) and [compact limitations](compact.md#c17-limits-and-verification) when interpreting coverage.
