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

#define SEASTAR_TESTING_MAIN

#include <seastar/core/app-template.hh>
#include <seastar/testing/test_case.hh>
#include <seastar/core/reactor.hh>
#include <seastar/core/shared_ptr.hh>
#include <seastar/core/do_with.hh>
#include <iostream>
#include "../sort_strategies.hh"
#include "../file_utils.hh"
#include "../block.hh"

const std::string pattern_dir(TEST_PATTERN_DIR);

static std::vector<seastar::sstring> test_pattern_unsorted({
    "External sorting is a class of sorting algorithms",
    "25",
    "123456789",
    "12345678",
    "Is required when the data being sorted do not fit into the main memory of a computing device",
    "External sorting is a class of sorting algorithms that can handle massive amounts of data.",
    "not fit into the main memory",
    " (usually RAM) and instead they must reside in the slower external memory",
    "slower external memory, usually a hard disk drive",
    "Thus, external sorting algorithms are external mem algorithms",
    "algorithms and thus applicable in the external mem"});

static std::vector<seastar::sstring> test_pattern_sorted({
    " (usually RAM) and instead they must reside in the slower external memory",
    "12345678",
    "123456789",
    "25",
    "External sorting is a class of sorting algorithms",
    "External sorting is a class of sorting algorithms that can handle massive amounts of data.",
    "Is required when the data being sorted do not fit into the main memory of a computing device",
    "Thus, external sorting algorithms are external mem algorithms",
    "algorithms and thus applicable in the external mem",
    "not fit into the main memory",
    "slower external memory, usually a hard disk drive"});

static std::vector<seastar::sstring> test_pattern_sorted_set({
    // sorted set_0
    "12345678",
    "123456789",
    "25",
    "External sorting is a class of sorting algorithms",
    // sorted set_1
    " (usually RAM) and instead they must reside in the slower external memory",
    "External sorting is a class of sorting algorithms that can handle massive amounts of data.",
    "Is required when the data being sorted do not fit into the main memory of a computing device",
    "not fit into the main memory",
    // sorted set_2
    "Thus, external sorting algorithms are external mem algorithms",
    "algorithms and thus applicable in the external mem",
    "slower external memory, usually a hard disk drive"});

void test_handle_exception(std::exception_ptr e, int line) {
    try {
        if (e)
            std::rethrow_exception(e);

    } catch(const std::exception& e) {
        std::cerr << "Caught exception \"" << e.what() << "\"\n";
    }
    std::cerr << "Exception invoked from line " << line << std::endl;
    BOOST_TEST(false);
}
#define TEST_HANDLE_EXCEPTION test_handle_exception(e, __LINE__)

using namespace sort_algorithm;
using namespace file_utils;

// write a test patter of 4k blocks
// | block 0 | block 1 |...| block n |
seastar::future<> write_test_pattern(seastar::sstring fname,
                                     int start_pos = 0,
                                     int end_pos = test_pattern_unsorted.size(),
                                     const std::vector<seastar::sstring>& test_pattern = test_pattern_unsorted) {
    return seastar::open_file_dma(fname, seastar::open_flags::rw|seastar::open_flags::create|seastar::open_flags::truncate)
    .then([start_pos, end_pos, &test_pattern](seastar::file f) {
        return seastar::do_for_each(
            boost::counting_iterator<uint32_t>(start_pos),
            boost::counting_iterator<uint32_t>(end_pos),
            [f, &test_pattern, start_pos](auto& i) mutable {
                const auto alignment = f.disk_write_dma_alignment();
                auto wbuf_ptr = seastar::allocate_aligned_buffer<char>(block_size, block_size);
                auto wb = wbuf_ptr.get();
                std::fill(wb, wb + block_size,0);
                std::copy(test_pattern[i].begin(), test_pattern[i].end(), wb);
                return f.dma_write( (i - start_pos) * block_size, wb, block_size).then(
                    [i, wbuf_ptr=std::move(wbuf_ptr)](size_t ret){
                        BOOST_REQUIRE(ret == 4096);
                        return seastar::make_ready_future<>();
                    });
            }).then([f]() mutable {
                return f.flush().finally([f]{});
            });
    });
}

SEASTAR_TEST_CASE( test_internal_sort ) {
    static seastar::sstring fname(pattern_dir  + "/blocks_test_pattern");
    static blocks_vector blocks;
    static size_t free_mem = 4096*4; //4 blocks can be stored in memory
   static bool check_done = false;

    return write_test_pattern(fname).then([]() mutable {
        return file_utils::read_blocks_from_file(fname, [](blocks_ptr &&block, int block_index, int blocks_tot){
            blocks.push_back(std::move(block));
            if(blocks.size() * block_size >= free_mem){
                //sort, save to disk and run sort on next block array
                sort_blocks(blocks);
                // check sorted subset
                static int i = 0;
                for(auto &x:blocks){
                    BOOST_REQUIRE(std::equal(x.get(),x.get() + test_pattern_sorted_set[i].size(), test_pattern_sorted_set[i].begin()));
                    i++;
                }
                blocks.clear();
            }
        });
    }).handle_exception([](std::exception_ptr e) {
        TEST_HANDLE_EXCEPTION;
    });
    BOOST_REQUIRE(check_done);
}

SEASTAR_TEST_CASE( test_external_sort ) {
    static seastar::sstring fname(pattern_dir  + "/blocks_test_pattern");
    static bool check_done = false;
    // write sequentially the test pattern: test_pattern_sorted_set
    // that consists in three set of ordered pattern
    // to reproduce the precondition at the end of internal sort
    return write_test_pattern(fname + ".1", 0, 4, test_pattern_sorted_set).then([]{
        return write_test_pattern(fname + ".2", 4, 8, test_pattern_sorted_set).then([]{
            return write_test_pattern(fname + ".3", 8, 11, test_pattern_sorted_set).then([]{
                return external_sort(fname, 3).then([]{
                    // load sorted file and
                    blocks_vector blocks;
                    return seastar::do_with(std::move(blocks), [](auto &blocks) {
                        return read_blocks_from_file(fname + ".sorted", [](blocks_ptr &&x, int block_index, int blocks_tot){
                            BOOST_REQUIRE(std::equal(x.get(),x.get() + test_pattern_sorted[block_index].size(), test_pattern_sorted[block_index].begin()));
                            check_done = true;
                            return seastar::make_ready_future<>();
                        });
                    });
                });
            });
        });
    }).handle_exception([](std::exception_ptr e) {
        TEST_HANDLE_EXCEPTION;
    });
    BOOST_REQUIRE(check_done);
}

// test read_blocks_from_file that read block of 4k from a file and call action at every step
SEASTAR_TEST_CASE(test_read_blocks_from_file) {
    static seastar::sstring fname(pattern_dir  + "/test_pattern");
    return write_test_pattern(fname).then([]{
        return read_blocks_from_file(fname, [](blocks_ptr &&x, int block_index, int blocks_tot){
            BOOST_REQUIRE(std::equal(x.get(),x.get() + test_pattern_unsorted[block_index].size(), test_pattern_unsorted[block_index].begin()));
        });
    }).handle_exception([](std::exception_ptr e) {
        TEST_HANDLE_EXCEPTION;
    });
}
