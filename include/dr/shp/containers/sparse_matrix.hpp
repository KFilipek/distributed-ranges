// SPDX-FileCopyrightText: Intel Corporation
//
// SPDX-License-Identifier: BSD-3-Clause

#pragma once

#include <iterator>
#include <shp/containers/index.hpp>
#include <shp/containers/matrix_entry.hpp>
#include <shp/copy.hpp>
#include <shp/distributed_span.hpp>
#include <shp/util/generate_random.hpp>
#include <shp/views/csr_matrix_view.hpp>

namespace shp {

template <rng::random_access_range Segments>
  requires(rng::viewable_range<Segments>)
class distributed_range_accessor {
public:
  using segment_type = rng::range_value_t<Segments>;

  using value_type = rng::range_value_t<segment_type>;

  using size_type = rng::range_size_t<segment_type>;
  using difference_type = rng::range_difference_t<segment_type>;

  using reference = rng::range_reference_t<segment_type>;

  using iterator_category = std::random_access_iterator_tag;

  using iterator_accessor = distributed_range_accessor;
  using const_iterator_accessor = iterator_accessor;
  using nonconst_iterator_accessor = iterator_accessor;

  constexpr distributed_range_accessor() noexcept = default;
  constexpr ~distributed_range_accessor() noexcept = default;
  constexpr distributed_range_accessor(
      const distributed_range_accessor &) noexcept = default;
  constexpr distributed_range_accessor &
  operator=(const distributed_range_accessor &) noexcept = default;

  constexpr distributed_range_accessor(Segments segments, size_type segment_id,
                                       size_type idx) noexcept
      : segments_(rng::views::all(std::forward<Segments>(segments))),
        segment_id_(segment_id), idx_(idx) {}

  constexpr distributed_range_accessor &
  operator+=(difference_type offset) noexcept {

    while (offset > 0) {
      difference_type current_offset = std::min(
          offset,
          difference_type(rng::size(*(segments_.begin() + segment_id_))) -
              difference_type(idx_));
      idx_ += current_offset;
      offset -= current_offset;

      if (idx_ >= rng::size((*(segments_.begin() + segment_id_)))) {
        segment_id_++;
        idx_ = 0;
      }
    }

    while (offset < 0) {
      difference_type current_offset =
          std::min(-offset, difference_type(idx_) + 1);

      difference_type new_idx = difference_type(idx_) - current_offset;

      if (new_idx < 0) {
        segment_id_--;
        new_idx = rng::size(*(segments_.begin() + segment_id_)) - 1;
      }

      idx_ = new_idx;
    }

    return *this;
  }

  constexpr bool operator==(const iterator_accessor &other) const noexcept {
    return segment_id_ == other.segment_id_ && idx_ == other.idx_;
  }

  constexpr difference_type
  operator-(const iterator_accessor &other) const noexcept {
    return difference_type(get_global_idx()) - other.get_global_idx();
  }

  constexpr bool operator<(const iterator_accessor &other) const noexcept {
    if (segment_id_ < other.segment_id_) {
      return true;
    } else if (segment_id_ == other.segment_id_) {
      return idx_ < other.idx_;
    } else {
      return false;
    }
  }

  constexpr reference operator*() const noexcept {
    return *((*(segments_.begin() + segment_id_)).begin() + idx_);
  }

private:
  size_type get_global_idx() const noexcept {
    size_type cumulative_size = 0;
    for (size_t i = 0; i < segment_id_; i++) {
      cumulative_size += segments_[i].size();
    }
    return cumulative_size + idx_;
  }

