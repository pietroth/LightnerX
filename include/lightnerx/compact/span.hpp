#ifndef LIGHTNERX_COMPACT_SPAN_HPP
#define LIGHTNERX_COMPACT_SPAN_HPP

#include <array>
#include <cassert>
#include <cstddef>
#include <iterator>
#include <memory>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>
#include <vector>

namespace lightnerx::compact {

inline constexpr std::size_t dynamic_extent = static_cast<std::size_t>(-1);
template<class T, std::size_t Extent = dynamic_extent> class span;

// C++17 has no contiguous_iterator or borrowed_range concepts. Custom iterator
// and range authors can opt in to these contracts; random access alone is not
// enough (deque is the important counterexample).
template<class Iterator> inline constexpr bool enable_contiguous_iterator = false;
template<class Range> inline constexpr bool enable_borrowed_range = false;
template<class T, std::size_t N> inline constexpr bool enable_borrowed_range<span<T, N>> = true;
template<class C, class Traits>
inline constexpr bool enable_borrowed_range<std::basic_string_view<C, Traits>> = true;

namespace span_detail {
template<class T> using remove_cvref_t = std::remove_cv_t<std::remove_reference_t<T>>;
template<class From, class To>
using compatible = std::is_convertible<From (*)[], To (*)[]>;
template<class> struct is_span : std::false_type {};
template<class T, std::size_t N> struct is_span<span<T, N>> : std::true_type {};
template<class> struct is_array : std::false_type {};
template<class T, std::size_t N> struct is_array<std::array<T, N>> : std::true_type {};

template<std::size_t N> struct extent_storage {
    constexpr explicit extent_storage(std::size_t n) noexcept { assert(n == N); (void)n; }
    constexpr std::size_t count() const noexcept { return N; }
};
template<> struct extent_storage<dynamic_extent> {
    std::size_t size;
    constexpr explicit extent_storage(std::size_t n) noexcept : size(n) {}
    constexpr std::size_t count() const noexcept { return size; }
};

template<class I, class V, bool = std::is_same_v<V, char> || std::is_same_v<V, wchar_t> ||
                                std::is_same_v<V, char16_t> || std::is_same_v<V, char32_t>>
struct string_iterator : std::false_type {};
template<class I, class V> struct string_iterator<I, V, true>
    : std::bool_constant<std::is_same_v<I, typename std::basic_string<V>::iterator> ||
                         std::is_same_v<I, typename std::basic_string<V>::const_iterator>> {};
template<class I, class = void> struct contiguous : std::false_type {};
template<class T> struct contiguous<T*, void> : std::bool_constant<std::is_object_v<T>> {};
template<class I> struct contiguous<I, std::void_t<typename std::iterator_traits<I>::value_type,
                                                 decltype(*std::declval<I&>()),
                                                 std::enable_if_t<!std::is_pointer_v<I>>>> {
    using V = typename std::iterator_traits<I>::value_type;
    static constexpr bool value = std::is_pointer_v<I> || enable_contiguous_iterator<I> ||
        std::is_same_v<I, typename std::vector<V>::iterator> ||
        std::is_same_v<I, typename std::vector<V>::const_iterator> || string_iterator<I, V>::value;
};
template<class T> constexpr T* address(T* pointer) noexcept { return pointer; }
template<class I> constexpr auto address(const I& iterator) noexcept {
    return span_detail::address(iterator.operator->());
}
template<class I, class T, class = void> struct iterator_ok : std::false_type {};
template<class I, class T> struct iterator_ok<I, T, std::void_t<decltype(*std::declval<I&>())>>
    : std::bool_constant<contiguous<I>::value && std::is_lvalue_reference_v<decltype(*std::declval<I&>())> &&
        compatible<std::remove_reference_t<decltype(*std::declval<I&>())>, T>::value> {};
template<class I, class S, class = void> struct sentinel_ok : std::false_type {};
template<class I, class S> struct sentinel_ok<I, S, std::void_t<decltype(std::declval<S>() - std::declval<I>())>>
    : std::bool_constant<!std::is_convertible_v<S, std::size_t>> {};

template<class R, class T, class = void> struct range_ok : std::false_type {};
template<class R, class T> struct range_ok<R, T,
    std::void_t<decltype(std::data(std::declval<R&>())), decltype(std::size(std::declval<R&>()))>>
    : std::bool_constant<
        !is_span<remove_cvref_t<R>>::value && !is_array<remove_cvref_t<R>>::value &&
        !std::is_array_v<remove_cvref_t<R>> &&
        std::is_pointer_v<decltype(std::data(std::declval<R&>()))> &&
        compatible<std::remove_pointer_t<decltype(std::data(std::declval<R&>()))>, T>::value &&
        (std::is_lvalue_reference_v<R> || std::is_const_v<T> || enable_borrowed_range<remove_cvref_t<R>>)> {};
}

template<class T, std::size_t Extent>
class span : private span_detail::extent_storage<Extent> {
    static_assert(std::is_object_v<T>, "span requires an object element type");
    using storage = span_detail::extent_storage<Extent>;
    T* pointer_;
    constexpr T* offset(std::size_t n) const noexcept { return n == 0 ? pointer_ : pointer_ + n; }

public:
    using element_type = T;
    using value_type = std::remove_cv_t<T>;
    using size_type = std::size_t;
    using difference_type = std::ptrdiff_t;
    using pointer = T*;
    using const_pointer = const T*;
    using reference = T&;
    using const_reference = const T&;
    using iterator = T*;
    using reverse_iterator = std::reverse_iterator<iterator>;
    static constexpr std::size_t extent = Extent;

