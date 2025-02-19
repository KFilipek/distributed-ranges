// SPDX-FileCopyrightText: Intel Corporation
//
// SPDX-License-Identifier: BSD-3-Clause

#pragma once

#ifdef SYCL_LANGUAGE_VERSION
#include <oneapi/dpl/algorithm>
#include <oneapi/dpl/execution>
#include <oneapi/dpl/numeric>
#endif

#include <cassert>
#include <concepts>
#include <fstream>
#include <iostream>
#include <iterator>
#include <memory>
#include <span>
#include <string>
#include <utility>
#include <vector>

#ifdef DR_FORMAT
#include <fmt/core.h>
#include <fmt/ranges.h>
#endif

#include "vendor/source_location/source_location.hpp"

// MPI should be optional
#include "mkl.h"
#include "mpi.h"

#ifdef DR_STD_RNG
#include <ranges>
namespace rng = std::ranges;
#else
// clang++/icpx do not work with /usr/include/c++/11/ranges
#include "range/v3/all.hpp"
namespace rng = ranges;
#endif

#include <experimental/mdarray>
#include <experimental/mdspan>
namespace stdex = std::experimental;

#include "details/logger.hpp"

#ifdef SYCL_LANGUAGE_VERSION
#include "details/allocators.hpp"
#endif

#include "details/common.hpp"
#include "details/communicator.hpp"

#include "concepts/concepts.hpp"

#include "details/remote_memory.hpp"

#include "details/remote_vector.hpp"

#include "details/memory.hpp"

#include "details/halo.hpp"

#include "details/distributed_vector.hpp"

#include "details/distributed_mdspan.hpp"

#include "details/remote_span.hpp"

#include "details/distributed_span.hpp"

#include "details/execution_policies.hpp"

#ifdef SYCL_LANGUAGE_VERSION
#include "algorithms/sycl_algorithms.hpp"
#endif

#include "details/views.hpp"

#include "algorithms/algorithms.hpp"
#include "algorithms/transpose.hpp"
