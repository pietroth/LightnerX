#ifndef LIGHTNERX_COMPACT_EXPECTED_HPP
#define LIGHTNERX_COMPACT_EXPECTED_HPP

#include <cassert>
#include <exception>
#include <functional>
#include <initializer_list>
#include <memory>
#include <new>
#include <type_traits>
#include <utility>

namespace lightnerx::compact {
struct unexpect_t { explicit constexpr unexpect_t() = default; };
inline constexpr unexpect_t unexpect{};
template<class E> class unexpected;
template<class T, class E> class expected;
template<class E> class bad_expected_access;

template<> class bad_expected_access<void> : public std::exception {
protected:
    bad_expected_access() noexcept = default;
    bad_expected_access(const bad_expected_access&) = default;
    bad_expected_access(bad_expected_access&&) = default;
    bad_expected_access& operator=(const bad_expected_access&) = default;
    bad_expected_access& operator=(bad_expected_access&&) = default;
    ~bad_expected_access() = default;
public:
    const char* what() const noexcept override { return "bad access to lightnerx::compact::expected"; }
};
template<class E> class bad_expected_access : public bad_expected_access<void> {
    E error_;
public:
    explicit bad_expected_access(E error) : error_(std::move(error)) {}
    E& error() & noexcept { return error_; }
    const E& error() const & noexcept { return error_; }
    E&& error() && noexcept { return std::move(error_); }
    const E&& error() const && noexcept { return std::move(error_); }
};

namespace expected_detail {
template<class T> using remove_cvref_t = std::remove_cv_t<std::remove_reference_t<T>>;
template<class To> void conversion_sink(To) noexcept;
template<class From, class To, bool = std::is_convertible_v<From, To>>
struct nothrow_convertible : std::false_type {};
template<class From, class To> struct nothrow_convertible<From, To, true>
    : std::bool_constant<noexcept(conversion_sink<To>(std::declval<From>()))> {};
template<class> struct is_unexpected : std::false_type {};
template<class E> struct is_unexpected<unexpected<E>> : std::true_type {};
template<class> struct is_expected : std::false_type {};
template<class T, class E> struct is_expected<expected<T, E>> : std::true_type {};
template<class E> inline constexpr bool error_type_ok = std::is_object_v<E> && !std::is_array_v<E> &&
    !std::is_const_v<E> && !std::is_volatile_v<E> && !is_unexpected<E>::value;
}

template<class E> class unexpected {
    static_assert(expected_detail::error_type_ok<E>, "invalid unexpected error type");
    E error_;
public:
    constexpr unexpected(const unexpected&) = default;
    constexpr unexpected(unexpected&&) = default;
    constexpr unexpected& operator=(const unexpected&) = default;
    constexpr unexpected& operator=(unexpected&&) = default;
    template<class G = E, std::enable_if_t<
        !std::is_same_v<expected_detail::remove_cvref_t<G>, unexpected> &&
        !std::is_same_v<expected_detail::remove_cvref_t<G>, std::in_place_t> &&
        std::is_constructible_v<E, G>, int> = 0>
    constexpr explicit unexpected(G&& error) noexcept(std::is_nothrow_constructible_v<E, G>)
        : error_(std::forward<G>(error)) {}
    template<class... Args, std::enable_if_t<std::is_constructible_v<E, Args...>, int> = 0>
    constexpr explicit unexpected(std::in_place_t, Args&&... args)
        noexcept(std::is_nothrow_constructible_v<E, Args...>) : error_(std::forward<Args>(args)...) {}
    template<class U, class... Args,
             std::enable_if_t<std::is_constructible_v<E, std::initializer_list<U>&, Args...>, int> = 0>
    constexpr explicit unexpected(std::in_place_t, std::initializer_list<U> list, Args&&... args)
        noexcept(std::is_nothrow_constructible_v<E, std::initializer_list<U>&, Args...>)
        : error_(list, std::forward<Args>(args)...) {}
    constexpr E& error() & noexcept { return error_; }
    constexpr const E& error() const & noexcept { return error_; }
    constexpr E&& error() && noexcept { return std::move(error_); }
    constexpr const E&& error() const && noexcept { return std::move(error_); }
    constexpr void swap(unexpected& other) noexcept(std::is_nothrow_swappable_v<E>) {
        static_assert(std::is_swappable_v<E>);
        using std::swap; swap(error_, other.error_);
    }
};
template<class E> unexpected(E) -> unexpected<E>;
template<class E, class G> constexpr bool operator==(const unexpected<E>& a, const unexpected<G>& b) {
    return a.error() == b.error();
}
template<class E, class G> constexpr bool operator!=(const unexpected<E>& a, const unexpected<G>& b) { return !(a == b); }
template<class E, std::enable_if_t<std::is_swappable_v<E>, int> = 0>
constexpr void swap(unexpected<E>& a, unexpected<E>& b) noexcept(noexcept(a.swap(b))) { a.swap(b); }

namespace expected_detail {
struct empty_t {};
struct void_value {};
struct invoke_value_t {};
struct invoke_error_t {};
enum class state : unsigned char { empty, value, error };

template<class T, class E, bool = std::is_trivially_destructible_v<T> && std::is_trivially_destructible_v<E>>
union payload {
    char empty;
    T value;
    E error;
    constexpr explicit payload(empty_t) noexcept : empty{} {}
    template<class... Args> constexpr explicit payload(std::in_place_t, Args&&... args) : value(std::forward<Args>(args)...) {}
    template<class... Args> constexpr explicit payload(unexpect_t, Args&&... args) : error(std::forward<Args>(args)...) {}
    template<class F> constexpr explicit payload(invoke_value_t, F&& function) : value(std::forward<F>(function)()) {}
    template<class F> constexpr explicit payload(invoke_error_t, F&& function) : error(std::forward<F>(function)()) {}
    ~payload() {}
};
template<class T, class E> union payload<T, E, true> {
    char empty;
    T value;
    E error;
    constexpr explicit payload(empty_t) noexcept : empty{} {}
    template<class... Args> constexpr explicit payload(std::in_place_t, Args&&... args) : value(std::forward<Args>(args)...) {}
    template<class... Args> constexpr explicit payload(unexpect_t, Args&&... args) : error(std::forward<Args>(args)...) {}
    template<class F> constexpr explicit payload(invoke_value_t, F&& function) : value(std::forward<F>(function)()) {}
    template<class F> constexpr explicit payload(invoke_error_t, F&& function) : error(std::forward<F>(function)()) {}
    ~payload() = default;
};
template<class T, class... Args> void construct(T* address, Args&&... args) {
    ::new (const_cast<void*>(static_cast<const volatile void*>(address))) T(std::forward<Args>(args)...);
}

// Replace an active union alternative while keeping a recoverable old value
// until every potentially throwing operation needed for the transition succeeds.
template<class New, class Old, class... Args>
void replace(New* next, Old* old, Args&&... args) {
    if constexpr (std::is_nothrow_constructible_v<New, Args...>) {
        old->~Old();
        construct(next, std::forward<Args>(args)...);
    } else if constexpr (std::is_nothrow_move_constructible_v<New>) {
        New temporary(std::forward<Args>(args)...);
        old->~Old();
        construct(next, std::move(temporary));
    } else {
        static_assert(std::is_nothrow_move_constructible_v<Old>);
        Old backup(std::move(*old));
        old->~Old();
        try { construct(next, std::forward<Args>(args)...); }
        catch (...) { construct(old, std::move(backup)); throw; }
    }
}

template<class T, class E> struct storage_common {
    payload<T, E> data_;
    state state_;
    constexpr explicit storage_common(empty_t tag) noexcept : data_(tag), state_(state::empty) {}
    template<class... Args> constexpr explicit storage_common(std::in_place_t tag, Args&&... args)
        : data_(tag, std::forward<Args>(args)...), state_(state::value) {}
    template<class... Args> constexpr explicit storage_common(unexpect_t tag, Args&&... args)
        : data_(tag, std::forward<Args>(args)...), state_(state::error) {}
    template<class F> constexpr explicit storage_common(invoke_value_t tag, F&& function)
        : data_(tag, std::forward<F>(function)), state_(state::value) {}
    template<class F> constexpr explicit storage_common(invoke_error_t tag, F&& function)
        : data_(tag, std::forward<F>(function)), state_(state::error) {}
    template<class... Args> void construct_value(Args&&... args) {
        construct(std::addressof(data_.value), std::forward<Args>(args)...);
        state_ = state::value;
    }
    template<class... Args> void construct_error(Args&&... args) {
        construct(std::addressof(data_.error), std::forward<Args>(args)...);
        state_ = state::error;
    }
    void destroy() noexcept {
        if (state_ == state::value) data_.value.~T();
        else if (state_ == state::error) data_.error.~E();
        state_ = state::empty;
    }
    template<class U> void assign_value(U&& value) {
        if (state_ == state::value) data_.value = std::forward<U>(value);
        else {
            replace(std::addressof(data_.value), std::addressof(data_.error), std::forward<U>(value));
            state_ = state::value;
        }
    }
    template<class G> void assign_error(G&& error) {
        if (state_ == state::error) data_.error = std::forward<G>(error);
        else {
            replace(std::addressof(data_.error), std::addressof(data_.value), std::forward<G>(error));
            state_ = state::error;
        }
    }
    void swap_storage(storage_common& other) {
        using std::swap;
        if (state_ == other.state_) {
            if (state_ == state::value) swap(data_.value, other.data_.value);
            else swap(data_.error, other.data_.error);
        } else if (state_ == state::error) {
            other.swap_storage(*this);
        } else if constexpr (std::is_nothrow_move_constructible_v<E>) {
            E backup(std::move(other.data_.error));
            other.destroy();
            try { other.construct_value(std::move(data_.value)); }
            catch (...) { other.construct_error(std::move(backup)); throw; }
            destroy();
            construct_error(std::move(backup));
        } else {
            static_assert(std::is_nothrow_move_constructible_v<T>);
            T backup(std::move(data_.value));
            destroy();
            try { construct_error(std::move(other.data_.error)); }
            catch (...) { construct_value(std::move(backup)); throw; }
            other.destroy();
            other.construct_value(std::move(backup));
        }
    }
};

template<class T, class E, bool = std::is_trivially_destructible_v<T> && std::is_trivially_destructible_v<E>>
struct storage : storage_common<T, E> {
    using storage_common<T, E>::storage_common;
    storage(const storage&) = default;
    storage(storage&&) = default;
    storage& operator=(const storage&) = default;
    storage& operator=(storage&&) = default;
    ~storage() { this->destroy(); }
};
template<class T, class E> struct storage<T, E, true> : storage_common<T, E> {
    using storage_common<T, E>::storage_common;
};

template<class T, class E, bool = std::is_trivially_copy_constructible_v<T> && std::is_trivially_copy_constructible_v<E>>
struct copy_storage : storage<T, E> {
    using storage<T, E>::storage;
    copy_storage(const copy_storage& other)
        noexcept(std::is_nothrow_copy_constructible_v<T> && std::is_nothrow_copy_constructible_v<E>)
        : storage<T, E>(empty_t{}) {
        if (other.state_ == state::value) this->construct_value(other.data_.value);
        else this->construct_error(other.data_.error);
    }
    copy_storage(copy_storage&&) = default;
    copy_storage& operator=(const copy_storage&) = default;
    copy_storage& operator=(copy_storage&&) = default;
};
template<class T, class E> struct copy_storage<T, E, true> : storage<T, E> { using storage<T, E>::storage; };

template<class T, class E, bool = std::is_trivially_move_constructible_v<T> && std::is_trivially_move_constructible_v<E>>
struct move_storage : copy_storage<T, E> {
    using copy_storage<T, E>::copy_storage;
    move_storage(const move_storage&) = default;
    move_storage(move_storage&& other)
        noexcept(std::is_nothrow_move_constructible_v<T> && std::is_nothrow_move_constructible_v<E>)
        : copy_storage<T, E>(empty_t{}) {
        if (other.state_ == state::value) this->construct_value(std::move(other.data_.value));
        else this->construct_error(std::move(other.data_.error));
    }
    move_storage& operator=(const move_storage&) = default;
    move_storage& operator=(move_storage&&) = default;
};
template<class T, class E> struct move_storage<T, E, true> : copy_storage<T, E> { using copy_storage<T, E>::copy_storage; };

template<class T, class E> struct assign_storage : move_storage<T, E> {
    using move_storage<T, E>::move_storage;
    assign_storage(const assign_storage&) = default;
    assign_storage(assign_storage&&) = default;
    assign_storage& operator=(const assign_storage& other)
        noexcept(std::is_nothrow_copy_constructible_v<T> && std::is_nothrow_copy_constructible_v<E> &&
                 std::is_nothrow_copy_assignable_v<T> && std::is_nothrow_copy_assignable_v<E>) {
        if (other.state_ == state::value) this->assign_value(other.data_.value);
        else this->assign_error(other.data_.error);
        return *this;
    }
    assign_storage& operator=(assign_storage&& other)
        noexcept(std::is_nothrow_move_constructible_v<T> && std::is_nothrow_move_constructible_v<E> &&
                 std::is_nothrow_move_assignable_v<T> && std::is_nothrow_move_assignable_v<E>) {
        if (other.state_ == state::value) this->assign_value(std::move(other.data_.value));
        else this->assign_error(std::move(other.data_.error));
        return *this;
    }
};

// Defaulted public special members inherit deletion and triviality from these
// bases. A constructor merely containing an ill-formed body is not sufficient:
// type traits must reject copying an expected holding a move-only alternative.
template<bool> struct copy_control {
    copy_control() = default; copy_control(const copy_control&) = default; copy_control(copy_control&&) = default;
    copy_control& operator=(const copy_control&) = default; copy_control& operator=(copy_control&&) = default;
};
template<> struct copy_control<false> {
    copy_control() = default; copy_control(const copy_control&) = delete; copy_control(copy_control&&) = default;
    copy_control& operator=(const copy_control&) = default; copy_control& operator=(copy_control&&) = default;
};
template<bool> struct move_control {
    move_control() = default; move_control(const move_control&) = default; move_control(move_control&&) = default;
    move_control& operator=(const move_control&) = default; move_control& operator=(move_control&&) = default;
};
template<> struct move_control<false> {
    move_control() = default; move_control(const move_control&) = default; move_control(move_control&&) = delete;
    move_control& operator=(const move_control&) = default; move_control& operator=(move_control&&) = default;
};
template<bool> struct copy_assign_control {
    copy_assign_control() = default; copy_assign_control(const copy_assign_control&) = default;
    copy_assign_control(copy_assign_control&&) = default;
    copy_assign_control& operator=(const copy_assign_control&) = default;
    copy_assign_control& operator=(copy_assign_control&&) = default;
};
template<> struct copy_assign_control<false> {
    copy_assign_control() = default; copy_assign_control(const copy_assign_control&) = default;
    copy_assign_control(copy_assign_control&&) = default;
    copy_assign_control& operator=(const copy_assign_control&) = delete;
    copy_assign_control& operator=(copy_assign_control&&) = default;
};
template<bool> struct move_assign_control {
    move_assign_control() = default; move_assign_control(const move_assign_control&) = default;
    move_assign_control(move_assign_control&&) = default;
    move_assign_control& operator=(const move_assign_control&) = default;
    move_assign_control& operator=(move_assign_control&&) = default;
};
template<> struct move_assign_control<false> {
    move_assign_control() = default; move_assign_control(const move_assign_control&) = default;
    move_assign_control(move_assign_control&&) = default;
    move_assign_control& operator=(const move_assign_control&) = default;
    move_assign_control& operator=(move_assign_control&&) = delete;
};
template<class T, class E> struct controls :
    copy_control<std::is_copy_constructible_v<T> && std::is_copy_constructible_v<E>>,
    move_control<std::is_move_constructible_v<T> && std::is_move_constructible_v<E>>,
    copy_assign_control<std::is_copy_constructible_v<T> && std::is_copy_constructible_v<E> &&
        std::is_copy_assignable_v<T> && std::is_copy_assignable_v<E> &&
        (std::is_nothrow_move_constructible_v<T> || std::is_nothrow_move_constructible_v<E>)>,
    move_assign_control<std::is_move_constructible_v<T> && std::is_move_constructible_v<E> &&
        std::is_move_assignable_v<T> && std::is_move_assignable_v<E> &&
        (std::is_nothrow_move_constructible_v<T> || std::is_nothrow_move_constructible_v<E>)> {};

template<class T, class From> inline constexpr bool consumes_expected =
    std::is_constructible_v<T, From&> || std::is_constructible_v<T, const From&> ||
    std::is_constructible_v<T, From&&> || std::is_constructible_v<T, const From&&> ||
    std::is_convertible_v<From&, T> || std::is_convertible_v<const From&, T> ||
    std::is_convertible_v<From&&, T> || std::is_convertible_v<const From&&, T>;
template<class T, class E, class U, class G> inline constexpr bool converting_ok =
    (!consumes_expected<T, expected<U, G>> || std::is_same_v<std::remove_cv_t<T>, bool>) &&
    !consumes_expected<unexpected<E>, expected<U, G>>;

template<class Self, class F> constexpr decltype(auto) invoke_value(Self&& self, F&& function) {
    if constexpr (std::is_void_v<typename remove_cvref_t<Self>::value_type>)
        return std::invoke(std::forward<F>(function));
    else return std::invoke(std::forward<F>(function), *std::forward<Self>(self));
}
template<class Self, class F> constexpr auto and_then(Self&& self, F&& function) {
    using R = remove_cvref_t<decltype(invoke_value(std::forward<Self>(self), std::forward<F>(function)))>;
    static_assert(is_expected<R>::value, "and_then must return expected");
    static_assert(std::is_same_v<typename R::error_type, typename remove_cvref_t<Self>::error_type>,
                  "and_then must preserve the error type");
    if (self.has_value()) return invoke_value(std::forward<Self>(self), std::forward<F>(function));
    return R(unexpect, std::forward<Self>(self).error());
}
template<class Self, class F> constexpr auto or_else(Self&& self, F&& function) {
    using R = remove_cvref_t<std::invoke_result_t<F, decltype(std::forward<Self>(self).error())>>;
    using T = typename remove_cvref_t<Self>::value_type;
    static_assert(is_expected<R>::value, "or_else must return expected");
    static_assert(std::is_same_v<typename R::value_type, T>, "or_else must preserve the value type");
    if (!self.has_value()) return std::invoke(std::forward<F>(function), std::forward<Self>(self).error());
    if constexpr (std::is_void_v<T>) return R();
    else return R(std::in_place, *std::forward<Self>(self));
}
template<class Self, class F> constexpr auto transform(Self&& self, F&& function) {
    using U = std::remove_cv_t<decltype(invoke_value(std::forward<Self>(self), std::forward<F>(function)))>;
    using R = expected<U, typename remove_cvref_t<Self>::error_type>;
    if (!self.has_value()) return R(unexpect, std::forward<Self>(self).error());
    if constexpr (std::is_void_v<U>) {
        invoke_value(std::forward<Self>(self), std::forward<F>(function)); return R();
    } else return R(invoke_value_t{}, [&]() -> U {
        return invoke_value(std::forward<Self>(self), std::forward<F>(function));
    });
}
template<class Self, class F> constexpr auto transform_error(Self&& self, F&& function) {
    using G = std::remove_cv_t<std::invoke_result_t<F, decltype(std::forward<Self>(self).error())>>;
    using T = typename remove_cvref_t<Self>::value_type;
    using R = expected<T, G>;
    if (!self.has_value()) return R(invoke_error_t{}, [&]() -> G {
        return std::invoke(std::forward<F>(function), std::forward<Self>(self).error());
    });
    if constexpr (std::is_void_v<T>) return R();
    else return R(std::in_place, *std::forward<Self>(self));
}
}

template<class T, class E>
class expected : private expected_detail::assign_storage<std::conditional_t<std::is_void_v<T>, expected_detail::void_value, T>, E>,
                 private expected_detail::controls<std::conditional_t<std::is_void_v<T>, expected_detail::void_value, T>, E> {
    static_assert((std::is_object_v<T> && !std::is_array_v<T>) || std::is_void_v<T>, "invalid expected value type");
    static_assert(!std::is_same_v<std::remove_cv_t<T>, std::in_place_t> &&
                  !std::is_same_v<std::remove_cv_t<T>, unexpect_t> &&
                  !expected_detail::is_unexpected<std::remove_cv_t<T>>::value, "invalid expected value type");
    static_assert(expected_detail::error_type_ok<E>, "invalid expected error type");
    using V = std::conditional_t<std::is_void_v<T>, expected_detail::void_value, T>;
    using base = expected_detail::assign_storage<V, E>;
    template<class U> static constexpr bool value_argument =
        !std::is_same_v<expected_detail::remove_cvref_t<U>, expected> &&
        !std::is_same_v<expected_detail::remove_cvref_t<U>, std::in_place_t> &&
        !std::is_same_v<expected_detail::remove_cvref_t<U>, unexpect_t> &&
        !expected_detail::is_unexpected<expected_detail::remove_cvref_t<U>>::value &&
        !(std::is_same_v<std::remove_cv_t<T>, bool> &&
          expected_detail::is_expected<expected_detail::remove_cvref_t<U>>::value) &&
        !std::is_void_v<T> && std::is_constructible_v<T, U>;
    template<class U> using lref = std::conditional_t<std::is_void_v<U>, void, std::add_lvalue_reference_t<U>>;
    template<class U> using rref = std::conditional_t<std::is_void_v<U>, void, std::add_rvalue_reference_t<U>>;
    template<class Self, class F> friend constexpr auto expected_detail::transform(Self&&, F&&);
    template<class Self, class F> friend constexpr auto expected_detail::transform_error(Self&&, F&&);
    template<class F> constexpr expected(expected_detail::invoke_value_t tag, F&& function)
        : base(tag, std::forward<F>(function)) {}
    template<class F> constexpr expected(expected_detail::invoke_error_t tag, F&& function)
        : base(tag, std::forward<F>(function)) {}

public:
    using value_type = T;
    using error_type = E;
    using unexpected_type = unexpected<E>;
    template<class U> using rebind = expected<U, E>;

    template<class U = V, std::enable_if_t<std::is_default_constructible_v<U>, int> = 0>
    constexpr expected() noexcept(std::is_nothrow_default_constructible_v<V>) : base(std::in_place) {}
    expected(const expected&) = default;
    expected(expected&&) = default;
    expected& operator=(const expected&) = default;
    expected& operator=(expected&&) = default;
    ~expected() = default;

    template<class U = T, std::enable_if_t<value_argument<U> && std::is_convertible_v<U, T>, int> = 0>
    constexpr expected(U&& value) noexcept(std::is_nothrow_constructible_v<T, U>) : base(std::in_place, std::forward<U>(value)) {}
    template<class U = T, std::enable_if_t<value_argument<U> && !std::is_convertible_v<U, T>, int> = 0>
    constexpr explicit expected(U&& value) noexcept(std::is_nothrow_constructible_v<T, U>) : base(std::in_place, std::forward<U>(value)) {}

    template<class... Args, std::enable_if_t<std::is_constructible_v<V, Args...> &&
        (!std::is_void_v<T> || sizeof...(Args) == 0), int> = 0>
    constexpr explicit expected(std::in_place_t tag, Args&&... args)
        noexcept(std::is_nothrow_constructible_v<V, Args...>) : base(tag, std::forward<Args>(args)...) {}
    template<class U, class... Args, std::enable_if_t<
        !std::is_void_v<T> && std::is_constructible_v<T, std::initializer_list<U>&, Args...>, int> = 0>
    constexpr explicit expected(std::in_place_t tag, std::initializer_list<U> list, Args&&... args)
        noexcept(std::is_nothrow_constructible_v<T, std::initializer_list<U>&, Args...>)
        : base(tag, list, std::forward<Args>(args)...) {}
    template<class... Args, std::enable_if_t<std::is_constructible_v<E, Args...>, int> = 0>
    constexpr explicit expected(unexpect_t tag, Args&&... args)
        noexcept(std::is_nothrow_constructible_v<E, Args...>) : base(tag, std::forward<Args>(args)...) {}
    template<class U, class... Args,
             std::enable_if_t<std::is_constructible_v<E, std::initializer_list<U>&, Args...>, int> = 0>
    constexpr explicit expected(unexpect_t tag, std::initializer_list<U> list, Args&&... args)
        noexcept(std::is_nothrow_constructible_v<E, std::initializer_list<U>&, Args...>)
        : base(tag, list, std::forward<Args>(args)...) {}

    template<class G, std::enable_if_t<std::is_constructible_v<E, const G&> &&
        std::is_convertible_v<const G&, E>, int> = 0>
    constexpr expected(const unexpected<G>& error)
        noexcept(std::is_nothrow_constructible_v<E, const G&>) : base(unexpect, error.error()) {}

    template<class G, std::enable_if_t<std::is_constructible_v<E, const G&> &&
        !std::is_convertible_v<const G&, E>, int> = 0>
    constexpr explicit expected(const unexpected<G>& error)
        noexcept(std::is_nothrow_constructible_v<E, const G&>) : base(unexpect, error.error()) {}

    template<class U, class G, std::enable_if_t<
        ((std::is_void_v<T> && std::is_void_v<U>) ||
         (!std::is_void_v<T> && std::is_constructible_v<T, std::add_lvalue_reference_t<const U>>)) && std::is_constructible_v<E, const G&> &&
        expected_detail::converting_ok<T, E, U, G> &&
        (((std::is_void_v<T> && std::is_void_v<U>) || std::is_convertible_v<std::add_lvalue_reference_t<const U>, T>) && std::is_convertible_v<const G&, E>), int> = 0>
    constexpr expected(const expected<U, G>& other)
        noexcept((std::is_void_v<T> || std::is_nothrow_constructible_v<T, std::add_lvalue_reference_t<const U>>) &&
                 std::is_nothrow_constructible_v<E, const G&>) : base(expected_detail::empty_t{}) {
        if (other.has_value()) {
            if constexpr (std::is_void_v<T>) this->construct_value();
            else this->construct_value(*other);
        } else this->construct_error(other.error());
    }

    template<class U, class G, std::enable_if_t<
        ((std::is_void_v<T> && std::is_void_v<U>) ||
         (!std::is_void_v<T> && std::is_constructible_v<T, std::add_lvalue_reference_t<const U>>)) && std::is_constructible_v<E, const G&> &&
        expected_detail::converting_ok<T, E, U, G> &&
        !(((std::is_void_v<T> && std::is_void_v<U>) || std::is_convertible_v<std::add_lvalue_reference_t<const U>, T>) && std::is_convertible_v<const G&, E>), int> = 0>
    constexpr explicit expected(const expected<U, G>& other)
        noexcept((std::is_void_v<T> || std::is_nothrow_constructible_v<T, std::add_lvalue_reference_t<const U>>) &&
                 std::is_nothrow_constructible_v<E, const G&>) : base(expected_detail::empty_t{}) {
        if (other.has_value()) {
            if constexpr (std::is_void_v<T>) this->construct_value();
            else this->construct_value(*other);
        } else this->construct_error(other.error());
    }

    template<class G, std::enable_if_t<std::is_constructible_v<E, G&&> &&
        std::is_convertible_v<G&&, E>, int> = 0>
    constexpr expected(unexpected<G>&& error)
        noexcept(std::is_nothrow_constructible_v<E, G&&>) : base(unexpect, std::move(error).error()) {}

    template<class G, std::enable_if_t<std::is_constructible_v<E, G&&> &&
        !std::is_convertible_v<G&&, E>, int> = 0>
    constexpr explicit expected(unexpected<G>&& error)
        noexcept(std::is_nothrow_constructible_v<E, G&&>) : base(unexpect, std::move(error).error()) {}

    template<class U, class G, std::enable_if_t<
        ((std::is_void_v<T> && std::is_void_v<U>) ||
         (!std::is_void_v<T> && std::is_constructible_v<T, std::add_rvalue_reference_t<U>>)) && std::is_constructible_v<E, G&&> &&
        expected_detail::converting_ok<T, E, U, G> &&
        (((std::is_void_v<T> && std::is_void_v<U>) || std::is_convertible_v<std::add_rvalue_reference_t<U>, T>) && std::is_convertible_v<G&&, E>), int> = 0>
    constexpr expected(expected<U, G>&& other)
        noexcept((std::is_void_v<T> || std::is_nothrow_constructible_v<T, std::add_rvalue_reference_t<U>>) &&
                 std::is_nothrow_constructible_v<E, G&&>) : base(expected_detail::empty_t{}) {
        if (other.has_value()) {
            if constexpr (std::is_void_v<T>) this->construct_value();
            else this->construct_value(*std::move(other));
        } else this->construct_error(std::move(other).error());
    }

    template<class U, class G, std::enable_if_t<
        ((std::is_void_v<T> && std::is_void_v<U>) ||
         (!std::is_void_v<T> && std::is_constructible_v<T, std::add_rvalue_reference_t<U>>)) && std::is_constructible_v<E, G&&> &&
        expected_detail::converting_ok<T, E, U, G> &&
        !(((std::is_void_v<T> && std::is_void_v<U>) || std::is_convertible_v<std::add_rvalue_reference_t<U>, T>) && std::is_convertible_v<G&&, E>), int> = 0>
    constexpr explicit expected(expected<U, G>&& other)
        noexcept((std::is_void_v<T> || std::is_nothrow_constructible_v<T, std::add_rvalue_reference_t<U>>) &&
                 std::is_nothrow_constructible_v<E, G&&>) : base(expected_detail::empty_t{}) {
        if (other.has_value()) {
            if constexpr (std::is_void_v<T>) this->construct_value();
            else this->construct_value(*std::move(other));
        } else this->construct_error(std::move(other).error());
    }

    template<class U = T, std::enable_if_t<value_argument<U> && std::is_assignable_v<std::add_lvalue_reference_t<T>, U> &&
        (std::is_nothrow_constructible_v<T, U> || std::is_nothrow_move_constructible_v<T> ||
         std::is_nothrow_move_constructible_v<E>), int> = 0>
    expected& operator=(U&& value) {
        this->assign_value(std::forward<U>(value)); return *this;
    }

    template<class G, std::enable_if_t<std::is_constructible_v<E, const G&> &&
        std::is_assignable_v<E&, const G&> && (std::is_nothrow_constructible_v<E, const G&> ||
            std::is_nothrow_move_constructible_v<V> || std::is_nothrow_move_constructible_v<E>), int> = 0>
    expected& operator=(const unexpected<G>& error) {
        this->assign_error(error.error()); return *this;
    }

    template<class G, std::enable_if_t<std::is_constructible_v<E, G&&> &&
        std::is_assignable_v<E&, G&&> && (std::is_nothrow_constructible_v<E, G&&> ||
            std::is_nothrow_move_constructible_v<V> || std::is_nothrow_move_constructible_v<E>), int> = 0>
    expected& operator=(unexpected<G>&& error) {
        this->assign_error(std::move(error).error()); return *this;
    }

    template<class... Args, std::enable_if_t<std::is_nothrow_constructible_v<V, Args...> &&
        (!std::is_void_v<T> || sizeof...(Args) == 0), int> = 0>
    lref<T> emplace(Args&&... args) noexcept {
        this->destroy();
        this->construct_value(std::forward<Args>(args)...);
        if constexpr (!std::is_void_v<T>) return this->data_.value;
    }
    template<class U, class... Args, std::enable_if_t<
        !std::is_void_v<T> && std::is_nothrow_constructible_v<T, std::initializer_list<U>&, Args...>, int> = 0>
    lref<T> emplace(std::initializer_list<U> list, Args&&... args) noexcept {
        this->destroy(); this->construct_value(list, std::forward<Args>(args)...);
        return this->data_.value;
    }
    template<class U = V, std::enable_if_t<
        std::is_swappable_v<U> && std::is_swappable_v<E> &&
        std::is_move_constructible_v<U> && std::is_move_constructible_v<E> &&
        (std::is_nothrow_move_constructible_v<U> || std::is_nothrow_move_constructible_v<E>), int> = 0>
    void swap(expected& other) noexcept(std::is_nothrow_move_constructible_v<V> &&
        std::is_nothrow_move_constructible_v<E> && std::is_nothrow_swappable_v<V> && std::is_nothrow_swappable_v<E>) {
        this->swap_storage(other);
    }
    constexpr bool has_value() const noexcept { return this->state_ == expected_detail::state::value; }
    constexpr explicit operator bool() const noexcept { return has_value(); }
    template<class U = T, std::enable_if_t<!std::is_void_v<U>, int> = 0>
    constexpr U* operator->() noexcept { assert(has_value()); return std::addressof(this->data_.value); }
    template<class U = T, std::enable_if_t<!std::is_void_v<U>, int> = 0>
    constexpr const U* operator->() const noexcept { assert(has_value()); return std::addressof(this->data_.value); }

    constexpr lref<T> operator*() & noexcept {
        assert(has_value());
        if constexpr (!std::is_void_v<T>) return (this->data_.value);
    }
    constexpr lref<T> value() & {
        static_assert(std::is_copy_constructible_v<E>, "value requires a copyable error");
        if (!has_value()) throw bad_expected_access<E>(static_cast<const E&>(this->data_.error));
        if constexpr (!std::is_void_v<T>) return (this->data_.value);
    }
    constexpr E& error() & noexcept {
        assert(!has_value()); return (this->data_.error);
    }

    template<class F, std::enable_if_t<std::is_constructible_v<E, E&> &&
                                      std::is_same_v<F, F>, int> = 0>
    constexpr auto and_then(F&& function) & {
        return expected_detail::and_then(*this, std::forward<F>(function));
    }

    template<class F, std::enable_if_t<std::is_constructible_v<V, V&> &&
                                      std::is_same_v<F, F>, int> = 0>
    constexpr auto or_else(F&& function) & {
        return expected_detail::or_else(*this, std::forward<F>(function));
    }

    template<class F, std::enable_if_t<std::is_constructible_v<E, E&> &&
                                      std::is_same_v<F, F>, int> = 0>
    constexpr auto transform(F&& function) & {
        return expected_detail::transform(*this, std::forward<F>(function));
    }

    template<class F, std::enable_if_t<std::is_constructible_v<V, V&> &&
                                      std::is_same_v<F, F>, int> = 0>
    constexpr auto transform_error(F&& function) & {
        return expected_detail::transform_error(*this, std::forward<F>(function));
    }

    constexpr lref<const T> operator*() const & noexcept {
        assert(has_value());
        if constexpr (!std::is_void_v<T>) return (this->data_.value);
    }
    constexpr lref<const T> value() const & {
        static_assert(std::is_copy_constructible_v<E>, "value requires a copyable error");
        if (!has_value()) throw bad_expected_access<E>(static_cast<const E&>(this->data_.error));
        if constexpr (!std::is_void_v<T>) return (this->data_.value);
    }
    constexpr const E& error() const & noexcept {
        assert(!has_value()); return (this->data_.error);
    }

    template<class F, std::enable_if_t<std::is_constructible_v<E, const E&> &&
                                      std::is_same_v<F, F>, int> = 0>
    constexpr auto and_then(F&& function) const & {
        return expected_detail::and_then(*this, std::forward<F>(function));
    }

    template<class F, std::enable_if_t<std::is_constructible_v<V, const V&> &&
                                      std::is_same_v<F, F>, int> = 0>
    constexpr auto or_else(F&& function) const & {
        return expected_detail::or_else(*this, std::forward<F>(function));
    }

    template<class F, std::enable_if_t<std::is_constructible_v<E, const E&> &&
                                      std::is_same_v<F, F>, int> = 0>
    constexpr auto transform(F&& function) const & {
        return expected_detail::transform(*this, std::forward<F>(function));
    }

    template<class F, std::enable_if_t<std::is_constructible_v<V, const V&> &&
                                      std::is_same_v<F, F>, int> = 0>
    constexpr auto transform_error(F&& function) const & {
        return expected_detail::transform_error(*this, std::forward<F>(function));
    }

    constexpr rref<T> operator*() && noexcept {
        assert(has_value());
        if constexpr (!std::is_void_v<T>) return std::move(this->data_.value);
    }
    constexpr rref<T> value() && {
        static_assert(std::is_copy_constructible_v<E> && std::is_move_constructible_v<E>,
                      "value requires a copyable and movable error");
        if (!has_value()) throw bad_expected_access<E>(std::move(this->data_.error));
        if constexpr (!std::is_void_v<T>) return std::move(this->data_.value);
    }
    constexpr E&& error() && noexcept {
        assert(!has_value()); return std::move(this->data_.error);
    }

    template<class F, std::enable_if_t<std::is_constructible_v<E, E&&> &&
                                      std::is_same_v<F, F>, int> = 0>
    constexpr auto and_then(F&& function) && {
        return expected_detail::and_then(std::move(*this), std::forward<F>(function));
    }

    template<class F, std::enable_if_t<std::is_constructible_v<V, V&&> &&
                                      std::is_same_v<F, F>, int> = 0>
    constexpr auto or_else(F&& function) && {
        return expected_detail::or_else(std::move(*this), std::forward<F>(function));
    }

    template<class F, std::enable_if_t<std::is_constructible_v<E, E&&> &&
                                      std::is_same_v<F, F>, int> = 0>
    constexpr auto transform(F&& function) && {
        return expected_detail::transform(std::move(*this), std::forward<F>(function));
    }

    template<class F, std::enable_if_t<std::is_constructible_v<V, V&&> &&
                                      std::is_same_v<F, F>, int> = 0>
    constexpr auto transform_error(F&& function) && {
        return expected_detail::transform_error(std::move(*this), std::forward<F>(function));
    }

    constexpr rref<const T> operator*() const && noexcept {
        assert(has_value());
        if constexpr (!std::is_void_v<T>) return std::move(this->data_.value);
    }
    constexpr rref<const T> value() const && {
        static_assert(std::is_copy_constructible_v<E>, "value requires a copyable error");
        if (!has_value()) {
            // The void specialization's native const& observer also handles
            // const rvalues. Its exception copies, rather than const-moves, E.
            if constexpr (std::is_void_v<T>)
                throw bad_expected_access<E>(static_cast<const E&>(this->data_.error));
            else throw bad_expected_access<E>(std::move(this->data_.error));
        }
        if constexpr (!std::is_void_v<T>) return std::move(this->data_.value);
    }
    constexpr const E&& error() const && noexcept {
        assert(!has_value()); return std::move(this->data_.error);
    }

    template<class F, std::enable_if_t<std::is_constructible_v<E, const E&&> &&
                                      std::is_same_v<F, F>, int> = 0>
    constexpr auto and_then(F&& function) const && {
        return expected_detail::and_then(std::move(*this), std::forward<F>(function));
    }

    template<class F, std::enable_if_t<std::is_constructible_v<V, const V&&> &&
                                      std::is_same_v<F, F>, int> = 0>
    constexpr auto or_else(F&& function) const && {
        return expected_detail::or_else(std::move(*this), std::forward<F>(function));
    }

    template<class F, std::enable_if_t<std::is_constructible_v<E, const E&&> &&
                                      std::is_same_v<F, F>, int> = 0>
    constexpr auto transform(F&& function) const && {
        return expected_detail::transform(std::move(*this), std::forward<F>(function));
    }

    template<class F, std::enable_if_t<std::is_constructible_v<V, const V&&> &&
                                      std::is_same_v<F, F>, int> = 0>
    constexpr auto transform_error(F&& function) const && {
        return expected_detail::transform_error(std::move(*this), std::forward<F>(function));
    }

    template<class U = std::remove_cv_t<T>, class Value = T, std::enable_if_t<!std::is_void_v<Value>, int> = 0>
    constexpr Value value_or(U&& fallback) const &
        noexcept(std::is_nothrow_copy_constructible_v<Value> &&
                 expected_detail::nothrow_convertible<U, Value>::value) {
        static_assert(std::is_copy_constructible_v<Value> && std::is_convertible_v<U, Value>);
        if (has_value()) return this->data_.value;
        return static_cast<Value>(std::forward<U>(fallback));
    }
    template<class G = E> constexpr E error_or(G&& fallback) const & {
        static_assert(std::is_copy_constructible_v<E> && std::is_convertible_v<G, E>);
        if (has_value()) return std::forward<G>(fallback);
        return this->data_.error;
    }

    template<class U = std::remove_cv_t<T>, class Value = T, std::enable_if_t<!std::is_void_v<Value>, int> = 0>
    constexpr Value value_or(U&& fallback) &&
        noexcept(std::is_nothrow_move_constructible_v<Value> &&
                 expected_detail::nothrow_convertible<U, Value>::value) {
        static_assert(std::is_move_constructible_v<Value> && std::is_convertible_v<U, Value>);
        if (has_value()) return std::move(this->data_.value);
        return static_cast<Value>(std::forward<U>(fallback));
    }
    template<class G = E> constexpr E error_or(G&& fallback) && {
        static_assert(std::is_move_constructible_v<E> && std::is_convertible_v<G, E>);
        if (has_value()) return std::forward<G>(fallback);
        return std::move(this->data_.error);
    }

};

template<class T, class E, class U, class G,
         std::enable_if_t<std::is_void_v<T> == std::is_void_v<U>, int> = 0>
constexpr bool operator==(const expected<T, E>& a, const expected<U, G>& b) {
    if (a.has_value() != b.has_value()) return false;
    if (!a.has_value()) return a.error() == b.error();
    if constexpr (std::is_void_v<T>) return true;
    else return *a == *b;
}
template<class T, class E, class U, class G,
         std::enable_if_t<std::is_void_v<T> == std::is_void_v<U>, int> = 0>
constexpr bool operator!=(const expected<T, E>& a, const expected<U, G>& b) { return !(a == b); }

template<class T, class E, class U, std::enable_if_t<!std::is_void_v<T> &&
    !expected_detail::is_expected<expected_detail::remove_cvref_t<U>>::value &&
    !expected_detail::is_unexpected<expected_detail::remove_cvref_t<U>>::value, int> = 0>
constexpr bool operator==(const expected<T, E>& a, const U& b) { return a.has_value() && *a == b; }
template<class T, class E, class U, std::enable_if_t<!std::is_void_v<T> &&
    !expected_detail::is_expected<expected_detail::remove_cvref_t<U>>::value &&
    !expected_detail::is_unexpected<expected_detail::remove_cvref_t<U>>::value, int> = 0>
constexpr bool operator==(const U& a, const expected<T, E>& b) { return b == a; }
template<class T, class E, class U, std::enable_if_t<!std::is_void_v<T> &&
    !expected_detail::is_expected<expected_detail::remove_cvref_t<U>>::value &&
    !expected_detail::is_unexpected<expected_detail::remove_cvref_t<U>>::value, int> = 0>
constexpr bool operator!=(const expected<T, E>& a, const U& b) { return !(a == b); }
template<class T, class E, class U, std::enable_if_t<!std::is_void_v<T> &&
    !expected_detail::is_expected<expected_detail::remove_cvref_t<U>>::value &&
    !expected_detail::is_unexpected<expected_detail::remove_cvref_t<U>>::value, int> = 0>
constexpr bool operator!=(const U& a, const expected<T, E>& b) { return !(b == a); }
template<class T, class E, class G>
constexpr bool operator==(const expected<T, E>& a, const unexpected<G>& b) { return !a.has_value() && a.error() == b.error(); }
template<class T, class E, class G>
constexpr bool operator==(const unexpected<G>& a, const expected<T, E>& b) { return b == a; }
template<class T, class E, class G>
constexpr bool operator!=(const expected<T, E>& a, const unexpected<G>& b) { return !(a == b); }
template<class T, class E, class G>
constexpr bool operator!=(const unexpected<G>& a, const expected<T, E>& b) { return !(b == a); }
template<class T, class E>
auto swap(expected<T, E>& a, expected<T, E>& b) noexcept(noexcept(a.swap(b))) -> decltype(a.swap(b)) { a.swap(b); }

}
#endif