    template<std::size_t N = Extent, std::enable_if_t<N == 0 || N == dynamic_extent, int> = 0>
    constexpr span() noexcept : storage(0), pointer_(nullptr) {}
    constexpr span(const span&) noexcept = default;
    constexpr span& operator=(const span&) noexcept = default;

    template<class I, std::size_t N = Extent,
             std::enable_if_t<span_detail::iterator_ok<I, T>::value && N == dynamic_extent, int> = 0>
    constexpr span(I first, size_type count) noexcept : storage(count), pointer_(span_detail::address(first)) {}
    template<class I, std::size_t N = Extent,
             std::enable_if_t<span_detail::iterator_ok<I, T>::value && N != dynamic_extent, int> = 0>
    constexpr explicit span(I first, size_type count) noexcept : storage(count), pointer_(span_detail::address(first)) {}

    template<class I, class S, std::size_t N = Extent,
             std::enable_if_t<span_detail::iterator_ok<I, T>::value && span_detail::sentinel_ok<I, S>::value &&
                              N == dynamic_extent, int> = 0>
    constexpr span(I first, S last) noexcept(noexcept(last - first))
        : span(first, first == last ? 0 : static_cast<size_type>(last - first)) {}
    template<class I, class S, std::size_t N = Extent,
             std::enable_if_t<span_detail::iterator_ok<I, T>::value && span_detail::sentinel_ok<I, S>::value &&
                              N != dynamic_extent, int> = 0>
    constexpr explicit span(I first, S last) noexcept(noexcept(last - first))
        : span(first, first == last ? 0 : static_cast<size_type>(last - first)) {}

    template<std::size_t N, std::enable_if_t<Extent == dynamic_extent || Extent == N, int> = 0>
    constexpr span(element_type (&array)[N]) noexcept : storage(N), pointer_(array) {}
    template<class U, std::size_t N, std::enable_if_t<(Extent == dynamic_extent || Extent == N) &&
                                                     span_detail::compatible<U, T>::value, int> = 0>
    constexpr span(std::array<U, N>& array) noexcept : storage(N), pointer_(array.data()) {}
    template<class U, std::size_t N, std::enable_if_t<(Extent == dynamic_extent || Extent == N) &&
                                                     span_detail::compatible<const U, T>::value, int> = 0>
    constexpr span(const std::array<U, N>& array) noexcept : storage(N), pointer_(array.data()) {}

    template<class R, std::size_t N = Extent,
             std::enable_if_t<span_detail::range_ok<R, T>::value && N == dynamic_extent, int> = 0>
    constexpr span(R&& range) noexcept(noexcept(std::data(range)) && noexcept(std::size(range)))
        : storage(std::size(range)), pointer_(std::data(range)) {}
    template<class R, std::size_t N = Extent,
             std::enable_if_t<span_detail::range_ok<R, T>::value && N != dynamic_extent, int> = 0>
    constexpr explicit span(R&& range) noexcept(noexcept(std::data(range)) && noexcept(std::size(range)))
        : storage(std::size(range)), pointer_(std::data(range)) {}

