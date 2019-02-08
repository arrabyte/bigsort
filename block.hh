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
    assert(end_sequence - start_sequence <= block_size);

    auto *ptr = tmp.get();
    std::copy(start_sequence, end_sequence, ptr);
    return tmp; //implicit move
}

}
