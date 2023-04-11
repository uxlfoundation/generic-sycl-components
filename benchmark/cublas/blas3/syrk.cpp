/**************************************************************************
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
 *  SYCL-BLAS: BLAS implementation using SYCL
 *
 *  @filename syrk.cpp
 *
 **************************************************************************/

#include "../utils.hpp"

template <typename scalar_t>
std::string get_name(char uplo, char trans, int n, int k, scalar_t alpha,
                     scalar_t beta) {
  std::ostringstream str{};
  str << "BM_Syrk<" << blas_benchmark::utils::get_type_name<scalar_t>() << ">/"
      << uplo << "/" << trans << "/" << n << "/" << k << "/" << alpha << "/"
      << beta;
  return str.str();
}

template <typename scalar_t, typename... args_t>
static inline void cublas_routine(args_t&&... args) {
  if constexpr (std::is_same_v<scalar_t, float>) {
    CUBLAS_CHECK(cublasSsyrk(std::forward<args_t>(args)...));
  } else if constexpr (std::is_same_v<scalar_t, double>) {
    CUBLAS_CHECK(cublasDsyrk(std::forward<args_t>(args)...));
  }
  return;
}

template <typename scalar_t>
void run(benchmark::State& state, cublasHandle_t* cuda_handle_ptr, char uplo,
         char trans, index_t n, index_t k, scalar_t alpha, scalar_t beta,
         bool* success) {
  index_t lda = (trans == 'n') ? n : k;
  index_t ldc = n;

  cublasHandle_t& cuda_handle = *cuda_handle_ptr;

  // The counters are double. We convert m, n and k to double to avoid
  // integer overflows for n_fl_ops and bytes_processed
  const double n_d = static_cast<double>(n);
  const double k_d = static_cast<double>(k);

  state.counters["k"] = k_d;
  state.counters["n"] = n_d;

  const double mem_readA = n_d * k_d;
  const double mem_readWriteC = 2 * n_d * (n_d + 1) / 2.;
  const double total_mem = (mem_readA + mem_readWriteC) * sizeof(scalar_t);
  state.counters["bytes_processed"] = total_mem;

  const double nflops_AtimesA = n_d * (n_d + 1) * k_d;
  const double nflops_addBetaC = (beta != scalar_t{0}) ? (n_d * (n_d + 1)) : 0;
  const double nflops = nflops_AtimesA + nflops_addBetaC;
  state.counters["n_fl_ops"] = nflops;

  const auto m_a_dim = (trans == 'n') ? (lda * k) : (lda * n);
  // Matrices
  std::vector<scalar_t> a =
      blas_benchmark::utils::random_data<scalar_t>(m_a_dim);
  std::vector<scalar_t> c =
      blas_benchmark::utils::random_data<scalar_t>(ldc * n);

  blas_benchmark::utils::CUDAVector<scalar_t> a_gpu(m_a_dim, a.data());
  blas_benchmark::utils::CUDAVector<scalar_t> c_gpu(ldc * n, c.data());

  cublasFillMode_t c_uplo =
      (uplo == 'u') ? CUBLAS_FILL_MODE_UPPER : CUBLAS_FILL_MODE_LOWER;

  cublasOperation_t c_t = (trans == 'n') ? CUBLAS_OP_N : CUBLAS_OP_T;

#ifdef BLAS_VERIFY_BENCHMARK
  // Run a first time with a verification of the results
  std::vector<scalar_t> c_ref = c;
  reference_blas::syrk(&uplo, &trans, n, k, alpha, a.data(), lda, beta,
                       c_ref.data(), ldc);
  std::vector<scalar_t> c_temp = c;
  {
    blas_benchmark::utils::CUDAVector<scalar_t, true> c_temp_gpu(ldc * n,
                                                                 c_temp.data());
    cublas_routine<scalar_t>(cuda_handle, c_uplo, c_t, n, k, &alpha, a_gpu, lda,
                             &beta, c_temp_gpu, ldc);
  }

  std::ostringstream err_stream;
  if (!utils::compare_vectors(c_temp, c_ref, err_stream, "")) {
    const std::string& err_str = err_stream.str();
    state.SkipWithError(err_str.c_str());
    *success = false;
  };
#endif
  auto blas_warmup = [&]() -> void {
    cublas_routine<scalar_t>(cuda_handle, c_uplo, c_t, n, k, &alpha, a_gpu, lda,
                             &beta, c_gpu, ldc);
    return;
  };

  cudaEvent_t start;
  cudaEvent_t stop;
  CUDA_CHECK(cudaEventCreate(&start));
  CUDA_CHECK(cudaEventCreate(&stop));

  auto blas_method_def = [&]() -> std::vector<cudaEvent_t> {
    CUDA_CHECK(cudaEventRecord(start));
    cublas_routine<scalar_t>(cuda_handle, c_uplo, c_t, n, k, &alpha, a_gpu, lda,
                             &beta, c_gpu, ldc);
    CUDA_CHECK(cudaEventRecord(stop));
    CUDA_CHECK(cudaEventSynchronize(stop));
    return std::vector{start, stop};
  };

  // Warmup
  blas_benchmark::utils::warmup(blas_warmup);
  CUDA_CHECK(cudaStreamSynchronize(NULL));

  blas_benchmark::utils::init_counters(state);

  // Measure
  for (auto _ : state) {
    // Run
    std::tuple<double, double> times =
        blas_benchmark::utils::timef_cuda(blas_method_def);

    // Report
    blas_benchmark::utils::update_counters(state, times);
  }

  state.SetBytesProcessed(state.iterations() * total_mem);
  state.SetItemsProcessed(state.iterations() * nflops);

  blas_benchmark::utils::calc_avg_counters(state);

  CUDA_CHECK(cudaEventDestroy(start));
  CUDA_CHECK(cudaEventDestroy(stop));
};

template <typename scalar_t>
void register_benchmark(blas_benchmark::Args& args,
                        cublasHandle_t* cuda_handle_ptr, bool* success) {
  auto syrk_params = blas_benchmark::utils::get_syrk_params<scalar_t>(args);

  for (auto p : syrk_params) {
    char s_uplo, s_trans;
    index_t n, k;
    scalar_t alpha, beta;
    std::tie(s_uplo, s_trans, n, k, alpha, beta) = p;

    auto BM_lambda = [&](benchmark::State& st, cublasHandle_t* cuda_handle_ptr,
                         char uplo, char trans, index_t n, index_t k,
                         scalar_t alpha, scalar_t beta, bool* success) {
      run<scalar_t>(st, cuda_handle_ptr, uplo, trans, n, k, alpha, beta,
                    success);
    };
    benchmark::RegisterBenchmark(
        get_name<scalar_t>(s_uplo, s_trans, n, k, alpha, beta).c_str(),
        BM_lambda, cuda_handle_ptr, s_uplo, s_trans, n, k, alpha, beta, success)
        ->UseRealTime();
  }
}

namespace blas_benchmark {
void create_benchmark(blas_benchmark::Args& args,
                      cublasHandle_t* cuda_handle_ptr, bool* success) {
  BLAS_REGISTER_BENCHMARK(args, cuda_handle_ptr, success);
}
}  // namespace blas_benchmark