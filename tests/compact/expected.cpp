#ifdef LIGHTNERX_NATIVE_REFERENCE
#include <expected>
namespace subject = std;
#else
#include <lightnerx/compact/expected.hpp>
namespace subject = lightnerx::compact;
#endif

#include <cassert>
#include <cstdint>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>

using subject::expected;
using subject::unexpected;
using subject::unexpect;

struct NoDefault { explicit NoDefault(int) {} };
struct Explicit { explicit Explicit(int n) : value(n) {} int value; };
struct FallbackSource {};
struct FallbackValue {
    explicit FallbackValue(FallbackSource&&) noexcept {}
    FallbackValue(const FallbackSource&) noexcept(false) {}
};
struct AccessError {
    int category = 0;
    AccessError() = default;
    AccessError(const AccessError&) : category(1) {}
    AccessError(const AccessError&&) : category(2) {}
    AccessError(AccessError&& other) noexcept : category(other.category) {}
};
struct Immovable {
    int value;
    explicit Immovable(int n) : value(n) {}
    Immovable(const Immovable&) = delete;
    Immovable(Immovable&&) = delete;
};
struct alignas(64) Aligned { int value; };

template<bool NothrowMove> struct Tracked {
    inline static int live = 0;
    inline static bool fail_copy = false;
    inline static bool fail_move = false;
    int value;
    explicit Tracked(int n = 0) noexcept : value(n) { ++live; }
    Tracked(const Tracked& other) : value(other.value) {
        if (fail_copy) throw 71;
        ++live;
    }
    Tracked(Tracked&& other) noexcept(NothrowMove) : value(other.value) {
        if constexpr (!NothrowMove) if (fail_move) throw 72;
        other.value = -1;
        ++live;
    }
    Tracked& operator=(const Tracked& other) {
        if (fail_copy) throw 71;
        value = other.value; return *this;
    }
    Tracked& operator=(Tracked&& other) noexcept(NothrowMove) {
        if constexpr (!NothrowMove) if (fail_move) throw 72;
        value = std::exchange(other.value, -1); return *this;
    }
    ~Tracked() { --live; }
    friend void swap(Tracked& a, Tracked& b) noexcept(NothrowMove) {
        if constexpr (!NothrowMove) if (fail_move) throw 72;
        std::swap(a.value, b.value);
    }
    friend bool operator==(const Tracked& a, const Tracked& b) { return a.value == b.value; }
};

static_assert(std::is_trivially_destructible_v<expected<int, int>>);
static_assert(std::is_trivially_copy_constructible_v<expected<int, int>>);
static_assert(std::is_trivially_move_constructible_v<expected<int, int>>);
static_assert(!std::is_trivially_copy_assignable_v<expected<int, int>>);
static_assert(!std::is_copy_constructible_v<expected<std::unique_ptr<int>, int>>);
static_assert(!std::is_copy_assignable_v<expected<int, std::unique_ptr<int>>>);
static_assert(std::is_nothrow_move_constructible_v<expected<std::unique_ptr<int>, int>>);
static_assert(std::is_nothrow_move_assignable_v<expected<std::unique_ptr<int>, int>>);
static_assert(!std::is_copy_assignable_v<expected<Tracked<false>, Tracked<false>>>);
static_assert(!std::is_move_assignable_v<expected<Tracked<false>, Tracked<false>>>);
static_assert(!std::is_default_constructible_v<expected<NoDefault, int>>);
static_assert(std::is_default_constructible_v<expected<int, NoDefault>>);
static_assert(std::is_default_constructible_v<expected<const void, int>>);
static_assert(!std::is_default_constructible_v<unexpected<int>>);
static_assert(!std::is_destructible_v<subject::bad_expected_access<void>>);
static_assert(!noexcept(std::declval<const expected<FallbackValue, int>&>().value_or(FallbackSource{})));
static_assert(!noexcept(std::declval<expected<FallbackValue, int>&&>().value_or(FallbackSource{})));
static_assert(!std::is_copy_assignable_v<expected<const int, int>>);
static_assert(std::is_constructible_v<expected<Explicit, Explicit>, expected<int, int>>);
static_assert(!std::is_convertible_v<expected<int, int>, expected<Explicit, Explicit>>);
static_assert(!std::is_convertible_v<unexpected<int>, expected<int, Explicit>>);
static_assert(!std::is_convertible_v<expected<int, int>, bool>);
static_assert(alignof(expected<Aligned, int>) >= 64);
static_assert(alignof(expected<void, Aligned>) >= 64);
static_assert(std::is_same_v<expected<int, int>::rebind<std::string>, expected<std::string, int>>);
static_assert(std::is_same_v<decltype(std::declval<expected<int,int>&>().value()), int&>);
static_assert(std::is_same_v<decltype(std::declval<const expected<int,int>&>().value()), const int&>);
static_assert(std::is_same_v<decltype(std::declval<expected<int,int>&&>().value()), int&&>);
static_assert(std::is_same_v<decltype(std::declval<const expected<int,int>&&>().value()), const int&&>);
static_assert(std::is_same_v<decltype(*std::declval<expected<void,int>&>()), void>);
constexpr expected<int, int> constant(42);
constexpr expected<int, int> constant_error(unexpect, 7);
constexpr expected<void, int> constant_void;
static_assert(constant.has_value() && *constant == 42 && constant.value() == 42);
static_assert(!constant_error && constant_error.error() == 7);
static_assert(constant_void.has_value());

