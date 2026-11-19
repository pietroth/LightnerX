# C++17 byte views and results

## Byte views with span

Include `<tcb/span.hpp>` for `tcb::span`. A span refers to contiguous elements owned elsewhere. Copying the view leaves ownership with the original buffer. Receive callback views expire after dispatch. Copy the bytes during the callback if you need to retain them.

Construct the view from a contiguous container, an array, or a pointer and count. For a slice previously built from container iterators, pass a pointer and the element count. Use `std::size_t{0}` for a zero count to disambiguate the pointer-and-count constructor from the pointer-pair constructor. The backport has no `.at()` member, so check a dynamic index against `size()` before accessing it. The CMake target propagates one contract policy across translation units. A contract violation terminates the program in both Debug and Release. Use `first`, `last` and `subspan` to select a slice, and `tcb::as_bytes` to view its object representation.

```cpp
#include <array>
#include <cassert>
#include <cstddef>
#include <tcb/span.hpp>

int main() {
    std::array<int, 3> values{10, 20, 30};
    tcb::span<int, 3> all{values};
    const tcb::span<const int> empty{values.data(), std::size_t{0}};
    assert(empty.empty());
    auto tail = all.subspan<1>();
    static_assert(decltype(tail)::extent == 2);
    tail[0] = 25;
    assert(values[1] == 25);
    const auto bytes = tcb::as_bytes(all);
    assert(bytes.size() == values.size() * sizeof(int));
}
```

For the complete interface and bounds-checking options, see [tcbrindle/span](https://github.com/tcbrindle/span).

## Results with expected

Include `<zeus/expected.hpp>` for `zeus::expected<T, E>` and `zeus::unexpected<E>`. LightnerX uses `zeus::expected<void, std::error_code>` for operations that return no value on success. Use `if (!result)` before reading `result.error()`. Calling `value()` on an error throws `zeus::bad_expected_access<E>` when exceptions are enabled.

```cpp
#include <cassert>
#include <limits>
#include <string>
#include <zeus/expected.hpp>

zeus::expected<int, std::string> divide(int numerator, int denominator) {
    if (denominator == 0) return zeus::unexpected(std::string("zero denominator"));
    if (numerator == std::numeric_limits<int>::min() && denominator == -1)
        return zeus::unexpected(std::string("quotient out of range"));
    return numerator / denominator;
}

int main() {
    auto result = divide(12, 3).transform([](int value) { return value * 2; });
    assert(result && result.value() == 8);
    auto recovered = divide(1, 0).or_else([](const std::string&) {
        return zeus::expected<int, std::string>(0);
    });
    assert(recovered == 0);
}
```

For the full interface and supported compiler versions, see [zeus-cpp/expected](https://github.com/zeus-cpp/expected/tree/v1.4.0).
