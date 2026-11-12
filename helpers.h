#ifndef HELPERS_H
#define HELPERS_H

#include <cstring>
#include <limits>
#include <optional>
#include <stdexcept>
#include <type_traits>

#include <QtEndian>

/// Converts to the equivalent signed type.
template<class Signed, class Unsigned>
constexpr Signed to_signed(Unsigned u)
{
    static_assert(std::is_signed_v<Signed>);
    static_assert(std::is_unsigned_v<Unsigned>);

    if (u > static_cast<Unsigned>(std::numeric_limits<Signed>::max())) {
        throw std::overflow_error("unsigned value does not fit in signed type");
    }

    return static_cast<Signed>(u);
}

/// Converts to the equivalent signed type.
template<class Unsigned>
constexpr auto to_signed(Unsigned s)
{
    return to_signed<std::make_signed_t<Unsigned>, Unsigned>(s);
}

/// Converts to the equivalent unsigned type.
template<class Unsigned, class Signed>
constexpr Unsigned to_unsigned(Signed s)
{
    static_assert(std::is_unsigned_v<Unsigned>);
    static_assert(std::is_signed_v<Signed>);
    static_assert(sizeof(Signed) <= sizeof(Unsigned));

    if (s < 0) {
        throw std::domain_error("negative value cannot be converted to unsigned");
    }

    return static_cast<Unsigned>(s);
}

/// Converts to the equivalent unsigned type.
template<class Signed>
constexpr auto to_unsigned(Signed s)
{
    return to_unsigned<std::make_unsigned_t<Signed>, Signed>(s);
}

/// Checks whether a value can be represented in type To without loss.
template<class To, class From>
constexpr bool fits_in(From v) noexcept
{
    static_assert(std::is_integral_v<To>);
    static_assert(std::is_integral_v<From>);

    if constexpr (std::is_signed_v<From> && std::is_unsigned_v<To>) {
        using Common = std::make_unsigned_t<From>;
        return v >= 0 && static_cast<Common>(v) <= static_cast<Common>(std::numeric_limits<To>::max());
    } else if constexpr (std::is_unsigned_v<From> && std::is_signed_v<To>) {
        return v <= static_cast<From>(std::numeric_limits<To>::max());
    } else {
        // Same signedness: compare against To's range in From's type.
        return v >= static_cast<From>(std::numeric_limits<To>::lowest())
            && v <= static_cast<From>(std::numeric_limits<To>::max());
    }
}

/// Attempts to narrow a value to type To, returning std::nullopt if it does not fit.
template<class To, class From>
constexpr std::optional<To> try_narrow(From v) noexcept
{
    if (!fits_in<To>(v)) {
        return std::nullopt;
    }

    return static_cast<To>(v);
}

/// Narrows a value to type To, asserting in debug builds that no data is lost.
template<class To, class From>
constexpr To narrow(From v)
{
    if (!fits_in<To>(v)) {
        throw std::overflow_error("value cannot be represented in destination type");
    }

    return static_cast<To>(v);
}

/// Checks whether widening from type From to type To is lossless.
template<class To, class From>
constexpr bool is_widening_to(From v) noexcept
{
    static_assert(std::is_integral_v<To>);
    static_assert(std::is_integral_v<From>);

    if constexpr (std::is_signed_v<From> && std::is_unsigned_v<To>) {
        return v >= 0; // negatives cannot become unsigned
    } else {
        return true; // widening within/between compatible ranges
    }
}

/// Widens a value to type To, asserting that the conversion is lossless.
template<class To, class From>
constexpr To widen(From v)
{
    static_assert(
        sizeof(To) > sizeof(From) || (std::is_signed_v<From> && std::is_unsigned_v<To> && sizeof(To) >= sizeof(From))
    );

    if (!is_widening_to<To>(v)) {
        throw std::domain_error("negative value cannot be widened to unsigned");
    }

    return static_cast<To>(v);
}

/// Checked cast from one type to another, throwing std::overflow_error on loss.
template<class To, class From>
constexpr To checked_cast(From v)
{
    static_assert(std::is_integral_v<To>);
    static_assert(std::is_integral_v<From>);

    if (!fits_in<To>(v)) {
        throw std::overflow_error("integer value cannot be represented in destination type");
    }

    return static_cast<To>(v);
}

/// Reads a big-endian value of type T from an arbitrary memory address.
template<typename T>
T read_be(const void *p) noexcept
{
    T tmp;
    std::memcpy(&tmp, p, sizeof(tmp));
    return qFromBigEndian(tmp);
}

#endif // HELPERS_H
