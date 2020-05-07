﻿#pragma once

#include "types.h"
#include "util/endian.hpp"
#include <cstring>
#include <cmath>

#if __has_include(<bit>)
#include <bit>
#else
#include <type_traits>
#endif

#if defined(_MSC_VER)
#define FMA_FUNC
#else
#define FMA_FUNC __attribute__((__target__("fma")))
#endif

// 128-bit vector type and also se_storage<> storage type
union alignas(16) v128
{
private:
	static const bool has_fma3;

public:
	char _bytes[16];

	template <typename T, std::size_t M>
	class masked_array_t // array type accessed as (index ^ M)
	{
		char m_data[16];

	public:
		T& operator[](std::size_t index)
		{
			return reinterpret_cast<T*>(m_data)[index ^ M];
		}

		const T& operator[](std::size_t index) const
		{
			return reinterpret_cast<const T*>(m_data)[index ^ M];
		}
	};

	template <typename T>
	using normal_array_t = masked_array_t<T, std::endian::little == std::endian::native ? 0 : 16 / sizeof(T) - 1>;
	template <typename T>
	using reversed_array_t = masked_array_t<T, std::endian::little == std::endian::native ? 16 / sizeof(T) - 1 : 0>;

	normal_array_t<u64> _u64;
	normal_array_t<s64> _s64;
	reversed_array_t<u64> u64r;
	reversed_array_t<s64> s64r;

	normal_array_t<u32> _u32;
	normal_array_t<s32> _s32;
	reversed_array_t<u32> u32r;
	reversed_array_t<s32> s32r;

	normal_array_t<u16> _u16;
	normal_array_t<s16> _s16;
	reversed_array_t<u16> u16r;
	reversed_array_t<s16> s16r;

	normal_array_t<u8> _u8;
	normal_array_t<s8> _s8;
	reversed_array_t<u8> u8r;
	reversed_array_t<s8> s8r;

	normal_array_t<f32> _f;
	normal_array_t<f64> _d;
	reversed_array_t<f32> fr;
	reversed_array_t<f64> dr;

	struct bit_array_128
	{
		char m_data[16];

	public:
		class bit_element
		{
			u32& data;
			const u32 mask;

		public:
			bit_element(u32& data, const u32 mask)
				: data(data)
				, mask(mask)
			{
			}

			operator bool() const
			{
				return (data & mask) != 0;
			}

			bit_element& operator=(const bool right)
			{
				if (right)
				{
					data |= mask;
				}
				else
				{
					data &= ~mask;
				}
				return *this;
			}

			bit_element& operator=(const bit_element& right)
			{
				if (right)
				{
					data |= mask;
				}
				else
				{
					data &= ~mask;
				}
				return *this;
			}
		};

		// Index 0 returns the MSB and index 127 returns the LSB
		bit_element operator[](u32 index)
		{
			const auto data_ptr = reinterpret_cast<u32*>(m_data);

			if constexpr (std::endian::little == std::endian::native)
			{
				return bit_element(data_ptr[3 - (index >> 5)], 0x80000000u >> (index & 0x1F));
			}
			else
			{
				return bit_element(data_ptr[index >> 5], 0x80000000u >> (index & 0x1F));
			}
		}

		// Index 0 returns the MSB and index 127 returns the LSB
		bool operator[](u32 index) const
		{
			const auto data_ptr = reinterpret_cast<const u32*>(m_data);

			if constexpr (std::endian::little == std::endian::native)
			{
				return (data_ptr[3 - (index >> 5)] & (0x80000000u >> (index & 0x1F))) != 0;
			}
			else
			{
				return (data_ptr[index >> 5] & (0x80000000u >> (index & 0x1F))) != 0;
			}
		}
	} _bit;

	v128() = default;

	template <typename T, typename type = std::remove_const_t<T>>
	static constexpr bool is_v128_convertible = 
		std::is_same_v<type, s128> ||
		std::is_same_v<type, u128> ||
		std::is_same_v<type, __m128> ||
		std::is_same_v<type, __m128i> ||
		std::is_same_v<type, __m128d>;

	template <typename T, typename = std::enable_if_t<is_v128_convertible<std::remove_reference_t<T>>, void>>
	v128(T&& value) noexcept
	{
		*this = std::bit_cast<v128>(std::forward<T>(value));
	}

	template <typename T, typename = std::enable_if_t<is_v128_convertible<T>, void>>
	operator T() const noexcept
	{
		return std::bit_cast<T>(*this);
	}

	static v128 from64(u64 _0, u64 _1 = 0)
	{
		auto to_s64 = [](u64 val) { return static_cast<s64>(val); };
		return _mm_set_epi64x(to_s64(_1), to_s64(_0));
	}

