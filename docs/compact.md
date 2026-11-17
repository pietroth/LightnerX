# C++17 compact types

LightnerX provides its own `lightnerx::compact::span` and `lightnerx::compact::expected` in ordinary, self-contained headers. They are implementations, not aliases for `std::span` (C++20) or `std::expected` (C++23). They do not add declarations to `std`, allocate internal storage, or depend on a newer language mode. Contained values and errors may themselves allocate.

## Byte views with span

Include `<lightnerx/compact/span.hpp>`. A span refers to contiguous elements owned elsewhere. Copying it copies the view, not the elements, and does not extend their lifetime. Receive callback views expire when LightnerX releases the received message after dispatch.

The interface follows C++20 span: dynamic and fixed extents, `dynamic_extent`, pointer/iterator and range construction, array construction, deduction guides, compatible const conversions, assignment, element access, forward/reverse iteration, `size`, `size_bytes`, `empty`, `first`, `last`, `subspan`, `as_bytes`, and `as_writable_bytes`. Fixed subviews retain their compile-time extent. Writable byte views reject const elements.

```cpp
#include <array>
#include <cassert>
#include <cstddef>
#include <lightnerx/compact/span.hpp>

int main() {
    namespace compact = lightnerx::compact;
    std::array<int, 3> values{10, 20, 30};
    compact::span<int, 3> all{values};
    auto tail = all.subspan<1>();
    static_assert(decltype(tail)::extent == 2);
    tail[0] = 25;
    assert(values[1] == 25);
    const auto bytes = compact::as_bytes(all);
    assert(bytes.size() == values.size() * sizeof(int));
}
```

As with the C++20 contract, indexing or slicing outside the extent, mismatched fixed extents, dangling ranges and dereferencing an empty span violate preconditions. Debug assertions diagnose several of these cases; there is no bounds-checking exception API. Newer additions such as C++26 `at()` and C++23 constant iterators are outside this C++20 interface.

C++17 cannot express contiguous-iterator and borrowed-range concepts. Raw pointers and the iterator types of default `std::vector`/`std::basic_string` instantiations are recognized; random access alone is insufficient, and deque iterators and packed `vector<bool>` elements are rejected. Range construction uses `std::data`/`std::size`, so contiguous containers, arrays and string views work without C++20 ranges customization. A temporary owning container can supply a const view, but the view must not outlive it.

Custom iterator authors may specialize `lightnerx::compact::enable_contiguous_iterator<Iterator>` to `true` when pointer arithmetic, dereference and `operator->` obey the contiguous iterator contract. Custom non-owning ranges may specialize `enable_borrowed_range<Range>` when their iterators remain valid after the range object is destroyed. Iterator types affected by custom allocators or string traits may need the same opt-in, or a pointer/count view. These opt-ins replace concept detection in C++17; they do not check or extend storage lifetime. Custom ranges relying exclusively on C++20 ranges customization must instead expose C++17 `data()`/`size()` or pass a pointer and count. There are no native span adapters or C++20 ranges specializations.

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

## C++17 limits and verification

Span's scalar construction, observers and subviews are constexpr where C++17 allows them; byte reinterpretation is not a constant expression. Expected supports constexpr scalar in-place/value/error construction, simple observers and trivial copies. C++17 does not permit the placement construction/destruction needed to change union alternatives during constant evaluation, and its `std::invoke` is not constexpr. Consequently, expected's nontrivial/converting construction, state-changing operations and monadic calls are runtime operations in this implementation. Runtime behavior and exception guarantees remain covered by tests.

The same test sources run against compact in C++17 and native span/expected in C++20/23. The suite compares deterministic outputs, reference categories, constraints, extents, slices, byte views, move-only types, cv-void results, conversions, exception rollback, object lifetimes and monadic results. Invalid programs must fail compilation after a positive control succeeds. See [Setup](setup.md#regression-tests) to run the comparisons. This is regression evidence for the tested contracts, not a claim that finite tests prove every possible instantiation equivalent.
