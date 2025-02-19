// SPDX-FileCopyrightText: Intel Corporation
//
// SPDX-License-Identifier: BSD-3-Clause

#include "shp-tests.hpp"

using T = int;
using DV = shp::distributed_vector<T, shp::shared_allocator<T>>;
using V = std::vector<T>;

struct increment {
  auto operator()(auto &&v) const { v++; }
};

TEST(ShpTests, Take) {
  const int n = 10;
  V a(n);
  DV dv_a(n);

  std::iota(a.begin(), a.end(), 20);
  std::iota(dv_a.begin(), dv_a.end(), 20);

  auto aview = a | std::ranges::views::take(2);
  auto dv_aview = dv_a | std::ranges::views::take(2);
  EXPECT_TRUE(equal(aview, dv_aview));

  std::ranges::for_each(aview, increment{});
  shp::for_each(shp::par_unseq, dv_a, increment{});
  EXPECT_TRUE(equal(aview, dv_aview));
}

TEST(ShpTests, Drop) {
  const int n = 10;
  V a(n);
  DV dv_a(n);

  auto incr = [](auto &&v) { v++; };

  std::iota(a.begin(), a.end(), 20);
  std::iota(dv_a.begin(), dv_a.end(), 20);

  auto aview = a | std::ranges::views::drop(2);
  auto dv_aview = dv_a | std::ranges::views::drop(2);
  EXPECT_TRUE(equal(aview, dv_aview));

  std::ranges::for_each(aview, incr);
  shp::for_each(shp::par_unseq, dv_aview, incr);
  EXPECT_TRUE(equal(aview, dv_aview));
}
