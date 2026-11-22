# C++17 migration and validation

The migration starts from `541ae1d39225ec9b747b9f888f96242a710e9073`, with GameNetworkingSockets pinned at `d534c19aa760df3fb75fd20db13ba1932b8a5463`. The original repository's 38 text files, including 14 module units and six Markdown documents, were reviewed before implementation. The dependency's public interfaces and build requirements were checked without changing its source.

## Architecture and migration sequence

The `communication` layer contains packed INER headers, notification registration, serialization and parsing. The `network` layer owns GNS initialization, client/server lifecycle, connection IDs, routing and a shared outbound queue. Applications connect the layers through synchronous callbacks and supply their own update loop.

The initial migration captured original behavior, introduced in-tree backports, replaced modules with headers and sources, then switched the library and consumers to C++17.

All 14 module units now have declarations in `include/lightnerx/{communication,network}/*.hpp` and definitions in the corresponding `.cpp` files. Templates remain in headers. Network ownership and the outbound queue each retain one state shared across translation units. Public constants use inline variables so their addresses also remain shared. Public headers compile independently and tolerate repeated inclusion.

Consumers include the corresponding LightnerX headers and rebuild against the C++17 `LightnerX` target. Use `std::string_view` for binary views, `lightnerx::Result` to inspect success or failure, and `lightnerx::network::ConnectionHandle` for connection identifiers. The [type guide](cpp17-types.md) shows how to wrap a binary payload.

CMake 3.22 or newer and the public `cxx_std_17` requirement replace the experimental module build. Standard includes, iterator algorithms and explicit function templates replace newer language/library facilities. [Setup](setup.md) contains current commands.

## Preserved behavior

- Packed header sizes remain Event 4, Notification 4, Intention 6, Response 5 and UnsuccessfulResponse 7 bytes. Native-endian fields and authority size/type encoding remain unchanged.
- Parsing still requires one complete frame, rejects truncation and unknown statuses, and preserves the existing extra-byte handling for approved, failure, denied and partially approved responses.
- Group and broadcast sends attempt every selected destination and return the first send error. On returned send errors, queue flush attempts every entry and returns the error from the last failed entry. Queuing consumes ownership even when rejected. See the [batcher reference](api.md#batcher) for exception behavior.
- Client and Server remain noncopyable and nonmovable. One wrapper owns networking per LightnerX library instance. Multiple plugins use the same instance when they link to the [shared library](setup.md#consuming-with-cmake).
Connection IDs remain 1–65535 with independent mappings, recycling, and bounded mutation during iteration.

## Recorded baseline and differential tests

The original C++26 revision was built separately with GCC/libstdc++ 15.2.0, CMake 4.2.3, Ninja 1.13.2, Protobuf 3.21.12 and OpenSSL 3.6.5. Its nine original tests passed without skips. CMake 4.2.3 is required only to reproduce that revision's experimental standard-library module configuration.

The fixture records error codes, serialized bytes and a deterministic parsing corpus with seed `0x4c58`, covering 13,275 invocations and 1,897 delivered messages with digest `14252850176607638377`. On little-endian hosts the test compares the full output byte for byte. On big-endian hosts it checks the symbolic error results and parser call count. Runtime handles, addresses, ephemeral ports, timings and localized error strings are excluded. See the [captured fixture](../tests/fixtures/cpp26-behavior.txt) and its [platform, revision and toolchain](../tests/fixtures/reference.json).

The protocol fixture and UDP tests validate LightnerX behavior across the migration. The cross-version UDP tests pair the original server with the new client and the original client with the new server. Upstream projects maintain the tests for their own type implementations.

The minimum was subsequently lowered to CMake 3.22 for compatibility. Header file sets (introduced in CMake 3.23) were replaced with ordinary private header source entries; include paths and C++17 requirements still propagate through the target. The original C++26 reference build retains its separate CMake 4.2.3 requirement.

## Historical results for the in-tree backports

Validation ran on Linux with CMake 4.5.0, GCC 14.2, Clang 19.1 and, for native comparisons, GCC/libstdc++ 15.2. The builds use C++17 except for the explicit native oracle targets and the separately built original revision.

| Configuration | Result |
| --- | --- |
| GCC 14 Debug | 15/15 passed |
| Clang 19 Debug | 15/15 passed |
| GCC 14 Release | 15/15 passed |
| Clang 19 Release | 15/15 passed |
| GCC 14 ASan, UBSan and leak detection | 15/15 passed |
| GCC 15 native comparisons and both cross-version UDP directions | 22/22 passed |

The documentation tests compile the C++ snippets and check API excerpts against the real declarations. The byte view and result examples also run. The open-ended network examples are compiled and linked. The tests check shell snippet syntax and local document links.

These historical runs include no skipped tests. The current CI workflow repeats Linux compiler and configuration checks, runs the cross-version UDP tests and uploads test artifacts. [CI workflow](../.github/workflows/cpp17.yml)

See [sanitizer instructions](setup.md#sanitizers) for the current build.

### Minimum CMake compatibility check

The compatibility adjustment was validated with CMake/CTest **3.22.0** in fresh Linux Debug build directories. GCC 14 passed all 15 standard tests, including the independent consumer build and compiled documentation. GCC/libstdc++ 15 passed all 22 tests with native comparisons and both C++26/C++17 UDP directions enabled. Neither run skipped tests. Logs are in `build/cmake322-gcc/` and `build/cmake322-native/`. The CI compiler/configuration matrix now uses CMake 3.22.0 for the migrated project.
