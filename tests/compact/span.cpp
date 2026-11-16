#ifdef LIGHTNERX_NATIVE_REFERENCE
#include <span>
namespace subject = std;
#else
#include <lightnerx/compact/span.hpp>
namespace subject = lightnerx::compact;
#endif

#include <algorithm>
#include <array>
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <deque>
#include <iostream>
#include <list>
#include <memory>
#include <string>
#include <string_view>
#include <type_traits>
#include <vector>

struct Base { int value; };
struct Derived : Base { int extra; };
struct ThrowingRange {
    int values[2]{};
    int* begin() noexcept { return values; }
    int* end() noexcept { return values + 2; }
    int* data() { throw 73; }
    std::size_t size() const noexcept { return 2; }
};
using subject::span;
static_assert(std::is_default_constructible_v<span<int>>);
static_assert(std::is_default_constructible_v<span<int, 0>>);
static_assert(!std::is_default_constructible_v<span<int, 1>>);
static_assert(std::is_trivially_copyable_v<span<int>>);
static_assert(std::is_trivially_copyable_v<span<int, 3>>);
static_assert(std::is_constructible_v<span<const int>, span<int>>);
static_assert(!std::is_constructible_v<span<int>, span<const int>>);
static_assert(!std::is_constructible_v<span<Base>, Derived*, std::size_t>);
static_assert(!std::is_constructible_v<span<int, 2>, std::array<int, 3>&>);
static_assert(!std::is_constructible_v<span<int, 2>, span<int, 3>>);
static_assert(std::is_constructible_v<span<int, 2>, span<int>>);
static_assert(!std::is_convertible_v<span<int>, span<int, 2>>);
static_assert(std::is_convertible_v<span<int, 2>, span<int>>);
static_assert(std::is_constructible_v<span<int>, std::vector<int>&>);
static_assert(std::is_nothrow_constructible_v<span<int>, std::vector<int>&>);
static_assert(std::is_nothrow_constructible_v<span<int, 3>, std::vector<int>&>);
static_assert(std::is_nothrow_constructible_v<span<const char>, std::string_view>);
static_assert(std::is_constructible_v<span<int>, ThrowingRange&>);
static_assert(!std::is_nothrow_constructible_v<span<int>, ThrowingRange&>);
static_assert(!std::is_constructible_v<span<int>, std::vector<int>>);
static_assert(std::is_constructible_v<span<const int>, std::vector<int>>);
static_assert(!std::is_constructible_v<span<int>, const std::vector<int>&>);
static_assert(!std::is_constructible_v<span<int>, std::list<int>&>);
static_assert(!std::is_constructible_v<span<int>, std::deque<int>&>);
static_assert(!std::is_constructible_v<span<int>, std::deque<int>::iterator, std::size_t>);
static_assert(!std::is_constructible_v<span<bool>, std::vector<bool>&>);
static_assert(!std::is_constructible_v<span<const bool>, std::vector<bool>::const_iterator, std::size_t>);
static_assert(std::is_constructible_v<span<std::unique_ptr<int>>, std::unique_ptr<int>*, std::size_t>);
static_assert(std::is_same_v<decltype(std::declval<const span<int>&>()[0]), int&>);
static_assert(std::is_same_v<decltype(std::declval<span<int, 5>>().subspan<2>()), span<int, 3>>);
static_assert(std::is_same_v<decltype(std::declval<span<int>>().subspan<2, 3>()), span<int, 3>>);
static_assert(std::is_same_v<decltype(std::declval<span<int, 5>>().first(2)), span<int>>);
static_assert(std::is_same_v<decltype(subject::as_bytes(std::declval<span<int, 3>>())),
                             span<const std::byte, sizeof(int) * 3>>);
static_assert(noexcept(std::declval<span<int>>().subspan(0)));

constexpr bool constant_operations() {
    int data[] = {2, 3, 5, 7};
    span view(data);
    static_assert(decltype(view)::extent == 4);
    view[1] = 11;
    auto tail = view.subspan<1>();
    auto middle = tail.first<2>().last<1>();
    span<const int> immutable(view);
    span<int, 0> empty;
    return view.size() == 4 && view.size_bytes() == 4 * sizeof(int) &&
           view.front() == 2 && view.back() == 7 && immutable[1] == 11 &&
           middle[0] == 5 && *view.rbegin() == 7 && view.end() - view.begin() == 4 &&
           empty.empty() && empty.data() == nullptr && empty.begin() == empty.end() &&
           empty.subspan<0>().size() == 0 && empty.last(0).data() == nullptr;
}
static_assert(constant_operations());

int main() {
    ThrowingRange throwing;
    try { span<int> view(throwing); (void)view; assert(false); }
    catch (int error) { assert(error == 73); }
    std::vector<int> values{1, 2, 3, 4};
    span from_range(values);
    span from_iterators(values.begin(), values.end());
    span from_count(values.begin(), values.size());
    span<int, 4> fixed(values);
    assert(from_range.data() == values.data());
    assert(from_iterators.data() == values.data() && from_iterators.size() == 4);
    assert(from_count.size() == 4 && fixed.size() == 4);
    const span<int> writable = values;
    writable[0] = 9;
    assert(values[0] == 9);
    std::array<int, 3> array{3, 4, 5};
    const std::array<int, 3> constant{6, 7, 8};
    span array_view(array);
    span constant_view(constant);
    static_assert(decltype(array_view)::extent == 3);
    static_assert(std::is_same_v<decltype(constant_view), span<const int, 3>>);
    assert(array_view.data() == array.data() && constant_view.back() == 8);
    std::string text = "LightnerX";
    span characters(text.begin(), text.end());
    span letters{std::string_view(text)};
    assert(letters.size() == text.size());
    assert(characters.size() == text.size());
    const auto byte_view = subject::as_bytes(span{array});
    const auto mutable_bytes = subject::as_writable_bytes(span{array});
    assert(byte_view.size() == sizeof(array));
    assert(mutable_bytes.data() == reinterpret_cast<std::byte*>(array.data()));
    auto saved = mutable_bytes[0];
    mutable_bytes[0] = std::byte{0};
    assert(byte_view[0] == std::byte{0});
    mutable_bytes[0] = saved;

    span<int> empty;
    span<int> empty_pair(static_cast<int*>(nullptr), static_cast<int*>(nullptr));
    assert(empty_pair.empty());
    assert(empty.first(0).data() == nullptr && empty.last<0>().data() == nullptr);
    assert(subject::as_bytes(empty).empty());
    assert(subject::as_writable_bytes(empty).empty());
    std::vector<int> no_values;
    span no_range(no_values);
    span no_iterators(no_values.begin(), no_values.end());
    assert(no_range.empty() && no_iterators.empty());

    std::array<std::uint32_t, 64> data{};
    for (std::size_t i = 0; i < data.size(); ++i) data[i] = static_cast<std::uint32_t>(i * i + 7);
    std::uint64_t digest = 0;
    for (std::size_t length = 0; length <= data.size(); ++length) {
        span view(data.data(), length);
        for (std::size_t first = 0; first <= length; ++first) {
            for (std::size_t count = 0; count <= length - first; ++count) {
                auto part = view.subspan(first, count);
                assert(part.data() == data.data() + first);
                assert(part.size_bytes() == count * sizeof(std::uint32_t));
                assert(view.first(first).size() == first);
                assert(view.last(count).data() == data.data() + length - count);
                for (auto value : part) digest = digest * 33 + value;
                for (auto it = part.rbegin(); it != part.rend(); ++it) digest = digest * 33 + *it;
            }
        }
    }
    std::cout << "span " << digest << '\n';
}