void construction() {
    expected<int, int> zero;
    assert(zero == 0 && 0 == zero && !(zero != 0) && !(0 != zero));
    expected<int, int> error(unexpect, 3);
    assert(error == unexpected(3) && unexpected(3) == error);
    assert(error != unexpected(4) && unexpected(4) != error);
    assert((zero != error && zero == expected<long, long>(0)));
    assert(error.value_or(9) == 9 && zero.value_or(9) == 0);
    assert(error.value_or({}) == 0 && std::move(error).value_or({}) == 0);
    assert(error.error_or(9) == 3 && zero.error_or(9) == 9);
    expected<std::vector<int>, int> list(std::in_place, {1, 2, 3});
    expected<int, std::vector<int>> error_list(unexpect, {4, 5});
    unexpected<std::vector<int>> unex(std::in_place, {6, 7});
    assert(list->size() == 3 && error_list.error().back() == 5 && unex.error()[0] == 6);
    expected<Explicit, Explicit> converted(zero);
    expected<Explicit, Explicit> converted_error(error);
    assert(converted->value == 0 && converted_error.error().value == 3);
    expected<std::unique_ptr<int>, std::unique_ptr<int>> pointer(std::in_place, new int(8));
    auto moved = std::move(pointer);
    assert(**moved == 8 && pointer.has_value() && !*pointer);
    moved = unexpected(std::make_unique<int>(9));
    assert(!moved && *moved.error() == 9);
    moved = std::make_unique<int>(10);
    assert(**moved == 10);
    expected<int, int> source_zero(0);
    expected<bool, int> boolean(source_zero);
    assert(boolean.has_value() && !*boolean);
    expected<bool, int> boolean_error(error);
    assert(!boolean_error && boolean_error.error() == 3);
    expected<void, int> success;
    expected<void, long> converted_void(success);
    assert((converted_void && success == expected<void, int>{}));
    success = unexpected(4);
    expected<const void, long> cv_void(success);
    assert(!cv_void && cv_void.error() == 4);
    try { success.value(); assert(false); }
    catch (const subject::bad_expected_access<int>& e) { assert(e.error() == 4 && e.what()[0]); }
    success.emplace();
    success.value();
    *success;
    assert(success);
    const expected<void, AccessError> void_error(unexpect);
    try { std::move(void_error).value(); assert(false); }
    catch (const subject::bad_expected_access<AccessError>& e) { assert(e.error().category == 1); }
    const expected<int, AccessError> value_error(unexpect);
    try { std::move(value_error).value(); assert(false); }
    catch (const subject::bad_expected_access<AccessError>& e) { assert(e.error().category == 2); }
    for (int qualifier = 0; qualifier != 4; ++qualifier) {
        expected<int, std::string> bad(unexpect, "error");
        try {
            if (qualifier == 0) bad.value();
            if (qualifier == 1) std::as_const(bad).value();
            if (qualifier == 2) std::move(bad).value();
            if (qualifier == 3) std::move(std::as_const(bad)).value();
            assert(false);
        } catch (const subject::bad_expected_access<std::string>& e) { assert(e.error() == "error"); }
    }
}

