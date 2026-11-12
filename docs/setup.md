# Setup

LightnerX is a static C++ library. The project provides the `LightnerX` target and does not have its own executable.

## GameNetworkingSockets

GameNetworkingSockets is maintained as a submodule in `external/GameNetworkingSockets`. After obtaining the repository, run the following from the LightnerX root directory:

```bash
git submodule update --init --recursive
```

This command retrieves GameNetworkingSockets and its recursive submodules in the state expected by LightnerX.

## Toolchain and build

LightnerX uses C++26 named modules and `import std`. The verified Linux toolchain
is GCC 15.2 with its matching libstdc++, CMake 4.2.3 and Ninja 1.13.2. Install the
GameNetworkingSockets prerequisites (including OpenSSL and Protobuf development
packages and the Protobuf compiler) before configuring. Compilers and standard
libraries without a C++26 `std` module are not supported by this configuration.

```bash
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Debug
cmake --build build --parallel 2
```

## Consuming with CMake

The `LightnerX` target propagates C++26, GameNetworkingSockets headers, compile
definitions and link requirements. Consumers should link only that target.
CMake's experimental `import std` support must be enabled **before the parent
project's first `project()` call**. This gate is specific to CMake 4.2; consult the
installed CMake documentation before changing CMake versions.

```cmake
cmake_minimum_required(VERSION 4.2)
set(CMAKE_EXPERIMENTAL_CXX_IMPORT_STD "d0edc3af-4c50-42ea-a356-e2862fe7a444")
set(CMAKE_CXX_MODULE_STD ON)
set(CMAKE_CXX_EXTENSIONS OFF)
project(MyGame LANGUAGES C CXX)

add_subdirectory(external/LightnerX)
add_executable(game main.cpp)
target_link_libraries(game PRIVATE LightnerX)
```

```cpp
#include <steam/steamnetworkingsockets.h>
import std;
import lightnerx.network.client;

int main() {
    lightnerx::network::Client client;
    return client.start() ? 0 : 1;
}
```

No prebuilt module binaries are distributed: the consumer and library must use
compatible compiler, standard-library and module settings. The standalone
`tests/consumer` project verifies this integration, including simultaneous imports
of the client and transport modules.

## Regression tests and sanitizers

The regression suite also requires Python 3 for deterministic fault-injection
tests that compile the production implementation with a small transport mock.

```bash
cmake -S . -B build/tests -G Ninja -DCMAKE_BUILD_TYPE=Debug -DLIGHTNERX_BUILD_TESTS=ON
cmake --build build/tests --parallel 2
ctest --test-dir build/tests --output-on-failure
```

With the tested GCC 15.2 toolchain, instrument the modules with AddressSanitizer:

```bash
cmake -S . -B build/sanitizers -G Ninja -DCMAKE_BUILD_TYPE=Debug \
  -DLIGHTNERX_BUILD_TESTS=ON -DLIGHTNERX_SANITIZERS=address
cmake --build build/sanitizers --parallel 2
ctest --test-dir build/sanitizers --output-on-failure
```

This option instruments LightnerX and its consumers, including the tests; it does
not instrument the standard-library module or third-party libraries. On a
compatible compiler, `-DLIGHTNERX_SANITIZERS=address,undefined` enables both ASan
and UBSan. GCC 15.2 hits an internal compiler error when compiling these modules
with UBSan, even if `std` itself is not instrumented. The supplemental Python
fault tests compile the production bodies without module declarations and run
both sanitizers whenever `LIGHTNERX_SANITIZERS` is enabled; they supplement the
real module tests rather than replacing them.

Sanitizer diagnostics fail the test immediately. The lifecycle fault test links
the real GNS library, which is built with `-fno-rtti`; only that supplemental test
disables UBSan's `vptr` check. A raw GNS init/CreatePollGroup/Kill probe reproduces
the incompatible check without using LightnerX. ASan and the other UBSan checks
remain enabled. The allocation and ID mocks do not need this exclusion.

Network integration tests require local UDP socket access. Restricted sandboxes
may prevent those tests from completing; a build alone does not validate network
behavior. Test assertions remain enabled in release configurations.
