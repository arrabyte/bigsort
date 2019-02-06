#include "sort_strategies.hh"
#include <seastar/core/thread.hh>
#include <exception>
#include <algorithm>
#include <functional>

namespace sort_algorithm {

seastar::future<> internal_sort(seastar::sstring fname, size_t file_size, size_t mem_size, int parallelism) {
    return seastar::open_file_dma(fname,seastar::open_flags::rw)
    .then([mem_size, parallelism, file_size](seastar::file file){
        auto f = seastar::make_lw_shared<seastar::file>(std::move(file));
        int readable_size = std::min(mem_size, file_size);
        int blocks_to_read = readable_size / block_size;
        std::cout << "file size:" << file_size << " loadable blocks:" << blocks_to_read << std::endl;
        // read from file
        std::vector<std::unique_ptr<unsigned char[],seastar::free_deleter>> blocks;

        return seastar::do_with(std::move(blocks), std::move(blocks_to_read), [f](auto &blocks, auto &blocks_to_read) {
            return seastar::do_for_each(
                    boost::counting_iterator<uint32_t>(0),
                    boost::counting_iterator<uint32_t>(blocks_to_read),
                    [f, &blocks, &blocks_to_read](auto &i) mutable {

                    if (i == 0) { return seastar::make_ready_future<>(); }
                    std::cout << "loop:" << i << std::endl;

                    auto rbuf = seastar::allocate_aligned_buffer<unsigned char>(block_size, block_size);
                    return f->dma_read(i*block_size, rbuf.get(), block_size)
                    .then([rbuf = std::move(rbuf), &blocks](size_t ret) mutable {
                        std::cout << "dma_read [" << ret << "/4096] bytes" << std::endl;
                        blocks.push_back(std::move(rbuf));
                        return seastar::make_ready_future<>();
                    });
                }
            )
            .then( [&blocks]{
                int i = 0;
                for(auto &x:blocks){
                    std::string s(reinterpret_cast<const char*>(x.get()));
                    std::cout << "block[" << i++ << "] data" << s.substr(0,100) << std::endl;
                }
                return seastar::make_ready_future<>();
            } )
            .finally([]{
                std::cout << "end foreach loop" << std::endl;
            });
        });
    });
}

// todo. a constructor of blocks_collection blocks_collection( file)
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

// external_sort operate by comparing just two blocks at each step, reading blocks from disk.
// only current block should be stored in memory.
// A list of current block index position iniside any file is neeeded to be keept in memory.
seastar::future<> external_sort(seastar::sstring root_filename, int files_count)
{
    struct disk_block_reader
    {
        disk_block_reader(seastar::file&& f, uint32_t findex, uint32_t num_of_blocks):
            file(std::move(f)),
            file_index(findex),
            block_index(0),
            number_of_blocks(num_of_blocks){}

        seastar::file file;
        uint32_t file_index;
        uint32_t block_index;
        uint32_t number_of_blocks;
    };

    // create the out file to write the ordered blocks sequence
    return seastar::open_file_dma(root_filename + ".sorted", seastar::open_flags::rw|seastar::open_flags::create|seastar::open_flags::truncate)
    .then([files_count, root_filename=std::move(root_filename)](seastar::file of){

        blocks_ptr current_min_block;
        int current_min_file_index = -1;
        std::vector<disk_block_reader> blocks_readers;
        int i(0);
        return seastar::do_with(std::move(blocks_readers), std::move(current_min_block), std::move(current_min_file_index), std::move(of), seastar::semaphore(10),std::move(i),
                                [files_count=std::move(files_count), root_filename=std::move(root_filename)](
                                    auto& blocks_readers, auto &current_min_block, auto &current_min_file_index, auto &of, auto &limit, auto &i) mutable {
            // open the files of the set containing sorted blocks
            // and initialize the blocks_readers.
            return seastar::do_for_each(boost::counting_iterator<uint32_t>(1),
                                        boost::counting_iterator<uint32_t>(files_count+1),
                                        [root_filename=std::move(root_filename), &blocks_readers, &current_min_block, &of](auto& k) mutable {
                return seastar::open_file_dma(root_filename + "." + std::to_string(k), seastar::open_flags::rw)
                .then([&blocks_readers, k, &current_min_block](seastar::file f) mutable {
                    return f.size().then([&blocks_readers, f, k](size_t size) mutable {
                        blocks_readers.push_back(std::move(disk_block_reader(std::move(f), k, size/block_size)));
                        return seastar::make_ready_future();
                    });
                });
            }).then([files_count=std::move(files_count), &blocks_readers, &current_min_block, &current_min_file_index, &of, &limit, &i]() mutable {
                // merge files sorting element at each step.

                auto funtil = [&blocks_readers]{
                    for(auto &x:blocks_readers)
                    {
                        if(x.block_index < x.number_of_blocks)
                            return false;
                    }
                    return true;
                };

                return seastar::do_until( /*[&blocks_readers]{return blocks_readers.empty();}*/funtil, [&blocks_readers, &current_min_block, &current_min_file_index, &of, &limit, &i]() mutable {
                    return seastar::do_for_each(blocks_readers, [&current_min_block, &current_min_file_index, &i, &of, &limit] (auto& el) mutable {
                        // provide its own function
                        // min(a, b , store)
                        if(el.block_index == el.number_of_blocks)
                            return seastar::make_ready_future<>();

                        auto rbuf = seastar::allocate_aligned_buffer<unsigned char>(block_size, block_size);
                        auto rb = rbuf.get();
                        return el.file.dma_read(el.block_index * block_size, rb, block_size)
                        .then([rbuf=std::move(rbuf), &current_min_block, &current_min_file_index, &el](size_t ret) mutable {
                            // suppouse that current min value is lower than the new value
                            auto first = current_min_block.get();
                            auto last = rbuf.get();
                            if(!current_min_block || !std::lexicographical_compare(first, first + block_size, last, last + block_size) ){
                                // get ownership of new min and delete previous
                                current_min_block = std::move(rbuf);
                                current_min_file_index = el.file_index;
                            }
                        });
                    }).then([&blocks_readers, &current_min_file_index, &current_min_block, &of, &i, &limit]{
                        return limit.wait(1).then([&blocks_readers, &current_min_file_index, &current_min_block, &of, &i, &limit] {
                            // current_min_block has the min of the iteration
                            blocks_readers[current_min_file_index-1].block_index++;
                            current_min_file_index = -1;

                            // write to out file
                            auto wb = current_min_block.get();
                            return of.dma_write(i++*block_size, wb, block_size).then([&limit, &current_min_block](size_t ret){
                                current_min_block.reset();
                                limit.signal(1);
                                return seastar::make_ready_future();
                            });
                        });
                    });
                }).then([&limit] { return limit.wait(4); });
            });
        }).then([]{});
    });
}

} // end namescpace sort algo