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
        ("mem", boost::program_options::value<size_t>()->default_value(1000), "Memory available for internal sort expressed in MB");
    boost::program_options::positional_options_description positional_opt;
    app.add_positional_options({
       { "filename", bpo::value<seastar::sstring>(),
         "file to be sorted", -1}
    });

    app.run(argc, argv, [&app]{
        auto& args = app.configuration();

        if (!args.count("filename")){
            std::cout << "bigsort filename" << '\n';
            return seastar::make_ready_future<>();
        }

        static datablock::blocks_vector blocks;
        static size_t free_mem = seastar::memory::stats().free_memory()/2;

        static seastar::sstring filename = (args["filename"].as<seastar::sstring>());
        static int file_index = 0;
        static uint64_t blocks_fetched = 0;
        static std::chrono::time_point<std::chrono::system_clock> start_time = std::chrono::system_clock::now();

        std::cout << "bigsort lexicographic sort of 4K blocks.\nAvailable memory for internal sort " << free_mem/1024/1024 << " Mb\nfile name " << filename << std::endl;

        return file_utils::read_blocks_from_file(filename, [](datablock::blocks_ptr &&block, uint64_t block_index, uint64_t blocks_tot){
            blocks.push_back(std::move(block));
            ++blocks_fetched;
            if(blocks.size() * block_size >= free_mem || blocks_fetched == blocks_tot){
                //sort and save to disk
                datablock::sort_blocks(blocks);
                return file_utils::write_blocks(blocks, filename + "." + std::to_string(++file_index))
                .then([blocks_tot]() mutable {
                    std::cout << "write " << blocks.size() << " blocks on disk -- file " <<  filename + "." + std::to_string(file_index) << std::endl;
                    std::vector<datablock::blocks_ptr>().swap(blocks); //delete memory blocks
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
                    return seastar::make_ready_future();
                });
            }
            return seastar::make_ready_future();
        }).then([]{}).handle_exception([](std::exception_ptr e) {
            handle_eptr(e);
        });
    });

    return 0;
}