template<bool FastMove> void copy_failures() {
    using X = Tracked<FastMove>;
    assert(X::live == 0);
    {
        X input(5);
        expected<X, int> target(unexpect, 9);
        X::fail_copy = true;
        try { target = input; assert(false); } catch (int e) { assert(e == 71); }
        assert(!target && target.error() == 9 && X::live == 1);
        X::fail_copy = false;
        target = input;
        assert(target->value == 5 && X::live == 2);
        X::fail_copy = true;
        try { auto copy = target; (void)copy; assert(false); } catch (int e) { assert(e == 71); }
        assert(X::live == 2 && target->value == 5);
        X::fail_copy = false;
        expected<X, int> copied(target);
        assert(copied->value == 5);
        copied = unexpected(17);
        assert(!copied && X::live == 2);
        copied.emplace(6);
        assert(copied->value == 6 && X::live == 3);
    }
    assert(X::live == 0);
    {
        expected<int, X> target(12);
        unexpected<X> error(std::in_place, 15);
        X::fail_copy = true;
        try { target = error; assert(false); } catch (int e) { assert(e == 71); }
        assert(target == 12 && X::live == 1);
        X::fail_copy = false;
        target = error;
        assert(!target && target.error().value == 15 && X::live == 2);
        expected<void, X> nothing;
        X::fail_copy = true;
        try { nothing = error; assert(false); } catch (int e) { assert(e == 71); }
        assert(nothing && X::live == 2);
        X::fail_copy = false;
        nothing = error;
        assert(X::live == 3);
        nothing.emplace();
        assert(nothing && X::live == 2);
    }
    assert(X::live == 0);
}

void swap_failures() {
    using X = Tracked<false>;
    {
        expected<X, int> value(std::in_place, 6), error(unexpect, 9);
        X::fail_move = true;
        try { value.swap(error); assert(false); } catch (int e) { assert(e == 72); }
        assert(value->value == 6 && error.error() == 9 && X::live == 1);
        X::fail_move = false;
        using std::swap;
        swap(error, value);
        assert(value.error() == 9 && error->value == 6 && X::live == 1);
    }
    assert(X::live == 0);
    {
        expected<int, X> value(6), error(unexpect, 9);
        X::fail_move = true;
        try { value.swap(error); assert(false); } catch (int e) { assert(e == 72); }
        assert(value == 6 && error.error().value == 9 && X::live == 1);
        X::fail_move = false;
        value.swap(error);
        assert(value.error().value == 9 && error == 6 && X::live == 1);
    }
    assert(X::live == 0);
    {
        expected<void, X> a, b(unexpect, 7);
        X::fail_move = true;
        try { a.swap(b); assert(false); } catch (int e) { assert(e == 72); }
        assert(a && b.error().value == 7 && X::live == 1);
        X::fail_move = false;
        a.swap(b);
        assert(a.error().value == 7 && b && X::live == 1);
    }
    assert(X::live == 0);
}

void assignments() {
    using X = Tracked<true>;
    for (bool source_error : {false, true}) for (bool destination_error : {false, true}) {
        {
            expected<X, X> source = source_error ? expected<X, X>(unexpect, 20) : expected<X, X>(std::in_place, 10);
            expected<X, X> destination = destination_error ? expected<X, X>(unexpect, 40) : expected<X, X>(std::in_place, 30);
            assert(X::live == 2);
            X::fail_copy = true;
            try { destination = source; assert(false); } catch (int e) { assert(e == 71); }
            assert(destination.has_value() == !destination_error);
            assert((destination_error ? destination.error().value : destination->value) == (destination_error ? 40 : 30));
            assert(X::live == 2);
            X::fail_copy = false;
            destination = source;
            assert(destination.has_value() == !source_error);
            assert((source_error ? destination.error().value : destination->value) == (source_error ? 20 : 10));
            assert(X::live == 2);
            expected<X, X> moved(std::move(source));
            destination = std::move(moved);
            assert((source_error ? destination.error().value : destination->value) == (source_error ? 20 : 10));
            assert(X::live == 3);
        }
        assert(X::live == 0);
        {
            expected<int, int> first = source_error ? expected<int, int>(unexpect, 20) : expected<int, int>(10);
            expected<int, int> second = destination_error ? expected<int, int>(unexpect, 40) : expected<int, int>(30);
            const auto saved_first = first;
            const auto saved_second = second;
            first.swap(second);
            assert(first == saved_second && second == saved_first);
        }
    }
}

