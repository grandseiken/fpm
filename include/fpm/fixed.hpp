#ifndef FPM_FIXED_HPP
#define FPM_FIXED_HPP

#include <cassert>
#include <cmath>
#include <cstdint>
#include <functional>
#include <limits>
#include <type_traits>

namespace fpm
{
namespace detail
{
template <typename T>
constexpr T fast_signed_div_pow2(const T& x, unsigned int pow2) {
  return x >= 0 ? x >> pow2 : x == std::numeric_limits<T>::min() ? std::numeric_limits<T>::min() / (T{1} << pow2) : -(-x >> pow2);
}
template <typename T>
constexpr T fast_signed_mul_pow2(const T& x, unsigned int pow2) {
  return x >= 0 ? x << pow2 : -(-x << pow2);
}
template <typename T>
constexpr T last_bit(const T& x) {
  return x >= 0 ? x & 0x1 : x == std::numeric_limits<T>::min() ? 0 : -(-x & 0x1);
}
}

//! Fixed-point number type
//! \tparam BaseType         the base integer type used to store the fixed-point number. This can be a signed or unsigned type.
//! \tparam IntermediateType the integer type used to store intermediate results during calculations.
//! \tparam FractionBits     the number of bits of the BaseType used to store the fraction
//! \tparam EnableRounding   enable rounding of LSB for multiplication, division, and type conversion
template <typename BaseType, typename IntermediateType, unsigned int FractionBits, bool EnableRounding = true>
class fixed
{
    static_assert(std::is_integral<BaseType>::value, "BaseType must be an integral type");
    static_assert(FractionBits > 0, "FractionBits must be greater than zero");
    static_assert(FractionBits + 2 <= sizeof(BaseType) * 8, "BaseType must at least be able to contain entire fraction, with space for at least two integral bits");
    static_assert(sizeof(IntermediateType) > sizeof(BaseType), "IntermediateType must be larger than BaseType");
    static_assert(std::numeric_limits<IntermediateType>::is_signed == std::numeric_limits<BaseType>::is_signed, "IntermediateType must have same signedness as BaseType");
    static constexpr BaseType FRACTION_MULT = BaseType{1} << FractionBits;

    struct raw_construct_tag {};
    constexpr inline fixed(BaseType val, raw_construct_tag) noexcept : m_value(val) {}

public:
    inline fixed() noexcept = default;

    // Converts an integral number to the fixed-point type.
    // Like static_cast, this truncates bits that don't fit.
    template <typename T, typename std::enable_if<std::is_integral<T>::value>::type* = nullptr>
    constexpr inline explicit fixed(T val) noexcept
        : m_value(detail::fast_signed_mul_pow2(static_cast<BaseType>(val), FractionBits))
    {}

    // Converts an floating-point number to the fixed-point type.
    // Like static_cast, this truncates bits that don't fit.
    template <typename T, typename std::enable_if<std::is_floating_point<T>::value>::type* = nullptr>
    constexpr inline explicit fixed(T val) noexcept
        : m_value(static_cast<BaseType>((EnableRounding) ?
		       (val >= 0.0) ? (val * T{FRACTION_MULT} + T{0.5}) : (val * T{FRACTION_MULT} - T{0.5})
		      : (val * T{FRACTION_MULT})))
    {}

    // Constructs from another fixed-point type with possibly different underlying representation.
    // Like static_cast, this truncates bits that don't fit.
    template <typename B, typename I, unsigned int F, bool R>
    constexpr inline explicit fixed(fixed<B,I,F,R> val) noexcept
        : m_value(from_fixed_point<F>(val.raw_value()).raw_value())
    {}

    // Explicit conversion to a floating-point type
    template <typename T, typename std::enable_if<std::is_floating_point<T>::value>::type* = nullptr>
    constexpr inline explicit operator T() const noexcept
    {
        return static_cast<T>(m_value) / static_cast<T>(FRACTION_MULT);
    }

    // Explicit conversion to an integral type
    template <typename T, typename std::enable_if<std::is_integral<T>::value>::type* = nullptr>
    constexpr inline explicit operator T() const noexcept
    {
        return static_cast<T>(detail::fast_signed_div_pow2(m_value, FractionBits));
    }