  rng::views::all_t<Segments> segments_;
  size_type segment_id_ = 0;
  size_type idx_ = 0;
};

template <typename Segments>
using distributed_sparse_matrix_iterator =
    lib::iterator_adaptor<distributed_range_accessor<Segments>>;

template <typename T, std::integral I = std::size_t> class sparse_matrix {
public:
  using size_type = std::size_t;
  using difference_type = std::ptrdiff_t;

  using value_type = shp::matrix_entry<T>;

  using scalar_reference =
      rng::range_reference_t<shp::device_vector<T, shp::device_allocator<T>>>;
  using const_scalar_reference = rng::range_reference_t<
      const shp::device_vector<T, shp::device_allocator<T>>>;

  using reference = shp::matrix_ref<T, scalar_reference>;
  using const_reference = shp::matrix_ref<const T, const_scalar_reference>;

  using key_type = shp::index<>;

  using segment_type = shp::csr_matrix_view<
      T, I, rng::iterator_t<shp::device_vector<T, shp::device_allocator<T>>>,
      rng::iterator_t<shp::device_vector<I, shp::device_allocator<I>>>>;

  // using iterator = sparse_matrix_iterator<T, shp::device_vector<T,
  // shp::device_allocator<T>>>;
  using iterator =
      distributed_sparse_matrix_iterator<std::span<segment_type> &&>;

  sparse_matrix(key_type shape)
      : shape_(shape), partition_(new shp::block_cyclic()) {
    init_();
  }

  sparse_matrix(key_type shape, double density)
      : shape_(shape), partition_(new shp::block_cyclic()) {
    init_random_(density);
  }

  sparse_matrix(key_type shape, double density,
                const matrix_partition &partition)
      : shape_(shape), partition_(partition.clone()) {
    init_random_(density);
  }

  sparse_matrix(key_type shape, const matrix_partition &partition)
      : shape_(shape), partition_(partition.clone()) {
    init_();
  }

  size_type size() const noexcept { return total_nnz_; }

  key_type shape() const noexcept { return shape_; }

  iterator begin() { return iterator(segments(), 0, 0); }

  iterator end() {
    return iterator(segments(), grid_shape_[0] * grid_shape_[1], 0);
  }

  segment_type tile(key_type tile_index) {
    std::size_t tile_idx = tile_index[0] * grid_shape_[1] + tile_index[1];
    auto values = values_[tile_idx].begin();
    auto rowptr = rowptr_[tile_idx].begin();
    auto colind = rowptr_[tile_idx].begin();
    auto nnz = nnz_[tile_idx];

    size_t tm =
        std::min(tile_shape_[0], shape()[0] - tile_index[0] * tile_shape_[0]);
    size_t tn =
        std::min(tile_shape_[1], shape()[1] - tile_index[1] * tile_shape_[1]);

    return segment_type(values, rowptr, colind, key_type{tm, tn}, nnz,
                        values_[tile_idx].rank());
  }

  key_type tile_shape() const noexcept { return tile_shape_; }

  key_type grid_shape() const noexcept { return grid_shape_; }

  std::span<segment_type> tiles() { return std::span(tiles_); }

  std::span<segment_type> segments() { return std::span(segments_); }

private:
  std::vector<segment_type> generate_tiles_() {
    std::vector<segment_type> views_;

    for (size_t i = 0; i < grid_shape_[0]; i++) {
      for (size_t j = 0; j < grid_shape_[1]; j++) {
        size_t tm = std::min(tile_shape_[0], shape()[0] - i * tile_shape_[0]);
        size_t tn = std::min(tile_shape_[1], shape()[1] - j * tile_shape_[1]);

        std::size_t tile_idx = i * grid_shape_[1] + j;

        auto values = values_[tile_idx].begin();
        auto rowptr = rowptr_[tile_idx].begin();
        auto colind = colind_[tile_idx].begin();
        auto nnz = nnz_[tile_idx];

        views_.emplace_back(values, rowptr, colind, key_type{tm, tn}, nnz,
                            values_[tile_idx].rank());
      }
    }
    return views_;
  }

  std::vector<segment_type> generate_segments_() {
    std::vector<segment_type> views_;

    for (size_t i = 0; i < grid_shape_[0]; i++) {
      for (size_t j = 0; j < grid_shape_[1]; j++) {
        size_t tm = std::min(tile_shape_[0], shape()[0] - i * tile_shape_[0]);
        size_t tn = std::min(tile_shape_[1], shape()[1] - j * tile_shape_[1]);

        std::size_t tile_idx = i * grid_shape_[1] + j;

        auto values = values_[tile_idx].begin();
        auto rowptr = rowptr_[tile_idx].begin();
        auto colind = colind_[tile_idx].begin();
        auto nnz = nnz_[tile_idx];

        size_t m_offset = i * tile_shape_[0];
        size_t n_offset = j * tile_shape_[1];

        views_.emplace_back(values, rowptr, colind, key_type{tm, tn}, nnz,
                            values_[i * grid_shape_[1] + j].rank(),
                            key_type{m_offset, n_offset});
      }
    }
    return views_;
  }

private:
  void init_() {
    grid_shape_ = partition_->grid_shape(shape());
    tile_shape_ = partition_->tile_shape(shape());

    values_.reserve(grid_shape_[0] * grid_shape_[1]);
    rowptr_.reserve(grid_shape_[0] * grid_shape_[1]);
    colind_.reserve(grid_shape_[0] * grid_shape_[1]);
    nnz_.reserve(grid_shape_[0] * grid_shape_[1]);

    for (std::size_t i = 0; i < grid_shape_[0]; i++) {
      for (std::size_t j = 0; j < grid_shape_[1]; j++) {
        std::size_t rank = partition_->tile_rank(shape(), {i, j});

        auto device = shp::devices()[rank];
        shp::device_allocator<T> alloc(shp::context(), device);
        shp::device_allocator<I> i_alloc(shp::context(), device);

        values_.emplace_back(1, alloc, rank);
        rowptr_.emplace_back(2, i_alloc, rank);
        colind_.emplace_back(1, i_alloc, rank);
        nnz_.push_back(0);
        rowptr_.back()[0] = 0;
        rowptr_.back()[1] = 0;
      }
    }
    tiles_ = generate_tiles_();
    segments_ = generate_segments_();
  }

  void init_random_(double density) {
    grid_shape_ = partition_->grid_shape(shape());
    tile_shape_ = partition_->tile_shape(shape());

    values_.reserve(grid_shape_[0] * grid_shape_[1]);
    rowptr_.reserve(grid_shape_[0] * grid_shape_[1]);
    colind_.reserve(grid_shape_[0] * grid_shape_[1]);
    nnz_.reserve(grid_shape_[0] * grid_shape_[1]);

    for (std::size_t i = 0; i < grid_shape_[0]; i++) {
      for (std::size_t j = 0; j < grid_shape_[1]; j++) {
        std::size_t rank = partition_->tile_rank(shape(), {i, j});

        size_t tm = std::min(tile_shape_[0], shape()[0] - i * tile_shape_[0]);
        size_t tn = std::min(tile_shape_[1], shape()[1] - j * tile_shape_[1]);

        auto device = shp::devices()[rank];
        shp::device_allocator<T> alloc(shp::context(), device);
        shp::device_allocator<I> i_alloc(shp::context(), device);

        auto csr = generate_random_csr<T, I>({tm, tn});
        std::size_t nnz = csr.size();

        shp::device_vector<T, shp::device_allocator<T>> values(csr.size(),
                                                               alloc, rank);
        shp::device_vector<I, shp::device_allocator<I>> rowptr(tm + 1, i_alloc,
                                                               rank);

        shp::device_vector<I, shp::device_allocator<I>> colind(csr.size(),
                                                               i_alloc, rank);

        shp::copy(csr.values_data(), csr.values_data() + csr.size(),
                  values.data());
        shp::copy(csr.rowptr_data(), csr.rowptr_data() + tm + 1, rowptr.data());
        shp::copy(csr.colind_data(), csr.colind_data() + csr.size(),
                  colind.data());

        values_.push_back(std::move(values));
        rowptr_.emplace_back(std::move(rowptr));
        colind_.emplace_back(std::move(colind));
        nnz_.push_back(nnz);
        total_nnz_ += nnz;

        delete[] csr.values_data();
        delete[] csr.rowptr_data();
        delete[] csr.colind_data();
      }
    }
    tiles_ = generate_tiles_();
    segments_ = generate_segments_();
  }

private:
  key_type shape_;
  key_type grid_shape_;
  key_type tile_shape_;
  std::unique_ptr<shp::matrix_partition> partition_;

  std::vector<shp::device_vector<T, shp::device_allocator<T>>> values_;
  std::vector<shp::device_vector<I, shp::device_allocator<I>>> rowptr_;
  std::vector<shp::device_vector<I, shp::device_allocator<I>>> colind_;

  std::vector<std::size_t> nnz_;
  std::size_t total_nnz_ = 0;

  std::vector<segment_type> tiles_;
  std::vector<segment_type> segments_;
};

} // namespace shp
