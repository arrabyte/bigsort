#pragma once

#include <seastar/core/reactor.hh>
#include <seastar/core/memory.hh>
#include <memory>
#include <vector>

extern const int block_size;

using blocks_data = unsigned char[];
using blocks_ptr = std::unique_ptr<blocks_data,seastar::free_deleter>;
using blocks_vector = std::vector<std::unique_ptr<unsigned char[],seastar::free_deleter>>;

void sort_blocks(blocks_vector &blocks);