    // Explicit conversion to bool
    constexpr inline explicit operator bool() const noexcept {
        return static_cast<bool>(m_value);
    }

    // Returns the raw underlying value of this type.
    // Do not use this unless you know what you're doing.
    constexpr inline BaseType raw_value() const noexcept
    {
        return m_value;
    }

    //! Constructs a fixed-point number from another fixed-point number.
    //! \tparam NumFractionBits the number of bits used by the fraction in \a value.
    //! \param value the integer fixed-point number
    template <unsigned int NumFractionBits, typename T, typename std::enable_if<(NumFractionBits > FractionBits)>::type* = nullptr>
    static constexpr inline fixed from_fixed_point(T value) noexcept
    {
	// To correctly round the last bit in the result, we need one more bit of information.
	// We do this by multiplying by two before dividing and adding the LSB to the real result.
	return (EnableRounding) ? fixed(static_cast<BaseType>(
            detail::fast_signed_div_pow2(value, NumFractionBits - FractionBits) +
            detail::last_bit(detail::fast_signed_div_pow2(value, NumFractionBits - FractionBits - 1))),
	    raw_construct_tag{}) : fixed(static_cast<BaseType>(
            detail::fast_signed_div_pow2(value, NumFractionBits - FractionBits)),
	     raw_construct_tag{});
    }

    template <unsigned int NumFractionBits, typename T, typename std::enable_if<(NumFractionBits <= FractionBits)>::type* = nullptr>
    static constexpr inline fixed from_fixed_point(T value) noexcept
    {
        return fixed(static_cast<BaseType>(detail::fast_signed_mul_pow2(value, FractionBits - NumFractionBits)),
            raw_construct_tag{});
    }

    // Constructs a fixed-point number from its raw underlying value.
    // Do not use this unless you know what you're doing.
    static constexpr inline fixed from_raw_value(BaseType value) noexcept
    {
        return fixed(value, raw_construct_tag{});
    }

    //
    // Constants
    //
    static constexpr fixed e() { return from_fixed_point<61>(6267931151224907085ll); }
    static constexpr fixed pi() { return from_fixed_point<61>(7244019458077122842ll); }
    static constexpr fixed half_pi() { return from_fixed_point<62>(7244019458077122842ll); }
    static constexpr fixed two_pi() { return from_fixed_point<60>(7244019458077122842ll); }

    //
    // Arithmetic member operators
    //

    constexpr inline fixed operator-() const noexcept
    {
        return fixed::from_raw_value(-m_value);
    }

    constexpr inline fixed& operator+=(const fixed& y) noexcept
    {
        m_value += y.m_value;
        return *this;
    }

    template <typename I, typename std::enable_if<std::is_integral<I>::value>::type* = nullptr>
    constexpr inline fixed& operator+=(I y) noexcept
    {
        m_value += detail::fast_signed_mul_pow2(static_cast<BaseType>(y), FractionBits);
        return *this;
    }

    constexpr inline fixed& operator-=(const fixed& y) noexcept
    {
        m_value -= y.m_value;
        return *this;
    }

    template <typename I, typename std::enable_if<std::is_integral<I>::value>::type* = nullptr>
    constexpr inline fixed& operator-=(I y) noexcept
    {
        m_value -= detail::fast_signed_mul_pow2(static_cast<BaseType>(y), FractionBits);
        return *this;
    }

    constexpr inline fixed& operator*=(const fixed& y) noexcept
    {
	if (EnableRounding){
	    // Normal fixed-point multiplication is: x * y / 2**FractionBits.
	    // To correctly round the last bit in the result, we need one more bit of information.
	    // We do this by multiplying by two before dividing and adding the LSB to the real result.
	    auto value = detail::fast_signed_div_pow2(static_cast<IntermediateType>(m_value) * y.m_value, FractionBits - 1);
	    m_value = static_cast<BaseType>(detail::fast_signed_div_pow2(value, 1) + detail::last_bit(value));
	} else {
	    auto value = detail::fast_signed_div_pow2(static_cast<IntermediateType>(m_value) * y.m_value, FractionBits);
	    m_value = static_cast<BaseType>(value);
	}
	return *this;
    }