struct Qualifier {
    expected<int, int> operator()(int&) const { return 1; }
    expected<int, int> operator()(const int&) const { return 2; }
    expected<int, int> operator()(int&&) const { return 3; }
    expected<int, int> operator()(const int&&) const { return 4; }
};
struct Member { int number() const { return 7; } };

void monads() {
    expected<int, int> good(8), bad(unexpect, 9);
    assert(good.and_then(Qualifier{}) == 1);
    assert(std::as_const(good).and_then(Qualifier{}) == 2);
    assert(std::move(good).and_then(Qualifier{}) == 3);
    assert(std::move(std::as_const(good)).and_then(Qualifier{}) == 4);
    assert(bad.or_else(Qualifier{}) == 1);
    assert(std::as_const(bad).or_else(Qualifier{}) == 2);
    assert(std::move(bad).or_else(Qualifier{}) == 3);
    assert(std::move(std::as_const(bad)).or_else(Qualifier{}) == 4);
    assert(bad.and_then(Qualifier{}) == unexpected(9));
    assert(good.or_else(Qualifier{}) == 8);
    int invocations = 0;
    auto touch = [&](int n) { ++invocations; return n * 2; };
    assert(good.transform(touch) == 16 && invocations == 1);
    assert(bad.transform(touch) == unexpected(9) && invocations == 1);
    assert(bad.transform_error(touch) == unexpected(18) && invocations == 2);
    assert(good.transform_error(touch) == 8 && invocations == 2);
    auto nothing = good.transform([&](int) { ++invocations; });
    static_assert(std::is_same_v<decltype(nothing), expected<void, int>>);
    assert(nothing && invocations == 3);
    auto filled = nothing.transform([] { return 42; });
    assert(filled == 42);
    auto untouched = expected<void, int>(unexpect, 7).transform([&] { ++invocations; });
    assert(untouched == unexpected(7) && invocations == 3);
    assert(nothing.and_then([] { return expected<int, int>(6); }) == 6);
    assert(untouched.or_else([](int) { return expected<void, long>(); }));
    assert(untouched.transform_error([](int n) { return std::to_string(n); }).error() == "7");
    auto moving = expected<std::unique_ptr<int>, int>(std::in_place, new int(19))
        .and_then([](std::unique_ptr<int>&& p) { return expected<int, int>(*p); });
    assert(moving == 19);
    auto moving_error = expected<int, std::unique_ptr<int>>(unexpect, new int(20))
        .or_else([](std::unique_ptr<int>&& p) { return expected<int, int>(*p); });
    assert(moving_error == 20);
    auto immovable = good.transform([](int n) { return Immovable(n); });
    assert(immovable->value == 8);
    auto immovable_error = bad.transform_error([](int n) { return Immovable(n); });
    assert(immovable_error.error().value == 9);
    assert((expected<Member, int>{}.transform(&Member::number) == 7));
    bool threw = false;
    try { good.transform([](int) -> int { throw 83; }); } catch (int e) { threw = e == 83; }
    assert(threw && good == 8);
}

int main() {
    construction();
    copy_failures<false>();
    copy_failures<true>();
    swap_failures();
    assignments();
    monads();
    std::uint64_t digest = 0;
    for (int input = -32; input <= 32; ++input) {
        for (int failure = 0; failure <= 7; ++failure) {
            expected<int, int> result = failure ? expected<int, int>(unexpect, failure) : expected<int, int>(input);
            auto output = result.and_then([](int n) -> expected<int, int> {
                if (n < 0) return unexpected(9);
                return n * n;
            }).transform([](int n) { return n + 3; })
              .transform_error([](int n) { return n * 2; });
            digest = digest * 33 + static_cast<std::uint64_t>(output.value_or(-output.error_or(0)));
            auto recovered = output.or_else([](int n) -> expected<int, int> { return n + 100; });
            digest = digest * 33 + static_cast<unsigned>(*recovered);
        }
    }
    std::cout << "expected " << digest << '\n';
}
