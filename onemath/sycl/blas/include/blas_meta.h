/***************************************************************************
 *
 *  @license
 *  Copyright (C) Codeplay Software Limited
 *  Licensed under the Apache License, Version 2.0 (the "License");
 *  you may not use this file except in compliance with the License.
 *  You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 *  For your convenience, a copy of the License has been included in this
 *  repository.
 *
 *  Unless required by applicable law or agreed to in writing, software
 *  distributed under the License is distributed on an "AS IS" BASIS,
 *  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *  See the License for the specific language governing permissions and
 *  limitations under the License.
 *
 *
 *
 **************************************************************************/

#ifndef ONEMATH_SYCL_BLAS_META_H
#define ONEMATH_SYCL_BLAS_META_H

#include <sycl/sycl.hpp>
#include <type_traits>
#include <utility>
#ifdef BLAS_ENABLE_COMPLEX
#define SYCL_EXT_ONEAPI_COMPLEX
#include <complex>
#if __has_include(<sycl/ext/oneapi/experimental/complex/complex.hpp>)
#include <sycl/ext/oneapi/experimental/complex/complex.hpp>
#else
#include <sycl/ext/oneapi/experimental/sycl_complex.hpp>
#endif
#endif

namespace blas {

/**
 * @class Access
 * @brief A wrapper type for Layout, providing common functionality and a safer
 * interface.
 */
enum class access_layout { row_major, col_major };

struct row_major {
  static constexpr bool is_col_major() { return false; }
};
struct col_major {
  static constexpr bool is_col_major() { return true; }
};

template <access_layout layout>
struct Layout;

template <>
struct Layout<access_layout::row_major> {
  using type = row_major;
};

template <>
struct Layout<access_layout::col_major> {
  using type = col_major;
};
/**
 * @enum Trans
 * @brief The possible transposition options for a matrix, expressed
 * algebraically.
 */
enum class transpose_type : char {
  Normal = 'n',
  Transposed = 't',
  Conjugate = 'c'
};

/**
 * @enum Uplo
 * @brief Specifies whether the lower or upper triangle needs to be accessed.
 */
enum class uplo_type : char { Upper = 'u', Lower = 'l' };

/**
 * @enum Diag
 * @brief Specifies the values on the diagonal of a triangular matrix.
 */
enum class diag_type : char { Nonunit = 'n', Unit = 'u' };

// choosing value at compile-time
template <bool Conds, typename val_t, val_t value_one_t, val_t value_two_t>
struct Choose {
  static constexpr auto type = value_one_t;
};

template <typename val_t, val_t value_one_t, val_t value_two_t>
struct Choose<false, val_t, value_one_t, value_two_t> {
  static constexpr auto type = value_two_t;
};
/// \struct RemoveAll
/// \brief These methods are used to remove all the & const and * from  a type.
/// template parameters
/// \tparam element_t : the type we are interested in
template <typename element_t>
struct RemoveAll {
  using Type = typename std::remove_reference<typename std::remove_cv<
      typename std::remove_pointer<element_t>::type>::type>::type;
};

template <typename container_t>
struct ValueType {
  using type = typename RemoveAll<container_t>::Type;
};

template <typename element_t, typename container_t>
struct RebindType {
  using type = RemoveAll<element_t> *;
};

template <typename index_t>
inline bool is_power_of_2(index_t ind) {
  return ind > 0 && !(ind & (ind - 1));
}

// This function returns the nearest power of 2
// if roundup is true returns result>=wgsize
// else it return result <= wgsize
template <typename index_t>
static inline index_t get_power_of_two(index_t wGSize, bool rounUp) {
  if (rounUp) --wGSize;
  wGSize |= (wGSize >> 1);
  wGSize |= (wGSize >> 2);
  wGSize |= (wGSize >> 4);
  wGSize |= (wGSize >> 8);
  wGSize |= (wGSize >> 16);
#if defined(__x86_64__) || defined(_M_X64) || defined(__amd64) || \
    defined(__aarch64__) || defined(_WIN64)
  wGSize |= (wGSize >> 32);
#endif
  return ((!rounUp) ? (wGSize - (wGSize >> 1)) : ++wGSize);
}

#ifdef __SYCL_DEVICE_ONLY__
#define ONEMATH_SYCL_BLAS_ALWAYS_INLINE \
  __attribute__((flatten)) __attribute__((always_inline))
#else
#define ONEMATH_SYCL_BLAS_ALWAYS_INLINE
#endif

#define ONEMATH_SYCL_BLAS_INLINE ONEMATH_SYCL_BLAS_ALWAYS_INLINE inline

template <typename index_t>
static ONEMATH_SYCL_BLAS_INLINE index_t roundUp(index_t x, index_t y) {
  return ((x + y - 1) / y) * y;
}

template <typename index_t, typename vector_t>
index_t vec_total_size(index_t &vector_size, vector_t &&current_vector) {
  vector_size += static_cast<index_t>(current_vector.size());
  return 0;
}

template <typename vector_t>
int append_vector(vector_t &lhs_vector, vector_t const &rhs_vector) {
  lhs_vector.insert(lhs_vector.end(), rhs_vector.begin(), rhs_vector.end());
  return 0;
}

template <typename first_vector_t, typename... other_vector_t>
first_vector_t concatenate_vectors(first_vector_t first_vector,
                                   other_vector_t &&...other_vectors) {
  int first_Vector_size = static_cast<int>(first_vector.size());
  int s[] = {vec_total_size(first_Vector_size, other_vectors)..., 0};
  first_vector.reserve(first_Vector_size);
  int val[] = {append_vector(first_vector,
                             std::forward<other_vector_t>(other_vectors))...,
               0};
  return std::move(first_vector);
}

/**
 * @brief Defines if a type is a scalar within the context of sycl.
 * @tparam type The type to be tested.
 */
template <typename type>
struct is_sycl_scalar
    : std::conditional<std::is_scalar<type>::value, std::true_type,
                       std::false_type>::type {};

template <>
struct is_sycl_scalar<sycl::half> : std::true_type {};

template <>
struct is_sycl_scalar<float *> : std::false_type {};

template <>
struct is_sycl_scalar<double *> : std::false_type {};

template <class type>
struct is_half
    : std::integral_constant<bool, std::is_same_v<type, sycl::half>> {};

#ifdef BLAS_ENABLE_COMPLEX
// SYCL Complex type alias
template <typename T>
using complex_sycl = typename sycl::ext::oneapi::experimental::complex<T>;

template <class type>
struct is_complex_sycl
    : std::integral_constant<bool,
                             std::is_same_v<type, complex_sycl<double>> ||
                                 std::is_same_v<type, complex_sycl<float>>> {};

template <class type>
struct is_complex_std
    : std::integral_constant<bool,
                             std::is_same_v<type, std::complex<double>> ||
                                 std::is_same_v<type, std::complex<float>>> {};

#endif

class unsupported_exception : public std::runtime_error {
 public:
  unsupported_exception(const std::string &operator_name)
      : std::runtime_error(operator_name), _msg(operator_name) {
    _msg += " operator currently not supported on selected device";
  };
  const char *what() const noexcept override { return _msg.c_str(); }

 private:
  std::string _msg{};
};

}  // namespace blas

#endif  // BLAS_META_H