    template <typename I, typename std::enable_if<std::is_integral<I>::value>::type* = nullptr>
    constexpr inline fixed& operator*=(I y) noexcept
    {
        m_value *= y;
        return *this;
    }

    constexpr inline fixed& operator/=(const fixed& y) noexcept
    {
        assert(y.m_value != 0);
	if (EnableRounding){
	    // Normal fixed-point division is: x * 2**FractionBits / y.
	    // To correctly round the last bit in the result, we need one more bit of information.
	    // We do this by multiplying by two before dividing and adding the LSB to the real result.
	    auto value = detail::fast_signed_mul_pow2(static_cast<IntermediateType>(m_value), FractionBits + 1) / y.m_value;
	    m_value = static_cast<BaseType>(detail::fast_signed_div_pow2(value, 1) + detail::last_bit(value));
	} else {
	    auto value = detail::fast_signed_mul_pow2(static_cast<IntermediateType>(m_value), FractionBits) / y.m_value;
	    m_value = static_cast<BaseType>(value);
	}
        return *this;
    }

    template <typename I, typename std::enable_if<std::is_integral<I>::value>::type* = nullptr>
    constexpr inline fixed& operator/=(I y) noexcept
    {
        m_value /= y;
        return *this;
    }

    constexpr inline fixed& operator%=(const fixed& y) noexcept
    {
        m_value %= y.m_value;
        return *this;
    }

    template <typename I, typename std::enable_if<std::is_integral<I>::value>::type* = nullptr>
    constexpr inline fixed& operator%=(I y) noexcept
    {
        m_value %= detail::fast_signed_mul_pow2(static_cast<BaseType>(y), FractionBits);
        return *this;
    }

    template <typename I, typename std::enable_if<std::is_integral<I>::value>::type* = nullptr>
    constexpr inline fixed& operator>>=(I y) noexcept
    {
        m_value >>= y;
        return *this;
    }

