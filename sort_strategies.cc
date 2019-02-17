#include "sort_strategies.hh"
#include <seastar/core/thread.hh>
#include <boost/iterator/counting_iterator.hpp>
#include <exception>
#include <algorithm>
#include <functional>
#include <iostream>

namespace sort_algorithm {

const int file_index_uninited(-1);

struct disk_block_reader
{
    disk_block_reader(seastar::file&& f, uint32_t findex, uint32_t num_of_blocks):
        file(std::move(f)),
        file_index(findex),
        block_index(0),
        number_of_blocks(num_of_blocks){}

    bool is_hexausted() {
        return block_index == number_of_blocks;
    };

    seastar::file file;
    uint32_t file_index;
    uint32_t block_index;
    uint32_t number_of_blocks;
    datablock::blocks_ptr cached_block;
};

struct external_sort_info
{
    external_sort_info():current_min_file_index(file_index_uninited){}
    int current_min_file_index;
    std::vector<disk_block_reader> blocks_readers;
};

seastar::future<> update_cached_block(disk_block_reader& block_reader){

    // get block data from cache if available
    if(!block_reader.cached_block){
        datablock::blocks_ptr rbuf = seastar::allocate_aligned_buffer<unsigned char>(block_size, block_size);
        return block_reader.file.dma_read(block_reader.block_index * block_size, rbuf.get(), block_size)
        .then([rbuf=std::move(rbuf), &block_reader](size_t ret) mutable {
            if(ret < block_size){
                std::cout << " Read error read " << ret << "bytes expected size is " << block_size << std::endl;
            }

            block_reader.cached_block = std::move(rbuf);
            return seastar::make_ready_future();
        });
    }
    return seastar::make_ready_future();
};

// External_sort operate by reading blocks from the head of any file involved that ware previously sorted by internal sort algo.
// Two blocks at time are compared to keep the min value that is stored in memory at each step.
// To perform the algo is needed a list of current block index position iniside any file.
// When the index position of a file reach the number of blocks inside the file, the file is considered hexausted.
// The algo stop when all files are hexausted.
seastar::future<> external_sort(seastar::sstring root_filename, int files_count)
{
    // create the out file to write the ordered blocks sequence
    return seastar::open_file_dma(root_filename + ".sorted", seastar::open_flags::rw|seastar::open_flags::create|seastar::open_flags::truncate)
    .then([files_count, root_filename=std::move(root_filename)](seastar::file of){

        external_sort_info sort_info;
        size_t block_ndx = 0;
        return seastar::do_with(std::move(sort_info), std::move(of), std::move(block_ndx),
                                [files_count=std::move(files_count), root_filename=std::move(root_filename)](
                                    auto& sort_info, auto &of, auto &block_ndx) mutable {
            // open the files of the set containing sorted blocks
            // and initialize the blocks_readers.
            return seastar::do_for_each(boost::counting_iterator<uint32_t>(1),
                                        boost::counting_iterator<uint32_t>(files_count+1),
                                        [root_filename=std::move(root_filename), &sort_info, &of](auto& file_ndx) mutable {
                return seastar::open_file_dma(root_filename + "." + std::to_string(file_ndx), seastar::open_flags::rw)
                .then([&sort_info, file_ndx](seastar::file f) mutable {
                    return f.size().then([&sort_info, f, file_ndx](size_t size) mutable {
                        // handle file size not multiple of block size by size%block_size==0
                        sort_info.blocks_readers.emplace_back(
                            disk_block_reader(std::move(f), file_ndx, size/block_size + (size%block_size==0?0:1)));
                        return seastar::make_ready_future();
                    });
                });
            }).then([files_count=std::move(files_count), &sort_info, &of, &block_ndx]() mutable {
                // merge files sorting element at each step.
                auto sort_is_done = [&sort_info]{ // todo: optimize removing hexausted element (better unordered_map instead of vector)
                    for(auto &x:sort_info.blocks_readers)
                    {
                        if(!x.is_hexausted())
                            return false;
                    }

                    return true;
                };

                return seastar::do_until(sort_is_done, [&sort_info, &of, &block_ndx]() mutable {
                    return seastar::do_for_each(sort_info.blocks_readers, [&sort_info, &block_ndx, &of] (auto& el) mutable {
                        if(el.is_hexausted())
                            return seastar::make_ready_future<>();

                        return update_cached_block(el).then([&sort_info, &el]() mutable {
                            auto first = sort_info.current_min_file_index != file_index_uninited ?
                                         sort_info.blocks_readers[sort_info.current_min_file_index-1].cached_block.get():
                                         nullptr;

                            // suppose that current min value is lower than the new value
                            auto last = el.cached_block.get();
                            if(!first || !std::lexicographical_compare(first, first + block_size, last, last + block_size))
                                    sort_info.current_min_file_index = el.file_index;

                            return seastar::make_ready_future();
                        });
                    }).then([&sort_info, &of, &block_ndx]{
                        // current_min_block has the min of the iteration
                        const int pos = sort_info.current_min_file_index-1;
                        sort_info.blocks_readers[pos].block_index++;

                        // write to out file
                        auto wb = sort_info.blocks_readers[pos].cached_block.get();
                        return of.dma_write(block_ndx++*block_size, wb, block_size).then([&sort_info, &of, &block_ndx, pos](size_t ret){
                            sort_info.blocks_readers[pos].cached_block.reset();
                            sort_info.current_min_file_index = file_index_uninited;

                            static int_fast64_t written = 0;
                            written += ret;
                            if(written % (4096*4096) == 0) //flush every 4MB
                            {
                                std::cout << written/1024/1024 << " Mbytes has been written -- write n." << block_ndx << std::endl;
                                return of.flush();
                            }else
                                return seastar::make_ready_future();
                        });
                    });
                });
            }).then([&of]() mutable{
                return of.flush().finally([&of]()mutable{
                    std::cout << "flush and close output file" << std::endl;
                    of.close();
                });
            });
        });
    });
}

} // end namescpace sort algo