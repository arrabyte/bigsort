#include <seastar/core/app-template.hh>
#include <seastar/core/reactor.hh>
#include <seastar/core/file.hh>
#include <seastar/core/sleep.hh>
#include <seastar/core/memory.hh>
#include <boost/program_options.hpp>
#include <boost/iterator/counting_iterator.hpp>
#include <iostream>
#include <chrono>

#include "../sort_strategies.hh"
#include "../file_utils.hh"
#include "../block.hh"

// generate 50GB of test pattern: test_pattern.big.sorted, test_pattern.big.unsorted
int main(int argc, char** argv)
{
    namespace bpo = boost::program_options;
    static int tpattern_index=0;
    static std::chrono::time_point<std::chrono::system_clock> start_time = std::chrono::system_clock::now();
    static int64_t towrite = (10LL*1024LL*1024LL*1024LL/block_size); // 10GB
    static int parallelism = 20;

    seastar::app_template app;

    boost::program_options::positional_options_description positional_opt;
    app.add_options()
    ("size", boost::program_options::value<size_t>()->default_value(10), "Size in GB of bigfile to be created");
    app.add_positional_options({
    { "filename", bpo::value<seastar::sstring>(),
        "path of the big file that will be created", -1}
    });

    app.run(argc, argv, [&app]{
        auto& args = app.configuration();
        if (!args.count("filename")){
            std::cout << "genbigfile filename" << '\n';
            return seastar::make_ready_future<>();
        }

        static seastar::sstring fname = (args["filename"].as<seastar::sstring>());
        towrite = (args["size"].as<size_t>()*1024*1024*1024)/block_size;
        if(towrite > block_size*block_size){
            std::cout << "Error: genbigfile can generate file of max size of " << (block_size^2)*block_size << " Bytes" << std::endl;
            return seastar::make_ready_future<>();
        }
        std::cout << "Start genbigfile " << fname << " of size:" << args["size"].as<size_t>() << " GB -- blocks:" << towrite << std::endl;

        return seastar::open_file_dma(fname + ".sorted", seastar::open_flags::rw|seastar::open_flags::create|seastar::open_flags::truncate)
        .then([](seastar::file f) {
            return seastar::open_file_dma(fname + ".unsorted" , seastar::open_flags::rw|seastar::open_flags::create|seastar::open_flags::truncate)
            .then([f](seastar::file fu) mutable {
                static seastar::semaphore write_semaphore(parallelism);
                return seastar::do_for_each(boost::counting_iterator<int64_t>(0),
                                    boost::counting_iterator<int64_t>(towrite),
                                    [f, fu](auto& block_i)  mutable {
                    std::bitset<4096> binstr(block_i);
                    std::string test_pattern = binstr.to_string();
                    datablock::blocks_ptr b = datablock::make_block(test_pattern.begin(), test_pattern.end());
                    datablock::blocks_ptr b2 = datablock::make_block(test_pattern.begin(), test_pattern.end());
                    return write_semaphore.wait(2) //wait must be multiple of 2 because inside the wait continuation two async calls will be done
                    .then([f, fu, b=std::move(b), b2=std::move(b2), block_i] () mutable {
                        // write sorted file
                        f.dma_write( block_i * block_size, b.get(), block_size).then(
                            [b=std::move(b), f](size_t ret) mutable {
                                static int_fast64_t written = 0;
                                written += ret;
                                if(written % (4096*4096) == 0) //flush every 4MB
                                {
                                    std::cout << written/1024/1024 << " Mbytes has been written" << std::endl;
                                    f.flush();
                                }
                        }).finally([f]{
                                write_semaphore.signal(1);
                        });

                        // write unsorted file
                        fu.dma_write( ((towrite-1) - block_i) * block_size, b2.get(), block_size).then(
                            [b2=std::move(b2), fu](size_t ret) mutable {
                                static int_fast64_t written = 0;
                                written += ret;
                                if(written % (4096*4096) == 0) //flush every 4MB
                                    fu.flush();
                        }).finally([fu]{
                                write_semaphore.signal(1);
                        });
                    });
                }).then([f, fu]() mutable {
                    std::cout << "big file have been done in "
                    <<  std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now() - start_time).count()
                    << "ms" << std::endl;
                    return write_semaphore.wait(parallelism).then([f,fu]()mutable{
                        return fu.flush().then([fu, f]()mutable{
                            return f.flush().then([fu, f]()mutable{
                                return f.close().then([fu, f]()mutable{
                                    return fu.close().finally([f,fu]{});
                                });
                            });
                        });
                    });
                });
            });
        }).then([]{}).handle_exception([](std::exception_ptr e) {
            try {
                if (e) {
                    std::rethrow_exception(e);
                }
            } catch(const std::exception& e) {
                std::cout << "Caught exception \"" << e.what() << "\"\n";
            }
        });
    });

    return 0;
}