    template <typename I, typename std::enable_if<std::is_integral<I>::value>::type* = nullptr>
    constexpr inline fixed& operator<<=(I y) noexcept
    {
        m_value <<= y;
        return *this;
    }

private:
    BaseType m_value;
};

//
// Convenience typedefs
//

using fixed_16_16 = fixed<std::int32_t, std::int64_t, 16>;
using fixed_24_8 = fixed<std::int32_t, std::int64_t, 8>;
using fixed_8_24 = fixed<std::int32_t, std::int64_t, 24>;

//
// Addition
//

template <typename B, typename I, unsigned int F, bool R>
constexpr inline fixed<B, I, F, R> operator+(const fixed<B, I, F, R>& x, const fixed<B, I, F, R>& y) noexcept
{
    return fixed<B, I, F, R>(x) += y;
}

template <typename B, typename I, unsigned int F, bool R, typename T, typename std::enable_if<std::is_integral<T>::value>::type* = nullptr>
constexpr inline fixed<B, I, F, R> operator+(const fixed<B, I, F, R>& x, T y) noexcept
{
    return fixed<B, I, F, R>(x) += y;
}

template <typename B, typename I, unsigned int F, bool R, typename T, typename std::enable_if<std::is_integral<T>::value>::type* = nullptr>
constexpr inline fixed<B, I, F, R> operator+(T x, const fixed<B, I, F, R>& y) noexcept
{
    return fixed<B, I, F, R>(y) += x;
}

//
// Subtraction
//

template <typename B, typename I, unsigned int F, bool R>
constexpr inline fixed<B, I, F, R> operator-(const fixed<B, I, F, R>& x, const fixed<B, I, F, R>& y) noexcept
{
    return fixed<B, I, F, R>(x) -= y;
}

template <typename B, typename I, unsigned int F, bool R, typename T, typename std::enable_if<std::is_integral<T>::value>::type* = nullptr>
constexpr inline fixed<B, I, F, R> operator-(const fixed<B, I, F, R>& x, T y) noexcept
{
    return fixed<B, I, F, R>(x) -= y;
}

template <typename B, typename I, unsigned int F, bool R, typename T, typename std::enable_if<std::is_integral<T>::value>::type* = nullptr>
constexpr inline fixed<B, I, F, R> operator-(T x, const fixed<B, I, F, R>& y) noexcept
{
    return fixed<B, I, F, R>(x) -= y;
}

//
// Multiplication
//

template <typename B, typename I, unsigned int F, bool R>
constexpr inline fixed<B, I, F, R> operator*(const fixed<B, I, F, R>& x, const fixed<B, I, F, R>& y) noexcept
{
    return fixed<B, I, F, R>(x) *= y;
}

template <typename B, typename I, unsigned int F, bool R, typename T, typename std::enable_if<std::is_integral<T>::value>::type* = nullptr>
constexpr inline fixed<B, I, F, R> operator*(const fixed<B, I, F, R>& x, T y) noexcept
{
    return fixed<B, I, F, R>(x) *= y;
}

template <typename B, typename I, unsigned int F, bool R, typename T, typename std::enable_if<std::is_integral<T>::value>::type* = nullptr>
constexpr inline fixed<B, I, F, R> operator*(T x, const fixed<B, I, F, R>& y) noexcept
{
    return fixed<B, I, F, R>(y) *= x;
}

//
// Division
//

template <typename B, typename I, unsigned int F, bool R>
constexpr inline fixed<B, I, F, R> operator/(const fixed<B, I, F, R>& x, const fixed<B, I, F, R>& y) noexcept
{
    return fixed<B, I, F, R>(x) /= y;
}

template <typename B, typename I, unsigned int F, typename T, bool R, typename std::enable_if<std::is_integral<T>::value>::type* = nullptr>
constexpr inline fixed<B, I, F, R> operator/(const fixed<B, I, F, R>& x, T y) noexcept
{
    return fixed<B, I, F, R>(x) /= y;
}

template <typename B, typename I, unsigned int F, typename T, bool R, typename std::enable_if<std::is_integral<T>::value>::type* = nullptr>
constexpr inline fixed<B, I, F, R> operator/(T x, const fixed<B, I, F, R>& y) noexcept
{
    return fixed<B, I, F, R>(x) /= y;
}

//
// Modulo
//

template <typename B, typename I, unsigned int F, bool R>
constexpr inline fixed<B, I, F, R> operator%(const fixed<B, I, F, R>& x, const fixed<B, I, F, R>& y) noexcept
{
    return fixed<B, I, F, R>(x) %= y;
}

template <typename B, typename I, unsigned int F, typename T, bool R, typename std::enable_if<std::is_integral<T>::value>::type* = nullptr>
constexpr inline fixed<B, I, F, R> operator%(const fixed<B, I, F, R>& x, T y) noexcept
{
    return fixed<B, I, F, R>(x) %= y;
}

template <typename B, typename I, unsigned int F, typename T, bool R, typename std::enable_if<std::is_integral<T>::value>::type* = nullptr>
constexpr inline fixed<B, I, F, R> operator%(T x, const fixed<B, I, F, R>& y) noexcept
{
    return fixed<B, I, F, R>(x) %= y;
}

//
// Bit-shift
//

template <typename B, typename I, unsigned int F, typename T, bool R, typename std::enable_if<std::is_integral<T>::value>::type* = nullptr>
constexpr inline fixed<B, I, F, R> operator>>(const fixed<B, I, F, R>& x, T y) noexcept
{
    return fixed<B, I, F, R>(x) >>= y;
}

template <typename B, typename I, unsigned int F, typename T, bool R, typename std::enable_if<std::is_integral<T>::value>::type* = nullptr>
constexpr inline fixed<B, I, F, R> operator<<(const fixed<B, I, F, R>& x, T y) noexcept
{
    return fixed<B, I, F, R>(x) <<= y;
}

//
// Comparison operators
//

template <typename B, typename I, unsigned int F, bool R>
constexpr inline bool operator==(const fixed<B, I, F, R>& x, const fixed<B, I, F, R>& y) noexcept
{
    return x.raw_value() == y.raw_value();
}

template <typename B, typename I, unsigned int F, bool R>
constexpr inline bool operator!=(const fixed<B, I, F, R>& x, const fixed<B, I, F, R>& y) noexcept
{
    return x.raw_value() != y.raw_value();
}

template <typename B, typename I, unsigned int F, bool R>
constexpr inline bool operator<(const fixed<B, I, F, R>& x, const fixed<B, I, F, R>& y) noexcept
{
    return x.raw_value() < y.raw_value();
}

template <typename B, typename I, unsigned int F, bool R>
constexpr inline bool operator>(const fixed<B, I, F, R>& x, const fixed<B, I, F, R>& y) noexcept
{
    return x.raw_value() > y.raw_value();
}

template <typename B, typename I, unsigned int F, bool R>
constexpr inline bool operator<=(const fixed<B, I, F, R>& x, const fixed<B, I, F, R>& y) noexcept
{
    return x.raw_value() <= y.raw_value();
}

template <typename B, typename I, unsigned int F, bool R>
constexpr inline bool operator>=(const fixed<B, I, F, R>& x, const fixed<B, I, F, R>& y) noexcept
{
    return x.raw_value() >= y.raw_value();
}

#if __cplusplus > 201703L
template <std::three_way_comparable B, typename I, unsigned int F, bool R>
constexpr inline auto operator<=>(const fixed<B, I, F, R>& x, const fixed<B, I, F, R>& y) noexcept
{
    return x.raw_value() <=> y.raw_value();
}
#endif

namespace detail
{
// Number of base-10 digits required to fully represent a number of bits
static constexpr int max_digits10(int bits)
{
    // 8.24 fixed-point equivalent of (int)ceil(bits * std::log10(2));
    using T = long long;
    return static_cast<int>((T{bits} * 5050445 + (T{1} << 24) - 1) >> 24);
}

// Number of base-10 digits that can be fully represented by a number of bits
static constexpr int digits10(int bits)
{
    // 8.24 fixed-point equivalent of (int)(bits * std::log10(2));
    using T = long long;
    return static_cast<int>((T{bits} * 5050445) >> 24);
}

} // namespace detail
} // namespace fpm

