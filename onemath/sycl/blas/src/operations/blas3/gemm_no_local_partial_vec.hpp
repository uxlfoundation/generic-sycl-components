/***************************************************************************
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

#ifndef ONEMATH_SYCL_BLAS_BLAS3_NO_LOCAL_PARTIAL_VEC_GEMM_HPP
#define ONEMATH_SYCL_BLAS_BLAS3_NO_LOCAL_PARTIAL_VEC_GEMM_HPP

#include "gemm_common.hpp"
#include "gemm_load_store.hpp"
#ifdef BLAS_ENABLE_COMPLEX
#include "gemm_load_store_complex.hpp"
#endif

namespace blas {

/*!
 * @brief This partial specialization of the Gemm class is a partially
 * vectorized, no local memory kernel. It can only vectorize loading of A when A
 * is not transposed and vice-versa for B, only vectorizing when it is
 * transposed. The kernel thus provides best performance when only B is
 * transposed, however its performance even when vectorization is disabled is
 * equal to or better than the previous no_local kernel which included no
 * vectorization.
 *
 *
 * @tparam ClSize  the size of the cache line of the architecture. This
 * parameter is not used in this kernel.
 * @tparam TileType  determines the size of the local, work group, and top
 *                   level tiles to use, see Tile
 * @tparam NbcA Used to control bank conflict mitigation for A, not used in this
 * kernel.
 * @tparam NbcB Used to control bank conflict mitigation for B, not used in this
 * kernel.
 * @tparam TransA  if true, matrix A will be transposed on the fly
 * @tparam TransB  if true, matrix B will be transposed on the fly
 * @tparam element_t  type of scalar alpha & beta
 * @tparam UseJointMatrix boolean parameter to decide whether to use
 * joint_matrix or not
 */
template <typename input_t, typename output_t, bool DoubleBuffer, bool NbcA,
          bool NbcB, int ClSize, typename tile_type, bool TransA, bool TransB,
          bool SymmA, bool SymmB, typename element_t, bool is_beta_zero,
          int VectorSize>
