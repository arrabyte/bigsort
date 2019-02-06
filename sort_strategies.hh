#pragma once

#include <seastar/core/reactor.hh>
#include <seastar/core/file.hh>
#include <seastar/core/sleep.hh>
#include <boost/program_options.hpp>
#include <boost/iterator/counting_iterator.hpp>
#include <iostream>
#include <memory>
#include "block.hh"

namespace sort_algorithm {

seastar::future<> internal_sort(seastar::sstring fname, size_t file_size, size_t mem_size, int parallelism);
seastar::future<> external_sort(seastar::sstring root_filename, int files_count);
seastar::future<> create_block_collections_from_file(blocks_vector& blocks, seastar::sstring fname, int blocks_offset = 0, int count = -1);

}