// Specializations for customization points
namespace std
{

template <typename B, typename I, unsigned int F, bool R>
struct hash<fpm::fixed<B,I,F,R>>
{
    using argument_type = fpm::fixed<B, I, F, R>;
    using result_type = std::size_t;

    result_type operator()(argument_type arg) const noexcept(noexcept(std::declval<std::hash<B>>()(arg.raw_value()))) {
        return m_hash(arg.raw_value());
    }

private:
    std::hash<B> m_hash;
};

template <typename B, typename I, unsigned int F, bool R>
struct numeric_limits<fpm::fixed<B,I,F,R>>
{
    static constexpr bool is_specialized = true;
    static constexpr bool is_signed = std::numeric_limits<B>::is_signed;
    static constexpr bool is_integer = false;
    static constexpr bool is_exact = true;
    static constexpr bool has_infinity = false;
    static constexpr bool has_quiet_NaN = false;
    static constexpr bool has_signaling_NaN = false;
    static constexpr std::float_denorm_style has_denorm = std::denorm_absent;
    static constexpr bool has_denorm_loss = false;
    static constexpr std::float_round_style round_style = std::round_to_nearest;
    static constexpr bool is_iec559 = false;
    static constexpr bool is_bounded = true;
    static constexpr bool is_modulo = std::numeric_limits<B>::is_modulo;
    static constexpr int digits = std::numeric_limits<B>::digits;

    // Any number with `digits10` significant base-10 digits (that fits in
    // the range of the type) is guaranteed to be convertible from text and
    // back without change. Worst case, this is 0.000...001, so we can only
    // guarantee this case. Nothing more.
    static constexpr int digits10 = 1;

    // This is equal to max_digits10 for the integer and fractional part together.
    static constexpr int max_digits10 =
        fpm::detail::max_digits10(std::numeric_limits<B>::digits - F) + fpm::detail::max_digits10(F);

    static constexpr int radix = 2;
    static constexpr int min_exponent = 1 - F;
    static constexpr int min_exponent10 = -fpm::detail::digits10(F);
    static constexpr int max_exponent = std::numeric_limits<B>::digits - F;
    static constexpr int max_exponent10 = fpm::detail::digits10(std::numeric_limits<B>::digits - F);
    static constexpr bool traps = true;
    static constexpr bool tinyness_before = false;

    static constexpr fpm::fixed<B,I,F,R> lowest() noexcept {
        return fpm::fixed<B,I,F,R>::from_raw_value(std::numeric_limits<B>::lowest());
    };

