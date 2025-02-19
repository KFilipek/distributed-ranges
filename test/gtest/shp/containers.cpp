// SPDX-FileCopyrightText: Intel Corporation
//
// SPDX-License-Identifier: BSD-3-Clause

#include "shp-tests.hpp"

using V = std::vector<int>;
using CV = const std::vector<int>;
using CVR = const std::vector<int> &;

using DV = shp::distributed_vector<int>;
using CDV = const shp::distributed_vector<int>;
using CDVR = const shp::distributed_vector<int> &;

TEST(ShpTests, DistributedVector) {
  static_assert(std::ranges::random_access_range<V>);
  static_assert(std::ranges::random_access_range<CV>);
  static_assert(std::ranges::random_access_range<CVR>);

  static_assert(std::ranges::random_access_range<DV>);
  static_assert(std::ranges::random_access_range<CDV>);
  static_assert(std::ranges::random_access_range<CDVR>);
}
