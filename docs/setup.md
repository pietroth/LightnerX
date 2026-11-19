# Setup

LightnerX is a C++17 library built statically by default. CMake provides the `LightnerX` target. The application supplies its own executable.

## Dependencies and toolchain

Use CMake **3.22 or newer**, a C++17 compiler, and a build tool such as Ninja. The CI matrix builds with GCC 14 and Clang 19 on Linux in Debug and Release.

LightnerX uses three pinned Git submodules.

| Dependency | Path |
| --- | --- |
| [GameNetworkingSockets](https://github.com/ValveSoftware/GameNetworkingSockets) | `external/GameNetworkingSockets` |
| [tcbrindle/span](https://github.com/tcbrindle/span/tree/836dc6a0efd9849cb194e88e4aa2387436bb079b) | `external/span` |
| [zeus-cpp/expected v1.4.0](https://github.com/zeus-cpp/expected/tree/v1.4.0) | `external/expected` |

Initialize or update them from the repository root.

```bash
git submodule update --init --recursive
```

Install OpenSSL and Protobuf development packages, the Protobuf compiler, and Python 3 when building tests. For example, on Debian 13:

```bash
sudo apt-get install cmake g++-14 clang-19 ninja-build libssl-dev libprotobuf-dev protobuf-compiler python3-venv pkg-config
cmake --version
```

Use a distribution package providing CMake 3.22 or newer. To reproduce the minimum-version check independently of the system package:

```bash
python3 -m venv .venv
.venv/bin/pip install cmake==3.22.0
export PATH="$PWD/.venv/bin:$PATH"
```

If dependencies live in a separate prefix, pass `-DCMAKE_PREFIX_PATH=/path/to/prefix` when configuring.

## Build

```bash
cmake -S . -B build/debug -G Ninja -DCMAKE_BUILD_TYPE=Debug
cmake --build build/debug --parallel 2
```

To select a compiler, pass `-DCMAKE_C_COMPILER=gcc-14 -DCMAKE_CXX_COMPILER=g++-14`, or `-DCMAKE_C_COMPILER=clang-19 -DCMAKE_CXX_COMPILER=clang++-19`, in a fresh build directory. Use `-DCMAKE_BUILD_TYPE=Release` for a release build.

LightnerX uses the shared GNS target when available. Configure with `-DBUILD_SHARED_LIB=OFF -DBUILD_STATIC_LIB=ON` to use static GNS. These options select the GNS variant independently of LightnerX.

## Consuming with CMake

The target propagates C++17, LightnerX and dependency headers, compile definitions and link requirements. Consumers link `LightnerX`:

```cmake
cmake_minimum_required(VERSION 3.22)
set(CMAKE_CXX_EXTENSIONS OFF)
project(MyGame LANGUAGES C CXX)

add_subdirectory(external/LightnerX)
add_executable(game main.cpp)
target_link_libraries(game PRIVATE LightnerX)
```

```cpp
#include <lightnerx/network/client.hpp>

int main() {
    lightnerx::network::Client client;
    return client.start() ? 0 : 1;
}
```

Use ordinary headers and sources with compatible compiler and standard-library ABIs. Each former module has a corresponding header, for example `<lightnerx/network/server.hpp>`, with definitions in `network/server.cpp`. The independent `tests/consumer` project checks propagated requirements and shared network/queue state across translation units. All public headers also compile in isolation.

Byte views use `tcb::span` from `<tcb/span.hpp>` and results use `zeus::expected` from `<zeus/expected.hpp>`. Both dependencies are header-only and available to consumers through the `LightnerX` target. See [C++17 byte views and results](cpp17-types.md) for examples.

For a host application that loads multiple plugins, configure with `-DLIGHTNERX_BUILD_SHARED=ON` and link each plugin to the same `LightnerX` target from the host build. Ship one shared library instance with the application. Static linking is supported for consumers that link LightnerX once into the process. Separate static copies embedded in plugins have separate state and can interfere with the same GNS runtime.

## Regression tests

```bash
cmake -S . -B build/tests -G Ninja -DCMAKE_BUILD_TYPE=Debug -DLIGHTNERX_BUILD_TESTS=ON
cmake --build build/tests --parallel 2
ctest --test-dir build/tests --output-on-failure
```

Tests cover deterministic protocol characterization, routing and allocation failures, the complete client ID domain, callbacks, ownership and multi-file consumers. They also compile the documentation's C++ examples. Assertions remain enabled in Release. Network integration requires local UDP socket access.

For cross-version UDP tests, also pass `-DLIGHTNERX_REFERENCE_BINARY=/absolute/path/to/original/lightnerx_test_lifecycle`. CTest then pairs the old server with the new client and the new server with the old client. [Migration validation](migration-cpp17.md) identifies the original revision and build requirements. The [CI workflow](../.github/workflows/cpp17.yml) builds it separately.

## Sanitizers

```bash
cmake -S . -B build/sanitizers -G Ninja -DCMAKE_BUILD_TYPE=Debug \
  -DLIGHTNERX_BUILD_TESTS=ON -DLIGHTNERX_SANITIZERS=address,undefined
cmake --build build/sanitizers --parallel 2
ASAN_OPTIONS=detect_leaks=1:halt_on_error=1 UBSAN_OPTIONS=halt_on_error=1 \
  ctest --test-dir build/sanitizers --output-on-failure
```

This instruments LightnerX and its consumers, including tests and any dependency templates instantiated in their code. The GameNetworkingSockets build disables RTTI, so GNS-linked targets disable UBSan's `vptr` check. The standalone mock tests enable both ASan and UBSan when sanitizers are requested. The lifecycle fault test uses the selected sanitizer list and links to the configured GNS target. Enabled sanitizer diagnostics fail tests immediately. LeakSanitizer may require execution outside a ptrace-restricted sandbox.