    static constexpr fpm::fixed<B,I,F,R> min() noexcept {
        return lowest();
    }

    static constexpr fpm::fixed<B,I,F,R> max() noexcept {
        return fpm::fixed<B,I,F,R>::from_raw_value(std::numeric_limits<B>::max());
    };

    static constexpr fpm::fixed<B,I,F,R> epsilon() noexcept {
        return fpm::fixed<B,I,F,R>::from_raw_value(1);
    };

    static constexpr fpm::fixed<B,I,F,R> round_error() noexcept {
        return fpm::fixed<B,I,F,R>(1) / 2;
    };

    static constexpr fpm::fixed<B,I,F,R> denorm_min() noexcept {
        return min();
    }
};

template <typename B, typename I, unsigned int F, bool R>
constexpr bool numeric_limits<fpm::fixed<B,I,F,R>>::is_specialized;
template <typename B, typename I, unsigned int F, bool R>
constexpr bool numeric_limits<fpm::fixed<B,I,F,R>>::is_signed;
template <typename B, typename I, unsigned int F, bool R>
constexpr bool numeric_limits<fpm::fixed<B,I,F,R>>::is_integer;
template <typename B, typename I, unsigned int F, bool R>
constexpr bool numeric_limits<fpm::fixed<B,I,F,R>>::is_exact;
template <typename B, typename I, unsigned int F, bool R>
constexpr bool numeric_limits<fpm::fixed<B,I,F,R>>::has_infinity;
template <typename B, typename I, unsigned int F, bool R>
constexpr bool numeric_limits<fpm::fixed<B,I,F,R>>::has_quiet_NaN;
template <typename B, typename I, unsigned int F, bool R>
constexpr bool numeric_limits<fpm::fixed<B,I,F,R>>::has_signaling_NaN;
template <typename B, typename I, unsigned int F, bool R>
constexpr std::float_denorm_style numeric_limits<fpm::fixed<B,I,F,R>>::has_denorm;
template <typename B, typename I, unsigned int F, bool R>
constexpr bool numeric_limits<fpm::fixed<B,I,F,R>>::has_denorm_loss;
template <typename B, typename I, unsigned int F, bool R>
constexpr std::float_round_style numeric_limits<fpm::fixed<B,I,F,R>>::round_style;
template <typename B, typename I, unsigned int F, bool R>
constexpr bool numeric_limits<fpm::fixed<B,I,F,R>>::is_iec559;
template <typename B, typename I, unsigned int F, bool R>
constexpr bool numeric_limits<fpm::fixed<B,I,F,R>>::is_bounded;
template <typename B, typename I, unsigned int F, bool R>
constexpr bool numeric_limits<fpm::fixed<B,I,F,R>>::is_modulo;
template <typename B, typename I, unsigned int F, bool R>
constexpr int numeric_limits<fpm::fixed<B,I,F,R>>::digits;
template <typename B, typename I, unsigned int F, bool R>
constexpr int numeric_limits<fpm::fixed<B,I,F,R>>::digits10;
template <typename B, typename I, unsigned int F, bool R>
constexpr int numeric_limits<fpm::fixed<B,I,F,R>>::max_digits10;
template <typename B, typename I, unsigned int F, bool R>
constexpr int numeric_limits<fpm::fixed<B,I,F,R>>::radix;
template <typename B, typename I, unsigned int F, bool R>
constexpr int numeric_limits<fpm::fixed<B,I,F,R>>::min_exponent;
template <typename B, typename I, unsigned int F, bool R>
constexpr int numeric_limits<fpm::fixed<B,I,F,R>>::min_exponent10;
template <typename B, typename I, unsigned int F, bool R>
constexpr int numeric_limits<fpm::fixed<B,I,F,R>>::max_exponent;
template <typename B, typename I, unsigned int F, bool R>
constexpr int numeric_limits<fpm::fixed<B,I,F,R>>::max_exponent10;
template <typename B, typename I, unsigned int F, bool R>
constexpr bool numeric_limits<fpm::fixed<B,I,F,R>>::traps;
template <typename B, typename I, unsigned int F, bool R>
constexpr bool numeric_limits<fpm::fixed<B,I,F,R>>::tinyness_before;

}

#endif
