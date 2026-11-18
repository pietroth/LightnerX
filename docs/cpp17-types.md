# C++17 byte views and results

## Byte views with span

Include `<tcb/span.hpp>` for `tcb::span`. A span refers to contiguous elements owned elsewhere. Copying the view leaves ownership with the original buffer. Receive callback views expire after dispatch. Copy the bytes during the callback if you need to retain them.

Construct the view from a contiguous container, an array, or a pointer and count. Use `first`, `last` and `subspan` to select a slice, and `tcb::as_bytes` to view its object representation.

```cpp
#include <array>
#include <cassert>
#include <cstddef>
#include <tcb/span.hpp>

int main() {
    std::array<int, 3> values{10, 20, 30};
    tcb::span<int, 3> all{values};
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

Include `<lightnerx/compact/expected.hpp>`. An `expected<T, E>` contains either a value or an error in a tagged union. `expected<void, E>` represents success without a value. The public interface follows C++23 expected, including `unexpected<E>`, `unexpect_t`, `unexpect`, `bad_expected_access<E>`, in-place and converting constructors, copy/move operations, `emplace`, `swap`, comparisons, `value_or`, `error_or`, and the four monadic operations with lvalue/rvalue and const overloads.

```cpp
#include <cassert>
#include <limits>
#include <string>
#include <lightnerx/compact/expected.hpp>

namespace compact = lightnerx::compact;

compact::expected<int, std::string> divide(int numerator, int denominator) {
    if (denominator == 0) return compact::unexpected(std::string("zero denominator"));
    if (numerator == std::numeric_limits<int>::min() && denominator == -1)
        return compact::unexpected(std::string("quotient out of range"));
    return numerator / denominator;
}

int main() {
    auto result = divide(12, 3).transform([](int value) { return value * 2; });
    assert(result && result.value() == 8);
    auto recovered = divide(1, 0).or_else([](const std::string&) {
        return compact::expected<int, std::string>(0);
    });
    assert(recovered == 0);
}
```

`and_then` chains a callable returning expected with the same error type. `or_else` recovers with expected of the same value type. `transform` changes the value type, including to void, and `transform_error` changes the error type. The unselected callable is never invoked. Move-only values/errors and immovable transform results are supported when the operation's construction requirements permit them.

`value()` throws `compact::bad_expected_access<E>` on failure and requires a copy-constructible error type, as in C++23, even when the result contains a value. `operator*` and `operator->` require success; `error()` requires failure. Check `has_value()` or the explicit boolean conversion before accessing an unchecked alternative. Types, explicit conversions, conditional trivial special members and enabled copy/move operations follow the corresponding construction requirements. State changes preserve a valid alternative if construction throws; assignment to an already active value/error has the contained type's exception guarantee.

The representation, iterator implementation types, and exception `what()` strings are not promised to match a particular standard library. Recompile consumers for this migration; the compact types are distinct types and do not preserve the old C++26 binary ABI.
