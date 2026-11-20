# C++17 byte views and results

## Binary buffers with string_view

Include `<string_view>` for `std::string_view`. Construct a binary view with a pointer and an explicit byte count to preserve embedded zero bytes. The view borrows the original buffer. Copy the bytes inside a receive callback if you need them after the callback returns. Message converters copy the bytes into their own buffer.

To use a trivially copyable struct as a payload, wrap its object representation. Both peers need the same layout and byte order. Serialize individual fields when the protocol needs to work across different layouts.

```cpp
#include <cassert>
#include <cstdint>
#include <cstring>
#include <string_view>
#include <type_traits>

struct Payload {
    std::uint32_t id;
};

int main() {
    static_assert(std::is_trivially_copyable_v<Payload>);
    const Payload original{42};
    const std::string_view bytes{
        reinterpret_cast<const char*>(&original), sizeof(original)
    };

    Payload received{};
    if (bytes.size() != sizeof(received)) return 1;
    std::memcpy(&received, bytes.data(), sizeof(received));
    assert(received.id == original.id);
}
```

## Results

Include `<lightnerx/Result.hpp>` for `lightnerx::Result<T, E>` and `lightnerx::ErrorCode`. The default error type is `ErrorCode`. Construct a success from a value, or use `{}` for a successful `Result<void>`. To construct an error, call `Result<T, E>::failure(...)`.

Use `if (result)` or `has_value()` to check the outcome before calling `value()` or `error()`. Accessing the inactive alternative terminates the process. `lightnerx::errorMessage(code)` describes a library error code.

```cpp
#include <cassert>
#include <limits>
#include <lightnerx/Result.hpp>

lightnerx::Result<int> divide(int numerator, int denominator) {
    if (denominator == 0 ||
        (numerator == std::numeric_limits<int>::min() && denominator == -1)) {
        return lightnerx::Result<int>::failure(lightnerx::ErrorCode::InvalidArgument);
    }
    return numerator / denominator;
}

int main() {
    const auto quotient = divide(12, 3);
    assert(quotient.has_value() && quotient.value() == 4);
    const auto invalid = divide(1, 0);
    assert(!invalid && invalid.error() == lightnerx::ErrorCode::InvalidArgument);

    const lightnerx::Result<void> done;
    assert(done);
    done.value();
}
```
