#ifndef LIGHTNERX_RESULT_HPP
#define LIGHTNERX_RESULT_HPP

#include <cstddef>
#include <cstdlib>
#include <type_traits>
#include <utility>
#include <variant>

namespace lightnerx {
    enum class ErrorCode {
        InvalidArgument,
        NotConnected,
        AlreadyConnected,
        OperationInProgress,
        DeviceOrResourceBusy,
        ResourceUnavailableTryAgain,
        MessageSize,
        NotEnoughMemory,
        IoError
    };

    constexpr const char* errorMessage(ErrorCode code) noexcept {
        switch (code) {
            case ErrorCode::InvalidArgument: return "Invalid argument";
            case ErrorCode::NotConnected: return "Not connected";
            case ErrorCode::AlreadyConnected: return "Already connected";
            case ErrorCode::OperationInProgress: return "Operation in progress";
            case ErrorCode::DeviceOrResourceBusy: return "Device or resource busy";
            case ErrorCode::ResourceUnavailableTryAgain: return "Resource unavailable, try again";
            case ErrorCode::MessageSize: return "Message too large";
            case ErrorCode::NotEnoughMemory: return "Not enough memory";
            case ErrorCode::IoError: return "I/O error";
        }
        return "Unknown LightnerX error";
    }

    // Call value() on success and error() on failure. Invalid access aborts.
    template<class T, class E = ErrorCode>
    class [[nodiscard]] Result {
    public:
        constexpr Result(const T& value) noexcept(std::is_nothrow_copy_constructible_v<T>)
            : storage(std::in_place_index<0>, value) {}
        constexpr Result(T&& value) noexcept(std::is_nothrow_move_constructible_v<T>)
            : storage(std::in_place_index<0>, std::move(value)) {}

        template<class... Args>
        static constexpr Result failure(Args&&... args)
            noexcept(std::is_nothrow_constructible_v<E, Args...>) {
            return Result(std::in_place_index<1>, std::forward<Args>(args)...);
        }

        constexpr bool has_value() const noexcept { return storage.index() == 0; }
        constexpr explicit operator bool() const noexcept { return has_value(); }

        constexpr T& value() & noexcept { return get<0>(storage); }
        constexpr const T& value() const & noexcept { return get<0>(storage); }
        constexpr T&& value() && noexcept { return std::move(value()); }
        constexpr const T&& value() const && noexcept { return std::move(value()); }

        constexpr E& error() & noexcept { return get<1>(storage); }
        constexpr const E& error() const & noexcept { return get<1>(storage); }
        constexpr E&& error() && noexcept { return std::move(error()); }
        constexpr const E&& error() const && noexcept { return std::move(error()); }

    private:
        template<class... Args>
        constexpr Result(std::in_place_index_t<1>, Args&&... args)
            noexcept(std::is_nothrow_constructible_v<E, Args...>)
            : storage(std::in_place_index<1>, std::forward<Args>(args)...) {}

        template<std::size_t Index, class Variant>
        static constexpr auto& get(Variant& variant) noexcept {
            auto* item = std::get_if<Index>(&variant);
            if (!item) std::abort();
            return *item;
        }

        std::variant<T, E> storage;
    };

    template<class E>
    class [[nodiscard]] Result<void, E> {
    public:
        constexpr Result() noexcept = default;

        template<class... Args>
        static constexpr Result failure(Args&&... args)
            noexcept(std::is_nothrow_constructible_v<E, Args...>) {
            return Result(std::in_place_index<1>, std::forward<Args>(args)...);
        }

        constexpr bool has_value() const noexcept { return storage.index() == 0; }
        constexpr explicit operator bool() const noexcept { return has_value(); }

        constexpr void value() const noexcept {
            if (!has_value()) std::abort();
        }

        constexpr E& error() & noexcept { return getError(storage); }
        constexpr const E& error() const & noexcept { return getError(storage); }
        constexpr E&& error() && noexcept { return std::move(error()); }
        constexpr const E&& error() const && noexcept { return std::move(error()); }

    private:
        template<class... Args>
        constexpr Result(std::in_place_index_t<1>, Args&&... args)
            noexcept(std::is_nothrow_constructible_v<E, Args...>)
            : storage(std::in_place_index<1>, std::forward<Args>(args)...) {}

        template<class Variant>
        static constexpr auto& getError(Variant& variant) noexcept {
            auto* error = std::get_if<1>(&variant);
            if (!error) std::abort();
            return *error;
        }

        std::variant<std::monostate, E> storage;
    };
}

#endif