    template<class U, std::size_t N,
             std::enable_if_t<span_detail::compatible<U, T>::value &&
                 (Extent == dynamic_extent || Extent == N), int> = 0>
    constexpr span(const span<U, N>& other) noexcept : storage(other.size()), pointer_(other.data()) {}
    template<class U, std::size_t N,
             std::enable_if_t<span_detail::compatible<U, T>::value &&
                 Extent != dynamic_extent && N == dynamic_extent, int> = 0>
    constexpr explicit span(const span<U, N>& other) noexcept : storage(other.size()), pointer_(other.data()) {}

    constexpr iterator begin() const noexcept { return pointer_; }
    constexpr iterator end() const noexcept { return offset(size()); }
    constexpr reverse_iterator rbegin() const noexcept { return reverse_iterator(end()); }
    constexpr reverse_iterator rend() const noexcept { return reverse_iterator(begin()); }
    constexpr reference front() const noexcept { assert(!empty()); return *pointer_; }
    constexpr reference back() const noexcept { assert(!empty()); return pointer_[size() - 1]; }
    constexpr reference operator[](size_type index) const noexcept { assert(index < size()); return pointer_[index]; }
    constexpr pointer data() const noexcept { return pointer_; }
    constexpr size_type size() const noexcept { return storage::count(); }
    constexpr size_type size_bytes() const noexcept { return size() * sizeof(T); }
    [[nodiscard]] constexpr bool empty() const noexcept { return size() == 0; }

    template<std::size_t Count> constexpr span<T, Count> first() const noexcept {
        static_assert(Extent == dynamic_extent || Count <= Extent, "first exceeds extent");
        assert(Count <= size());
        return span<T, Count>(pointer_, Count);
    }
    constexpr span<T> first(size_type count) const noexcept {
        assert(count <= size()); return span<T>(pointer_, count);
    }
    template<std::size_t Count> constexpr span<T, Count> last() const noexcept {
        static_assert(Extent == dynamic_extent || Count <= Extent, "last exceeds extent");
        assert(Count <= size());
        return span<T, Count>(offset(size() - Count), Count);
    }
    constexpr span<T> last(size_type count) const noexcept {
        assert(count <= size()); return span<T>(offset(size() - count), count);
    }
    template<std::size_t Offset, std::size_t Count = dynamic_extent>
    constexpr auto subspan() const noexcept
        -> span<T, Count != dynamic_extent ? Count : (Extent != dynamic_extent ? Extent - Offset : dynamic_extent)> {
        static_assert(Extent == dynamic_extent || Offset <= Extent, "subspan offset exceeds extent");
        static_assert(Count == dynamic_extent || Extent == dynamic_extent || Count <= Extent - Offset,
                      "subspan count exceeds extent");
        assert(Offset <= size());
        assert(Count == dynamic_extent || Count <= size() - Offset);
        using result = span<T, Count != dynamic_extent ? Count : (Extent != dynamic_extent ? Extent - Offset : dynamic_extent)>;
        return result(offset(Offset), Count == dynamic_extent ? size() - Offset : Count);
    }
    constexpr span<T> subspan(size_type position, size_type count = dynamic_extent) const noexcept {
        assert(position <= size());
        assert(count == dynamic_extent || count <= size() - position);
        return span<T>(offset(position), count == dynamic_extent ? size() - position : count);
    }
};

template<class I, class S> span(I, S) -> span<std::remove_reference_t<decltype(*std::declval<I&>())>>;
template<class T, std::size_t N> span(T (&)[N]) -> span<T, N>;
template<class T, std::size_t N> span(std::array<T, N>&) -> span<T, N>;
template<class T, std::size_t N> span(const std::array<T, N>&) -> span<const T, N>;
template<class R> span(R&&) -> span<std::remove_pointer_t<decltype(std::data(std::declval<R&>()))>>;

template<class T, std::size_t N>
constexpr auto as_bytes(span<T, N> view) noexcept -> span<const std::byte, N == dynamic_extent ? dynamic_extent : sizeof(T) * N> {
    return span<const std::byte, N == dynamic_extent ? dynamic_extent : sizeof(T) * N>(
        reinterpret_cast<const std::byte*>(view.data()), view.size_bytes());
}
template<class T, std::size_t N, std::enable_if_t<!std::is_const_v<T>, int> = 0>
constexpr auto as_writable_bytes(span<T, N> view) noexcept -> span<std::byte, N == dynamic_extent ? dynamic_extent : sizeof(T) * N> {
    return span<std::byte, N == dynamic_extent ? dynamic_extent : sizeof(T) * N>(
        reinterpret_cast<std::byte*>(view.data()), view.size_bytes());
}
}
#endif