class Gemm<input_t, output_t, DoubleBuffer, NbcA, NbcB, ClSize, tile_type,
           TransA, TransB, SymmA, SymmB, element_t, is_beta_zero,
           static_cast<int>(gemm_memory_t::no_local),
           static_cast<int>(gemm_algorithm_t::standard),
           static_cast<int>(gemm_vectorization_t::partial), VectorSize,
           static_cast<int>(gemm_batch_type_t::strided), false> {
 public:
  using value_t = typename std::remove_const<typename input_t::value_t>::type;
  using index_t = typename std::make_signed<typename input_t::index_t>::type;
  using address_t = sycl::access::address_space;
  using packetize_t = Packetize<VectorSize, value_t, index_t>;
  using vector_t = typename packetize_t::PacketType;
  static constexpr int local_memory_size = 0;
  /*! @brief The number of rows processed by each work item */
  static constexpr index_t item_rows = tile_type::item_rows;
  /*! @brief The number of cols processed by each work item */
  static constexpr index_t item_cols = tile_type::item_cols;
  /*! @brief The number of work items in each row of work group */
  static constexpr index_t wg_rows = tile_type::wg_rows;
  /*! @brief The number of work items in each column of work group */
  static constexpr index_t wg_cols = tile_type::wg_cols;
  /*! @brief Number of rows within a work-group level tile */
  static constexpr index_t block_rows = wg_rows * item_rows;
  /*! @brief Number of columns within a work-group level tile */
  static constexpr index_t block_cols = wg_cols * item_cols;
  /*! @brief A boolean parameter represents wheather or not matrix A is
   * transposed */
  static constexpr bool trans_a = TransA;
  /*! @brief A boolean parameter represents wheather or not matrix B is
   * transposed */
  static constexpr bool trans_b = TransB;

  static_assert(wg_cols * item_cols == item_rows * wg_rows,
                "Work group size should be a multiple "
                "of the number of rows in a block\n"
                " --- this is ensured iff: item_rows | wg_cols");

  static_assert(item_rows % packetize_t::packet_size == 0,
                "Item rows must be a multiple of the vector packet size");
  static_assert(item_cols % packetize_t::packet_size == 0,
                "Item cols must be a multiple of the vector packet size");

#ifdef BLAS_ENABLE_COMPLEX
  static_assert((VectorSize == 1 && is_complex_sycl<element_t>::value) ||
                    !is_complex_sycl<element_t>::value,
                "Vector size should be equal to 1 for Complex Data types");
#endif

  input_t a_;
  input_t b_;
  output_t c_;
  const element_t alpha_;
  const element_t beta_;
  index_t batch_size_;
  index_t stridea_;
  index_t strideb_;
  index_t stridec_;

  ONEMATH_SYCL_BLAS_INLINE Gemm(input_t A, input_t B, output_t C, element_t alpha,
                       element_t beta, index_t batch_size, index_t stride_a,
                       index_t stride_b, index_t stride_c)
      : a_(A),
        b_(B),
        c_(C),
        alpha_(alpha),
        beta_(beta / alpha),
        batch_size_(batch_size),
        stridea_{stride_a},
        strideb_{stride_b},
        stridec_{stride_c} {}

  /*!
   * @brief Get the type of this NoLocalGemmFactory as a human readable string.
   */
  static ONEMATH_SYCL_BLAS_INLINE std::string get_type_string() noexcept {
    std::ostringstream str{};
    str << "Gemm <" << DoubleBuffer << ", " << NbcA << ", " << NbcB << ", "
        << ClSize << ", " << tile_type::get_type_string() << ", "
        << type_string<value_t>::get_value() << "_"
        << type_string<element_t>::get_value() << "gemm_memory:no_local, "
        << "gemm_algorithm:standard, " << "gemm_vectorization:partial, "
        << "vector size" << VectorSize << ", batch_type:strided>";
    return str.str();
  }
  /*!
   *@brief gt_workgroup_cluster. This function is used to find the optimum
   *number of work_group required to execute each GEMM.
   *
   */
  ONEMATH_SYCL_BLAS_INLINE index_t get_workgroup_cluster() const noexcept {
    return (((a_.get_size_row() - 1) / (item_rows * wg_rows) + 1) *
            ((b_.get_size_col() - 1) / (item_cols * wg_cols) + 1));
  }
  /*!
   *@brief get_num_workgroup_cluster. This function is used to extend the number
   *of work_group cluster, in order to make sure that atleast 4 gemm operations
   *is available per work group. The number 4 is used based on empirical
   *research.
   *
   */
  ONEMATH_SYCL_BLAS_INLINE index_t
  get_num_workgroup_cluster(index_t compute_units) const noexcept {
    constexpr index_t num_gemm_per_compute_units = 4;
    return ((num_gemm_per_compute_units * compute_units - 1) /
                get_workgroup_cluster() +
            1);
  }

  ONEMATH_SYCL_BLAS_INLINE sycl::nd_range<1> get_nd_range(
      index_t compute_units) const noexcept {
    const sycl::range<1> nwg(get_workgroup_cluster() *
                             get_num_workgroup_cluster(compute_units));
    const sycl::range<1> wgs(wg_rows * wg_cols);

    return sycl::nd_range<1>(nwg * wgs, wgs);
  }

  ONEMATH_SYCL_BLAS_INLINE index_t get_size() const {
    return a_.get_size_row() * b_.get_size_col();
  }

  ONEMATH_SYCL_BLAS_INLINE bool valid_thread(const sycl::nd_item<1> &) const {
    return true;
  }

  ONEMATH_SYCL_BLAS_INLINE void eval(sycl::nd_item<1> id) noexcept {
    index_t m = a_.get_size_row();
    index_t n = b_.get_size_col();
    const index_t k = a_.get_size_col();
    const index_t lda = a_.getSizeL();
    const index_t ldb = b_.getSizeL();
    const index_t ldc = c_.getSizeL();

    // The batch index that each workgroup should start working with
    const index_t wg_batch_id = id.get_group(0) / get_workgroup_cluster();
    // This will disable all workgroups that dont have any batch to work on
    if (wg_batch_id >= batch_size_) {
      return;
    }

    const index_t batch_stride =
        id.get_group_range(0) / get_workgroup_cluster();

    const index_t a_size = trans_a ? m * lda : k * lda;
    const index_t b_size = trans_b ? ldb * k : n * ldb;
    const index_t c_size = ldc * n;

    auto orig_A = a_.get_pointer() + (wg_batch_id * stridea_);
    auto orig_B = b_.get_pointer() + (wg_batch_id * strideb_);
    auto orig_C = c_.get_pointer() + (wg_batch_id * stridec_);

    const index_t number_of_block_per_row = ((m - 1) / block_rows) + 1;
    /* linear work group id The number of work-group required to executed each
     * batch efficiently*/
    const index_t wg_id = id.get_group(0) % get_workgroup_cluster();
    /* linear work item id */
    const index_t item_id = id.get_local_id(0);
    /* row tile id  per work group */
    const index_t tile_id_row = wg_id % number_of_block_per_row;
    /* column tile id per work group */
    const index_t tile_id_col = wg_id / number_of_block_per_row;
    /* the start position of the tile-row per work group */
    const index_t wg_row = tile_id_row * block_rows;
    /* the start position of the tile-column per work group */
    const index_t wg_col = tile_id_col * block_cols;
    /*!
     * @brief is_internal_block_m and is_internal_block_n is used to distinguish
     * the internal block. Therefore, work items using these blocks dont need to
     * check for boundaries.
     */
    const bool is_internal_block =
        (m - wg_row >= block_rows) && (n - wg_col >= block_cols);

    /// Packet size for A. It can only be vectorized if it is not transposed.
    constexpr index_t a_packet_size = (trans_a ? 1 : packetize_t::packet_size);
    /// Packet size for B. It can only be vectorized if it is transposed.
    constexpr index_t b_packet_size = (trans_b ? packetize_t::packet_size : 1);

    /* work item id per row */
    const index_t local_item_id_row =
        item_id % wg_rows * (is_internal_block ? a_packet_size : 1);
    /* work item id per column */
    const index_t local_item_id_col =
        item_id / wg_rows * (is_internal_block ? b_packet_size : 1);

    /* Exiting from any threads outside of the m and n boundary */
    const bool out_of_range = ((local_item_id_row + wg_row >= m) ||
                               (local_item_id_col + wg_col >= n));
    /*
     * The ma and nb are used to adjust the start position of each work-item for
     * A, B and C matrices.
     */
    const index_t dim_m_a_start = (local_item_id_row + wg_row);
    const index_t dim_n_b_start = (local_item_id_col + wg_col);

    /*! @brief Adjusting the start position of A, B , and C */
    orig_A += dim_m_a_start * (trans_a ? lda : 1);
    orig_B += dim_n_b_start * (trans_b ? 1 : ldb);
    orig_C += dim_m_a_start + (dim_n_b_start * ldc);

    /*
     * The following lambdas: boundary_check_m, boundary_check_n, and
     * boundary_check_c  are used to check the A, B , and C boundaries
     * respectively.
     */
    const auto boundary_check_m =
        [&](index_t dim_m_a_start)
            ONEMATH_SYCL_BLAS_ALWAYS_INLINE { return dim_m_a_start < m; };
    const auto boundary_check_n =
        [&](index_t dim_n_b_start)
            ONEMATH_SYCL_BLAS_ALWAYS_INLINE { return dim_n_b_start < n; };
    const auto boundary_check_c =
        [&](index_t dim_m_c_start, index_t dim_n_c_start)
            ONEMATH_SYCL_BLAS_ALWAYS_INLINE {
              return (dim_m_c_start < m && dim_n_c_start < n);
            };

    // computing the next element for a and b;
    const index_t A_ptr_index =
        (trans_a ? lda : 1) * wg_rows * (is_internal_block ? a_packet_size : 1);
    const index_t B_ptr_index =
        (trans_b ? 1 : ldb) * wg_cols * (is_internal_block ? b_packet_size : 1);
    /* temporary register array used to prefetch columns of A*/
    value_t reg_a[item_rows];
    /* temporary register used to prefetch elements of B*/
    value_t reg_b[item_cols];
    /*
     * computing the gemm panel
     */
    if ((is_internal_block == true)) {
      compute_gemm_no_shared_pannel<false, a_packet_size, b_packet_size>(
          orig_A, orig_B, orig_C, a_size, b_size, c_size, a_.get_size_col(), k,
          dim_m_a_start, dim_n_b_start, A_ptr_index, B_ptr_index,
          boundary_check_m, boundary_check_n, boundary_check_c, reg_a, reg_b,
          out_of_range, batch_stride, wg_batch_id, batch_size_, lda, ldb, ldc);
    } else {
      compute_gemm_no_shared_pannel<true, 1, 1>(
          orig_A, orig_B, orig_C, a_size, b_size, c_size, a_.get_size_col(), k,
          dim_m_a_start, dim_n_b_start, A_ptr_index, B_ptr_index,
          boundary_check_m, boundary_check_n, boundary_check_c, reg_a, reg_b,
          out_of_range, batch_stride, wg_batch_id, batch_size_, lda, ldb, ldc);
    }
  }
  /** @brief If beta is not zero then this function will load in values from C,
  multiply them by the beta value and store them in the results register. If
  beta is zero then this function does nothing. */
  template <bool need_check_boundary, index_t a_packet_size,
            index_t b_packet_size, typename InputPointerType,
            typename CheckBoundaryType, bool beta_zero = is_beta_zero>
  ONEMATH_SYCL_BLAS_INLINE typename std::enable_if<!beta_zero>::type scaling_c(
      element_t *reg_res, InputPointerType C, const index_t &ldc,
      const index_t &dim_m_c_start, const index_t &dim_n_c_start,
      CheckBoundaryType check_boundary, bool out_of_range) {
    if (out_of_range) {
      return;
    }
#pragma unroll
    for (index_t i = 0; i < item_cols; ++i) {
#pragma unroll
      for (index_t j = 0; j < item_rows; ++j) {
        if (do_check<need_check_boundary>(check_boundary(
                dim_m_c_start + j * wg_rows, dim_n_c_start + i * wg_cols))) {
          reg_res[i * item_rows + j] =
              beta_ * C[(j % a_packet_size) +
                        (j / a_packet_size) * wg_rows * a_packet_size];
        }
      }
      C += ((i + 1) % b_packet_size == 0
                ? ((wg_cols * b_packet_size - (b_packet_size - 1)) * ldc)
                : ldc);
    }
  }

  template <bool need_check_boundary, index_t, index_t,
            typename InputPointerType, typename CheckBoundaryType,
            bool beta_zero = is_beta_zero>
  ONEMATH_SYCL_BLAS_INLINE typename std::enable_if<beta_zero>::type scaling_c(
      element_t *reg_res, InputPointerType, const index_t &, const index_t &,
      const index_t &, CheckBoundaryType, bool) {
#pragma unroll
    for (index_t i = 0; i < item_cols * item_rows; ++i) {
      reg_res[i] = 0;
    }
  }

  template <bool need_check_boundary, index_t a_packet_size,
            index_t b_packet_size, typename A_t, typename B_t, typename C_t,
            typename check_boundary_m_t, typename check_boundary_n_t,
            typename check_boundary_c_t>
  ONEMATH_SYCL_BLAS_INLINE void compute_gemm_no_shared_pannel(
      A_t orig_A, B_t orig_B, C_t orig_C, const index_t &a_size,
      const index_t &b_size, const index_t &c_size, index_t orig_k, index_t k,
      const index_t &dim_m_a_start, const index_t &dim_n_b_start,
      const index_t &A_ptr_index, const index_t &B_ptr_index,
      const check_boundary_m_t &boundary_check_m,
      const check_boundary_n_t &boundary_check_n,
      const check_boundary_c_t &boundary_check_c, value_t *reg_a,
      value_t *reg_b, const bool out_of_range, const index_t &batch_stride,
      const index_t &wg_batch_id, index_t batch_size, const index_t &lda,
      const index_t &ldb, const index_t &ldc) noexcept {
    do {
      auto A = orig_A;
      auto B = orig_B;
      auto C = orig_C;

      /* 2D register array used to store the result C*/
      element_t reg_res[item_rows * item_cols];
      scaling_c<need_check_boundary, a_packet_size, b_packet_size>(
          reg_res, C, ldc, dim_m_a_start, dim_n_b_start, boundary_check_c,
          out_of_range);
      while (k > 0) {
        /*
         * Loading a corresponding block of matrix A into reg_a
         */
        load<item_rows, wg_rows * a_packet_size, need_check_boundary,
             a_packet_size>(A, reg_a, A_ptr_index, dim_m_a_start,
                            boundary_check_m, out_of_range);

        /*
         * Loading a corresponding block of matrix B into reg_b
         */
        load<item_cols, wg_cols * b_packet_size, need_check_boundary,
             b_packet_size>(B, reg_b, B_ptr_index, dim_n_b_start,
                            boundary_check_n, out_of_range);

        /*
         * Computing a the partial GEMM for the loaded block of reg_a andd
         * reg_b and adding the result into reg_res
         */
        compute_block_gemm_no_shared(reg_a, reg_b, reg_res);

        /*
         * Moving forward to the next block
         */
        --k;
        A = A + (trans_a ? 1 : lda);
        B = B + (trans_b ? ldb : 1);
      }
      /*
       *  Storing the reg_res into C matrix
       */
      store<need_check_boundary, a_packet_size, b_packet_size>(
          C, reg_res, dim_m_a_start, dim_n_b_start, boundary_check_c,
          out_of_range, ldc);

      orig_A += (stridea_ * batch_stride);
      orig_B += (strideb_ * batch_stride);
      orig_C += (stridec_ * batch_stride);
      k = orig_k;
      // batch_size_ must be signed as the negative value has meaning here.
      batch_size -= batch_stride;
    } while (batch_size > wg_batch_id);
  }
  /*!
   * @brief binding the placeholder accessors to the SYCL command group
   * handler
   * @param h: SYCL command group handler. */
  void bind(sycl::handler &h) {
    a_.bind(h);
    b_.bind(h);
    c_.bind(h);
  }

  void adjust_access_displacement() {
    a_.adjust_access_displacement();
    b_.adjust_access_displacement();
    c_.adjust_access_displacement();
  }

 private:
  /*!
   * @brief Following function load a block of row_items/col_items elements from
   * A/B matrix into reg_a/reg_b.
   * @tparam item_size it is the size of private register: either row_items or
   * column_item
   * @tparam next_element : is the stride to acces the next element of A or B
   * matrix. it is either wg_rows or wg_cols.
   * @tparam check_block: determined whether or not the requested block is
   * internal. false means no need to check the boundaries
   * @tparam pointerType: the type of the input matrix
   * @tparam check_boundary: the type of a function used for checking the
   * boundary for blocks of data located at the edge of the input matrix
   * @param ptr : the input matrix, either A or B.
   * @param reg[item_size] the private array containing the input block per
   * work-item: it is either reg_a or reg_b.
   * @param ld : the leading dimension of the input matrix.
   * @param index: the start position of the block of data to be loaded.
   * @param chk_boundary: an instance of the check_boundary function
   */

  template <index_t item_size, index_t next_element, bool check_block,
            index_t work_per_load, typename PointerType,
            typename check_boundary>
  ONEMATH_SYCL_BLAS_INLINE void load(PointerType ptr, value_t *reg, const index_t &ld,
                            index_t index, const check_boundary &chk_boundary,
                            const bool out_of_range) noexcept {
    if (out_of_range) {
      return;
    }
    // Work done in this loop is reduced proportionally to the work done per
    // load (vector packet size).
#pragma unroll
    for (int i = 0; i < item_size / work_per_load; i++) {
      // Check that the last element of the packet loaded is in range
      bool in_range =
          do_check<check_block>(chk_boundary(index + (work_per_load - 1)));

      using l_vector_t =
          typename Packetize<work_per_load, value_t, index_t>::PacketType;
      l_vector_t in_vec{0};
      if (in_range) {
        in_vec.template load<address_t::global_space>(
            0, sycl::multi_ptr<const value_t, address_t::global_space>(ptr));
      }
      in_vec.template store<address_t::private_space>(
          0, sycl::multi_ptr<value_t, address_t::private_space>(reg));

      // Move pointers and update index for next load
      ptr += ld;
      index += next_element;
      reg += work_per_load;
    }
  }

  /*!
   * @brief The following function compute the partial GEMM for the input block
   * reg_a and reg_b and add the result to the reg_res
   * @param reg_a  temporary register array used to prefetch columns of A
   * @param reg_b  temporary register used to prefetch elements of B
   * @param reg_res  2D register array used to store the result C
   */
  ONEMATH_SYCL_BLAS_INLINE void compute_block_gemm_no_shared(
      value_t *reg_a, value_t *reg_b, element_t *reg_res) noexcept {
#pragma unroll
    for (int i = 0; i < item_cols; i++) {
#pragma unroll
      for (int j = 0; j < item_rows; j++) {
        reg_res[i * item_rows + j] =
            mul_add(reg_a[j], reg_b[i], reg_res[i * item_rows + j]);
      }
    }
  }

  template <bool internal, index_t work_per_load, typename OutputPointerType>
  ONEMATH_SYCL_BLAS_INLINE typename std::enable_if<!internal>::type store_packet(
      element_t *reg, OutputPointerType out_ptr) {
    *out_ptr = alpha_ * (*reg);
  }

  template <bool internal, index_t work_per_load, typename OutputPointerType>
  ONEMATH_SYCL_BLAS_INLINE typename std::enable_if<internal>::type store_packet(
      element_t *reg, OutputPointerType out_ptr) {
    using l_vector_t =
        typename Packetize<work_per_load, element_t, index_t>::PacketType;
    l_vector_t out_vec{0};

    out_vec.template load<address_t::private_space>(
        0, sycl::multi_ptr<const element_t, address_t::private_space>(reg));
    out_vec *= alpha_;

    out_vec.template store<address_t::global_space>(
        0, sycl::multi_ptr<element_t, address_t::global_space>(out_ptr));
  }

  /*!
   * @brief For each work itemThe following function store the computed block of
   * GEMM reg_res into output matrix C
   * @tparam check_block: determined whether or not the requested block is
   * internal. false means no need to check the boundaries
   * @tparam a_packet_size Packet/vector size of A
   * @tparam b_packet_size Packet/vector size of B
   * @tparam pointerType: the type of the matrix C
   * @tparam check_boundary: the type of a function used for checking the
   * boundary for blocks of data located at the edge of the input matrix
   * @param c: is the output matrix C
   * @param reg_res  2D register array used to store the result C
   * @param chk_boundary: an instance of the check_boundary function
   * @param ldc is the leading dimension of C
   * @param mc and nc are indices, used to check the boundary of C
   */
  template <bool check_block, index_t a_packet_size, index_t b_packet_size,
            typename PointerType, typename check_boundary>
  ONEMATH_SYCL_BLAS_INLINE void store(PointerType C, element_t *reg_res,
                             const index_t &dim_m_c_start,
                             const index_t &dim_n_c_start,
                             const check_boundary &chk_boundary,
                             const bool out_of_range,
                             const index_t &ldc) noexcept {
    if (out_of_range) {
      return;
    }
#pragma unroll
    for (int i = 0; i < item_cols; i++) {
#pragma unroll
      for (int j = 0; j < item_rows / a_packet_size; j++) {
        if (do_check<check_block>(chk_boundary(dim_m_c_start + j * wg_rows,
                                               dim_n_c_start + i * wg_cols))) {
          using l_vector_t =
              typename Packetize<a_packet_size, element_t, index_t>::PacketType;
          l_vector_t out_vec{0};

          out_vec.template load<address_t::private_space>(
              0, sycl::multi_ptr<const element_t, address_t::private_space>(
                     reg_res + i * item_rows + j * a_packet_size));
          out_vec *= alpha_;

          out_vec.template store<address_t::global_space>(
              0, sycl::multi_ptr<element_t, address_t::global_space>(
                     C + j * wg_rows * a_packet_size));
        }
      }
      C += ((i + 1) % b_packet_size == 0
                ? ((wg_cols * b_packet_size - (b_packet_size - 1)) * ldc)
                : ldc);
    }
  }
};  // end class Gemm

}  // namespace blas

#endif  // ONEMATH_SYCL_BLAS_BLAS3_NO_LOCAL_PARTIAL_VEC_GEMM_HPP
