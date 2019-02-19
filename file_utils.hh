/*
 * Copyright (C) 2019 Alessandro Arrabito
 */

/*
 * This file is part of bigsort.
 *
 * bigsort is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * bigsort is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with bigsort.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <seastar/core/reactor.hh>
#include <seastar/core/file.hh>
#include <boost/iterator/counting_iterator.hpp>
#include "block.hh"
#include <memory>

namespace file_utils {

using namespace datablock;

template <typename Action>
seastar::future<> read_blocks_from_file(seastar::sstring fname, Action action) {
    return seastar::open_file_dma(fname, seastar::open_flags::rw)
    .then([action=std::move(action)](seastar::file f) mutable {
        return f.size().then([action=std::move(action), f](size_t size) mutable {
            int count = size / block_size;
            return seastar::do_for_each(
                    boost::counting_iterator<uint32_t>(0),
                    boost::counting_iterator<uint32_t>(count),
                    [f, action=std::move(action), count](auto &i) mutable {

                auto rbuf = seastar::allocate_aligned_buffer<unsigned char>(block_size, block_size);
                size_t pos = static_cast<size_t>(i) * static_cast<size_t>(block_size);
                auto rb = rbuf.get();
                return f.dma_read(pos, rb, block_size)
                .then([i, count, rbuf = std::move(rbuf), pos, action=std::move(action),f](size_t ret) mutable {
                    return seastar::futurize_apply(std::move(action),
                                                    std::move(rbuf),
                                                    std::move(i),
                                                    std::move(count))
                    .finally([f](){});
                });
            });
        });
    });
}

seastar::future<> write_blocks(blocks_vector& blocks, seastar::sstring fname) {
    return seastar::open_file_dma(fname,seastar::open_flags::rw|seastar::open_flags::create|seastar::open_flags::truncate)
    .then([&blocks](seastar::file f) mutable {
        return seastar::do_with(seastar::semaphore(10), [f, &blocks](auto &semaphore) mutable {
            return seastar::do_for_each(boost::counting_iterator<uint32_t>(0),
                                        boost::counting_iterator<uint32_t>(blocks.size()),
                                        [f, &blocks, &semaphore](auto& i) mutable {
                return semaphore.wait(1).then([f, &blocks, i, &semaphore]() mutable {
                    auto wb = blocks[i].get();
                    f.dma_write(i * block_size, wb, block_size).then([f, &semaphore](auto ret){
                        // check size
                    }).finally([&semaphore] { semaphore.signal(1); });
                });
            }).then([f, &semaphore]() mutable {
                return semaphore.wait(10).then([f]() mutable{
                    return f.flush().then([f]()mutable{
                        return f.close().finally([f] {});
                    });
                });
            });
        });
    });
}

seastar::future<> create_block_collections_from_file(blocks_vector& blocks, seastar::sstring fname, int blocks_offset, int count) {
    return seastar::open_file_dma(fname,seastar::open_flags::rw)
    .then([count, &blocks, blocks_offset,fname](seastar::file f) mutable {
        return f.size().then([f, count, &blocks, blocks_offset,fname](size_t file_size) mutable {
            count = count == -1 ? file_size/block_size : std::min(file_size/block_size, static_cast<size_t>(count));
            return seastar::do_for_each(
                    boost::counting_iterator<uint32_t>(blocks_offset),
                    boost::counting_iterator<uint32_t>(count),
                    [f, &blocks, &count, &blocks_offset](auto &i) mutable {
                    auto rbuf = seastar::allocate_aligned_buffer<unsigned char>(block_size, block_size);
                    int pos = i*block_size + blocks_offset * block_size;
                    auto rb = rbuf.get();
                    return f.dma_read(pos, rb, block_size)
                    .then([rbuf = std::move(rbuf), &blocks, pos](size_t ret) mutable {
                        blocks.push_back(std::move(rbuf));
                        return seastar::make_ready_future<>();
                });
            });
        });
    });
}

}
