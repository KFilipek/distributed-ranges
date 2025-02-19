// SPDX-FileCopyrightText: Intel Corporation
//
// SPDX-License-Identifier: BSD-3-Clause

#include "cpu-tests.hpp"

TEST(CpuMpiTests, RemoteVectorRequirements) {
  using RV = lib::remote_vector<int>;

  static_assert(rng::range<RV>);
  static_assert(lib::remote_contiguous_range<RV>);

  // RV rv;
}