	static v128 from64r(u64 _1, u64 _0 = 0)
	{
		return from64(_0, _1);
	}

	static v128 from32(u32 _0, u32 _1 = 0, u32 _2 = 0, u32 _3 = 0)
	{
		auto to_int = [](u32 val) { return static_cast<int>(val); };
		return _mm_set_epi32(to_int(_3), to_int(_2), to_int(_1), to_int(_0));
	}

	static v128 from32r(u32 _3, u32 _2 = 0, u32 _1 = 0, u32 _0 = 0)
	{
		return from32(_0, _1, _2, _3);
	}

	static v128 from32p(u32 value)
	{
		return _mm_set1_epi32(static_cast<s32>(value));
	}

	static v128 from16p(u16 value)
	{
		return _mm_set1_epi16(static_cast<s16>(value));
	}

	static v128 from8p(u8 value)
	{
		return _mm_set1_epi8(static_cast<s8>(value));
	}

	static v128 fromBit(u32 bit)
	{
		v128 ret{};
		ret._bit[bit] = true;
		return ret;
	}

	// Unaligned load with optional index offset
	static v128 loadu(const void* ptr, std::size_t index = 0)
	{
		v128 ret;
		std::memcpy(&ret, static_cast<const u8*>(ptr) + index * sizeof(v128), sizeof(v128));
		return ret;
	}

	// Unaligned store with optional index offset
	static void storeu(v128 value, void* ptr, std::size_t index = 0)
	{
		std::memcpy(static_cast<u8*>(ptr) + index * sizeof(v128), &value, sizeof(v128));
	}

	static inline v128 add8(v128 left, v128 right)
	{
		return _mm_add_epi8(left, right);
	}

	static inline v128 add16(v128 left, v128 right)
	{
		return _mm_add_epi16(left, right);
	}

	static inline v128 add32(v128 left, v128 right)
	{
		return _mm_add_epi32(left, right);
	}

	static inline v128 addfs(v128 left, v128 right)
	{
		return _mm_add_ps(left, right);
	}

	static inline v128 addfd(v128 left, v128 right)
	{
		return _mm_add_pd(left, right);
	}

	static inline v128 sub8(v128 left, v128 right)
	{
		return _mm_sub_epi8(left, right);
	}

	static inline v128 sub16(v128 left, v128 right)
	{
		return _mm_sub_epi16(left, right);
	}

	static inline v128 sub32(v128 left, v128 right)
	{
		return _mm_sub_epi32(left, right);
	}

	static inline v128 subfs(v128 left, v128 right)
	{
		return _mm_sub_ps(left, right);
	}

	static inline v128 subfd(v128 left, v128 right)
	{
		return _mm_sub_pd(left, right);
	}

	static inline v128 maxu8(v128 left, v128 right)
	{
		return _mm_max_epu8(left, right);
	}

	static inline v128 minu8(v128 left, v128 right)
	{
		return _mm_min_epu8(left, right);
	}

	static inline v128 eq8(v128 left, v128 right)
	{
		return _mm_cmpeq_epi8(left, right);
	}

	static inline v128 eq16(v128 left, v128 right)
	{
		return _mm_cmpeq_epi16(left, right);
	}

	static inline v128 eq32(v128 left, v128 right)
	{
		return _mm_cmpeq_epi32(left, right);
	}

	bool operator==(v128 right) const
	{
		return _u64[0] == right._u64[0] && _u64[1] == right._u64[1];
	}

	bool operator!=(v128 right) const
	{
		return _u64[0] != right._u64[0] || _u64[1] != right._u64[1];
	}

	// result = (~left) & (right)
	static inline v128 andnot(v128 left, v128 right)
	{
		return _mm_andnot_si128(left, right);
	}

	// result = fma(a, b, c)
	static FMA_FUNC inline v128 fma32(v128 a, v128 b, v128 c)
	{
		if (has_fma3) [[likely]]
		{
			return _mm_fmadd_ps(a, b, c);
		}

		// Extend to 4 64-bit doubles in 2 vectors, multiply+add then truncate to 32-bit floats vector
 
 		const auto a1 = _mm_cvtps_pd(a);
		const auto b1 = _mm_cvtps_pd(b);
		const auto c1 = _mm_cvtps_pd(c);
		const auto res1 = _mm_cvtpd_ps(_mm_add_pd(_mm_mul_pd(a1, b1), c1));

#ifndef _MSC_VER
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wold-style-cast"
#endif
		const auto a2 = _mm_cvtps_pd(_mm_shuffle_ps(a, a, 0xe));
		const auto b2 = _mm_cvtps_pd(_mm_shuffle_ps(b, b, 0xe));
		const auto c2 = _mm_cvtps_pd(_mm_shuffle_ps(c, c, 0xe));
		const auto mulres = _mm_cvtpd_ps(_mm_add_pd(_mm_mul_pd(a2, b2), c2));
		const auto res2 = _mm_shuffle_ps(mulres, mulres, 0x4f);
#ifndef _MSC_VER
#pragma GCC diagnostic pop
#endif

		return _mm_or_ps(res1, res2);
	}

