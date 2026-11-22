# Setup

These instructions apply to the historical `legacy/cpp26` branch. The `LightnerX` target builds a static library by default.

## GameNetworkingSockets

For a fresh checkout, select the historical branch and its pinned submodules together.

```bash
git clone --branch legacy/cpp26 --recurse-submodules https://github.com/pietroth/LightnerX.git
cd LightnerX
```

In an existing checkout of `legacy/cpp26`, initialize the pinned GameNetworkingSockets revision in `external/GameNetworkingSockets`.

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

### Linking applications and plugins

Use the default static library in a single executable. If multiple plugins use
networking in one process, configure with `-DLIGHTNERX_BUILD_SHARED=ON` and link
every plugin to the same shared LightnerX library. This gives all importers one
network owner and one outbound queue. Embedding separate static copies creates
independent runtime state.

LightnerX links the shared GameNetworkingSockets target when available. To use
its static target, configure with `-DBUILD_SHARED_LIB=OFF -DBUILD_STATIC_LIB=ON`.

## Consuming with CMake

The `LightnerX` target propagates C++26 and its link requirements. Applications
using only LightnerX link `LightnerX`. Examples that include Steam headers or
use the GameNetworkingSockets API directly also link
`GameNetworkingSockets::shared` or `GameNetworkingSockets::static`, according to
the configured GNS variant.
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

The regression suite requires Python 3. It builds the complete module examples
from the guides and exercises a separate CMake consumer. The wire-format test
compares serialized bytes and parser output with the saved C++26 fixture.

```bash
cmake -S . -B build/tests -G Ninja -DCMAKE_BUILD_TYPE=Debug -DLIGHTNERX_BUILD_TESTS=ON
cmake --build build/tests --parallel 2
ctest --test-dir build/tests --output-on-failure
```

To check UDP interoperability with another LightnerX revision, build its
`lightnerx_test_lifecycle` executable separately. Configure this branch with
`-DLIGHTNERX_REFERENCE_BINARY=/absolute/path/to/lightnerx_test_lifecycle`.
CTest then runs each version as the client and as the server against the other.

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
