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

#pragma once

#include <seastar/core/reactor.hh>
#include <seastar/core/memory.hh>
#include <memory>
#include <vector>

#include <seastar/core/memory.hh>
#include <iterator>

extern int const block_size;

namespace datablock{

using blocks_data = unsigned char[];
using blocks_ptr = std::unique_ptr<blocks_data,seastar::free_deleter>;
using blocks_vector = std::vector<std::unique_ptr<unsigned char[],seastar::free_deleter>>;

void sort_blocks(blocks_vector &blocks);

template<class InputIt>
blocks_ptr make_block(InputIt start_sequence, InputIt end_sequence){
    blocks_ptr tmp(std::move(seastar::allocate_aligned_buffer<unsigned char>(block_size, block_size)));
    auto *ptr = tmp.get();
    std::copy(start_sequence, std::min(end_sequence, start_sequence+block_size) , ptr);
    return tmp; //implicit move
}

}