	// result = fma(a, b, c)
	static FMA_FUNC inline v128 fma64(v128 a, v128 b, v128 c)
	{
		if (has_fma3) [[likely]]
		{
			return _mm_fmadd_pd(a, b, c);
		}

		auto do_fma = [&](std::size_t i) { return std::bit_cast<u64>(std::fma(a._d[i], b._d[i], c._d[i])); };
		return from64(do_fma(0), do_fma(1));
	}

	void clear()
	{
		*this = {};
	}
};

template <typename T, std::size_t M>
struct offset32_array<v128::masked_array_t<T, M>>
{
	template <typename Arg>
	static inline u32 index32(const Arg& arg)
	{
		return u32{sizeof(T)} * (static_cast<u32>(arg) ^ static_cast<u32>(M));
	}
};

inline v128 operator|(v128 left, v128 right)
{
	return _mm_or_si128(left, right);
}

inline v128 operator&(v128 left, v128 right)
{
	return _mm_and_si128(left, right);
}

inline v128 operator^(v128 left, v128 right)
{
	return _mm_xor_si128(left, right);
}

inline v128 operator~(v128 other)
{
	return other ^ v128::eq32(other, other); // XOR with ones
}

using stx::se_t;
using stx::se_storage;

// se_t<> with native endianness
template <typename T, std::size_t Align = alignof(T)>
using nse_t = se_t<T, false, Align>;

template <typename T, std::size_t Align = alignof(T)>
using be_t = se_t<T, std::endian::little == std::endian::native, Align>;
template <typename T, std::size_t Align = alignof(T)>
using le_t = se_t<T, std::endian::big == std::endian::native, Align>;

// Type converter: converts native endianness arithmetic/enum types to appropriate se_t<> type
template <typename T, bool Se, typename = void>
struct to_se
{
	template <typename T2, typename = void>
	struct to_se_
	{
		using type = T2;
	};

	template <typename T2>
	struct to_se_<T2, std::enable_if_t<std::is_arithmetic<T2>::value || std::is_enum<T2>::value>>
	{
		using type = std::conditional_t<(sizeof(T2) > 1), se_t<T2, Se>, T2>;
	};

	// Convert arithmetic and enum types
	using type = typename to_se_<T>::type;
};

template <bool Se>
struct to_se<v128, Se>
{
	using type = se_t<v128, Se>;
};

template <bool Se>
struct to_se<u128, Se>
{
	using type = se_t<u128, Se>;
};

template <bool Se>
struct to_se<s128, Se>
{
	using type = se_t<s128, Se>;
};

template <typename T, bool Se>
struct to_se<const T, Se, std::enable_if_t<!std::is_array<T>::value>>
{
	// Move const qualifier
	using type = const typename to_se<T, Se>::type;
};

template <typename T, bool Se>
struct to_se<volatile T, Se, std::enable_if_t<!std::is_array<T>::value && !std::is_const<T>::value>>
{
	// Move volatile qualifier
	using type = volatile typename to_se<T, Se>::type;
};

template <typename T, bool Se>
struct to_se<T[], Se>
{
	// Move array qualifier
	using type = typename to_se<T, Se>::type[];
};

template <typename T, bool Se, std::size_t N>
struct to_se<T[N], Se>
{
	// Move array qualifier
	using type = typename to_se<T, Se>::type[N];
};

// BE/LE aliases for to_se<>
template <typename T>
using to_be_t = typename to_se<T, std::endian::little == std::endian::native>::type;
template <typename T>
using to_le_t = typename to_se<T, std::endian::big == std::endian::native>::type;

// BE/LE aliases for atomic_t
template <typename T>
using atomic_be_t = atomic_t<be_t<T>>;
template <typename T>
using atomic_le_t = atomic_t<le_t<T>>;

template <typename T, bool Se, std::size_t Align>
struct fmt_unveil<se_t<T, Se, Align>, void>
{
	using type = typename fmt_unveil<T>::type;

	static inline auto get(const se_t<T, Se, Align>& arg)
	{
		return fmt_unveil<T>::get(arg);
	}
};
