# Setup

LightnerX is a static C++17 library. CMake provides the `LightnerX` target. The application supplies its own executable.

## Dependencies and toolchain

Use CMake **4.5.0 or newer**, a C++17 compiler, and a build tool such as Ninja. CMake 4.5.0 was the latest stable release selected for this migration; CMake's version and the C++ language standard are independent requirements. Linux was validated with GCC 14.2 and Clang 19.1 in Debug and Release. GCC/libstdc++ 15.2 additionally validates the native comparison tests. Other platforms have not been validated by this migration.

GameNetworkingSockets is pinned as a submodule in `external/GameNetworkingSockets`. Initialize it from the repository root:

```bash
git submodule update --init --recursive
```

Install OpenSSL and Protobuf development packages, the Protobuf compiler, and Python 3 when building tests. For example, on Debian 13:

```bash
sudo apt-get install curl ca-certificates g++-14 clang-19 ninja-build libssl-dev libprotobuf-dev protobuf-compiler python3-venv pkg-config
curl -fL https://github.com/Kitware/CMake/releases/download/v4.5.0/cmake-4.5.0-linux-x86_64.tar.gz -o /tmp/lightnerx-cmake.tar.gz
echo '81f0d5b0960fb6adc8197030ffc827fba937640d51e73e39e0e9e40d88c66f6f  /tmp/lightnerx-cmake.tar.gz' | sha256sum -c -
mkdir -p .tools/cmake
tar -xzf /tmp/lightnerx-cmake.tar.gz --strip-components=1 -C .tools/cmake
export PATH="$PWD/.tools/cmake/bin:$PATH"
```

The download above is the official Linux x86_64 distribution, with its release checksum. For other architectures, choose the matching asset from the [CMake release](https://github.com/Kitware/CMake/releases/tag/v4.5.0). CMake 4.5.0 is not yet available through the Python package index.

If dependencies live in a separate prefix, pass `-DCMAKE_PREFIX_PATH=/path/to/prefix` when configuring. The local validation used Protobuf 3.21.12, OpenSSL 3.6.5 and Ninja 1.13.2.

## Build

```bash
cmake -S . -B build/debug -G Ninja -DCMAKE_BUILD_TYPE=Debug
cmake --build build/debug --parallel 2
```

To select a compiler, pass `-DCMAKE_C_COMPILER=gcc-14 -DCMAKE_CXX_COMPILER=g++-14`, or `-DCMAKE_C_COMPILER=clang-19 -DCMAKE_CXX_COMPILER=clang++-19`, in a fresh build directory. Use `-DCMAKE_BUILD_TYPE=Release` for a release build.

## Consuming with CMake

The target propagates C++17, LightnerX headers, GameNetworkingSockets headers, compile definitions and link requirements. Consumers link `LightnerX`:

```cmake
cmake_minimum_required(VERSION 4.5.0)
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

Byte views use `lightnerx::compact::span`, and results use `lightnerx::compact::expected`. These are LightnerX implementations of the C++20 `std::span` and C++23 `std::expected` contracts for C++17; see [compact types](compact.md) for supported operations and language limitations.

## Regression tests

```bash
cmake -S . -B build/tests -G Ninja -DCMAKE_BUILD_TYPE=Debug -DLIGHTNERX_BUILD_TESTS=ON
cmake --build build/tests --parallel 2
ctest --test-dir build/tests --output-on-failure
```

Tests include deterministic protocol characterization, routing and allocation failures, the complete client ID domain, callbacks, ownership, multi-file consumers, and the compact types. They also compile the documentation's C++ examples. Assertions remain enabled in Release. Network integration requires local UDP socket access; a build alone does not validate network behavior.

To compare the compact types against the native standard types, use GCC/libstdc++ 15.2 and enable the separate reference executables:

```bash
cmake -S . -B build/native -G Ninja -DCMAKE_BUILD_TYPE=Debug \
  -DLIGHTNERX_BUILD_TESTS=ON -DLIGHTNERX_BUILD_NATIVE_REFERENCE_TESTS=ON
cmake --build build/native --parallel 2
ctest --test-dir build/native --output-on-failure
```

Only these reference executables require C++20/23. LightnerX and the compact tests remain in C++17. A missing or incompatible native implementation fails the optional reference build; tests are not silently skipped. GCC 14's native expected implementation has an older conversion-to-boolean defect, so it is not the oracle for this comparison.

For cross-version UDP tests, also pass `-DLIGHTNERX_REFERENCE_BINARY=/absolute/path/to/original/lightnerx_test_lifecycle`. CTest then pairs the old server with the new client and the new server with the old client. [Migration validation](migration-cpp17.md) identifies the original revision and build requirements. The [CI workflow](../.github/workflows/cpp17.yml) builds it separately.

## Sanitizers

```bash
cmake -S . -B build/sanitizers -G Ninja -DCMAKE_BUILD_TYPE=Debug \
  -DLIGHTNERX_BUILD_TESTS=ON -DLIGHTNERX_SANITIZERS=address,undefined
cmake --build build/sanitizers --parallel 2
ASAN_OPTIONS=detect_leaks=1:halt_on_error=1 UBSAN_OPTIONS=halt_on_error=1 \
  ctest --test-dir build/sanitizers --output-on-failure
```

This instruments LightnerX and its consumers, including tests. Dependencies are not rebuilt with sanitizer instrumentation. The pinned GameNetworkingSockets build disables RTTI, so GNS-linked targets disable only UBSan's `vptr` check. All other selected checks remain enabled, and independent compact and transport/allocation mock tests retain `vptr` checks as well. Fault-injection tests enable both ASan and UBSan when sanitizers are requested. Enabled sanitizer diagnostics fail tests immediately. LeakSanitizer may require execution outside a ptrace-restricted sandbox.
