#include <seastar/core/app-template.hh>
#include <seastar/core/reactor.hh>
#include <seastar/core/file.hh>
#include <seastar/core/sleep.hh>
#include <seastar/core/memory.hh>
#include <boost/program_options.hpp>
#include <boost/iterator/counting_iterator.hpp>
#include <iostream>
#include <chrono>

#include "sort_strategies.hh"
#include "file_utils.hh"
#include "block.hh"


void handle_eptr(std::exception_ptr eptr)
{
    try {
        if (eptr) {
            std::rethrow_exception(eptr);
        }
    } catch(const std::exception& e) {
        std::cout << "Caught exception \"" << e.what() << "\"\n";
    }
}

int main(int argc, char** argv) {
    namespace bpo = boost::program_options;
    seastar::app_template app;
    app.add_options()
        ("mem", boost::program_options::value<size_t>()->default_value(0x40000000), "Memory available for internal sort");
    boost::program_options::positional_options_description positional_opt;
    app.add_positional_options({
       { "filename", bpo::value<seastar::sstring>(),
         "file to be sorted", -1}
    });

    app.run(argc, argv, [&app]{
        auto& args = app.configuration();

        // memory available to load file inside memory blocks
        const size_t memory_blocks = args["mem"].as<size_t>()/block_size*block_size;

        if (!args.count("filename")){
            std::cout << "bigsort filename" << '\n';
            return seastar::make_ready_future<>();
        }

        static blocks_vector blocks;
        static size_t free_mem = std::min(seastar::memory::stats().free_memory(), memory_blocks);
        static seastar::sstring filename = (args["filename"].as<seastar::sstring>());
        static int file_index = 0;
        static int blocks_fetched = 0;
        static std::chrono::time_point<std::chrono::system_clock> start_time = std::chrono::system_clock::now();

        std::cout << "Scyllatest lexicographic sort of 4K blocks.\nAvailable memory " << free_mem/1024/1024 << " Mb\nfile size " << filename << std::endl;

        return file_utils::read_blocks_from_file(filename, [](blocks_ptr &&block, int block_index, int blocks_tot){
            blocks.push_back(std::move(block));
            ++blocks_fetched;
            if(blocks.size() * block_size >= free_mem || blocks_fetched == blocks_tot){
                //sort, save to disk and run sort on next block array
                sort_blocks(blocks);
                return file_utils::write_blocks(blocks, filename + "." + std::to_string(++file_index))
                .then([blocks_tot]() mutable {
                    std::cout << "write " << blocks.size() << " blocks on disk -- file " <<  filename + "." + std::to_string(file_index) << std::endl;
                    blocks.clear(); //delete memory blocks

                    if(blocks_fetched == blocks_tot)
                    {
                        std::cout << "internal sort done in "
                                  <<  std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now() - start_time).count()
                                  << "ms" << std::endl;
                        start_time = std::chrono::system_clock::now();
                        return sort_algorithm::external_sort(filename, file_index).then([]{
                            std::cout << "externa sort sort done in "
                                        <<  std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now() - start_time).count()
                                        << "ms" << std::endl;
                            return seastar::make_ready_future();
                        });
                    }                    
                });
            }
            return seastar::make_ready_future();
        }).then([]{}).handle_exception([](std::exception_ptr e) {
            handle_eptr(e);
        });
    });


    return 0;
}
