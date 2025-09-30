#pragma once

#include <cstdint>
#include <cstddef>
#include <memory>
#include <string>
#include <vector>
#include <array>
#include <span>
#include <expected>
#include <optional>

namespace lore::core {

// ============================================================================
// Fundamental Type Aliases
// ============================================================================

// Unsigned integer types
using u8  = std::uint8_t;
using u16 = std::uint16_t;
using u32 = std::uint32_t;
using u64 = std::uint64_t;

// Signed integer types
using i8  = std::int8_t;
using i16 = std::int16_t;
using i32 = std::int32_t;
using i64 = std::int64_t;

// Floating-point types
using f32 = float;
using f64 = double;

// Size and pointer types
using usize = std::size_t;
using isize = std::ptrdiff_t;
using uptr  = std::uintptr_t;
using iptr  = std::intptr_t;

// Byte type
using byte = std::byte;

// ============================================================================
// Smart Pointer Aliases
// ============================================================================

template<typename T>
using UniquePtr = std::unique_ptr<T>;

template<typename T>
using SharedPtr = std::shared_ptr<T>;

template<typename T>
using WeakPtr = std::weak_ptr<T>;

// ============================================================================
// Container Aliases
// ============================================================================

template<typename T>
using Vector = std::vector<T>;

template<typename T, std::size_t N>
using Array = std::array<T, N>;

template<typename T>
using Span = std::span<T>;

using String = std::string;
using StringView = std::string_view;

// ============================================================================
// Result and Optional Types
// ============================================================================

template<typename T, typename E>
using Result = std::expected<T, E>;

template<typename T>
using Option = std::optional<T>;

// ============================================================================
// Error Types
// ============================================================================

/**
 * Common error codes used throughout the Lore Engine
 */
enum class ErrorCode : u32 {
    Success = 0,
    
    // Generic errors (1-99)
    Unknown = 1,
    InvalidArgument = 2,
    OutOfMemory = 3,
    NotImplemented = 4,
    NotSupported = 5,
    Timeout = 6,
    Cancelled = 7,
    
    // File I/O errors (100-199)
    FileNotFound = 100,
    FileAccessDenied = 101,
    FileAlreadyExists = 102,
    FileReadError = 103,
    FileWriteError = 104,
    DirectoryNotFound = 105,
    PathTooLong = 106,
    
    // Graphics errors (200-299)
    GraphicsInitFailed = 200,
    ShaderCompileFailed = 201,
    TextureLoadFailed = 202,
    BufferCreationFailed = 203,
    PipelineCreationFailed = 204,
    SwapchainCreationFailed = 205,
    CommandBufferFailed = 206,
    SynchronizationFailed = 207,
    
    // Audio errors (300-399)
    AudioInitFailed = 300,
    AudioDeviceNotFound = 301,
    AudioFormatNotSupported = 302,
    AudioStreamFailed = 303,
    
    // Asset errors (400-499)
    AssetNotFound = 400,
    AssetLoadFailed = 401,
    AssetInvalidFormat = 402,
    AssetCorrupted = 403,
    
    // Network errors (500-599)
    NetworkError = 500,
    ConnectionFailed = 501,
    ConnectionLost = 502,
    InvalidResponse = 503,
    
    // System errors (600-699)
    ThreadCreationFailed = 600,
    MutexLockFailed = 601,
    ConditionVariableFailed = 602,
};

/**
 * Error information structure
 */
struct Error {
    ErrorCode code = ErrorCode::Success;
    String message;
    String source_location; // File:Line where error occurred
    
    Error() = default;
    
    explicit Error(ErrorCode code_, String message_ = "", String location_ = "")
        : code(code_), message(std::move(message_)), source_location(std::move(location_)) {}
    
    bool is_success() const { return code == ErrorCode::Success; }
    bool is_error() const { return code != ErrorCode::Success; }
    
    String to_string() const {
        if (is_success()) return "Success";
        String result = "Error " + std::to_string(static_cast<u32>(code));
        if (!message.empty()) result += ": " + message;
        if (!source_location.empty()) result += " at " + source_location;
        return result;
    }
};

// ============================================================================
// Handle Types
// ============================================================================

/**
 * Type-safe handle wrapper for resource management
 */
template<typename Tag, typename ValueType = u64>
struct Handle {
    ValueType value = 0;
    
    constexpr Handle() = default;
    constexpr explicit Handle(ValueType v) : value(v) {}
    
    constexpr bool is_valid() const { return value != 0; }
    constexpr explicit operator bool() const { return is_valid(); }
    
    constexpr bool operator==(const Handle& other) const = default;
    constexpr auto operator<=>(const Handle& other) const = default;
};

// Define hash for handles to use in unordered containers
template<typename Tag, typename ValueType>
struct std::hash<lore::core::Handle<Tag, ValueType>> {
    std::size_t operator()(const lore::core::Handle<Tag, ValueType>& h) const noexcept {
        return std::hash<ValueType>{}(h.value);
    }
};

// ============================================================================
// Utility Macros
// ============================================================================

// Create type-safe handle types
#define LORE_DEFINE_HANDLE(name) \
    struct name##Tag {}; \
    using name = ::lore::core::Handle<name##Tag>

// Try-return pattern for Result types
#define LORE_TRY(expr) \
    ({ \
        auto _result = (expr); \
        if (!_result) return std::unexpected(_result.error()); \
        std::move(*_result); \
    })

// ============================================================================
// Constants
// ============================================================================

// Invalid/null handle sentinel
inline constexpr u64 INVALID_HANDLE = 0;

// Maximum values
inline constexpr u8  MAX_U8  = UINT8_MAX;
inline constexpr u16 MAX_U16 = UINT16_MAX;
inline constexpr u32 MAX_U32 = UINT32_MAX;
inline constexpr u64 MAX_U64 = UINT64_MAX;

inline constexpr i8  MAX_I8  = INT8_MAX;
inline constexpr i16 MAX_I16 = INT16_MAX;
inline constexpr i32 MAX_I32 = INT32_MAX;
inline constexpr i64 MAX_I64 = INT64_MAX;

inline constexpr i8  MIN_I8  = INT8_MIN;
inline constexpr i16 MIN_I16 = INT16_MIN;
inline constexpr i32 MIN_I32 = INT32_MIN;
inline constexpr i64 MIN_I64 = INT64_MIN;

} // namespace lore::core

// Bring common types into global lore namespace for convenience
namespace lore {
    using lore::core::u8;
    using lore::core::u16;
    using lore::core::u32;
    using lore::core::u64;
    using lore::core::i8;
    using lore::core::i16;
    using lore::core::i32;
    using lore::core::i64;
    using lore::core::f32;
    using lore::core::f64;
    using lore::core::usize;
    using lore::core::isize;
    using lore::core::byte;
    using lore::core::Result;
    using lore::core::Option;
    using lore::core::Error;
    using lore::core::ErrorCode;
} // namespace lore
