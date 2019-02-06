#include <seastar/core/reactor.hh>
#include <seastar/core/file.hh>
#include <seastar/core/sleep.hh>
#include <boost/iterator/counting_iterator.hpp>
#include <memory>

namespace file_utils {

seastar::future<> write_blocks(blocks_vector& blocks, seastar::sstring fname);

template <typename Act>
seastar::future<> read_blocks_from_file(seastar::sstring fname, Act action) {
    return seastar::open_file_dma(fname, seastar::open_flags::rw).then([action/*, this*/](seastar::file f) mutable {
        return f.size().then([action=std::move(action), /*this,*/ f](size_t size) mutable {
            int count = size / block_size;
            //blocks_count = count;
            return seastar::do_with(std::move(f),[action=std::move(action), /*this,*/ count](auto &f) mutable {
                return seastar::do_for_each(
                        boost::counting_iterator<uint32_t>(0),
                        boost::counting_iterator<uint32_t>(count),
                        [&f, action=std::move(action), count/*, this*/](auto &i) mutable {

                    //offset = i;
                    auto rbuf = seastar::allocate_aligned_buffer<unsigned char>(block_size, block_size);
                    int pos = i*block_size;
                    auto rb = rbuf.get();
                    return f.dma_read(pos, rb, block_size)
                    .then([i, count, rbuf = std::move(rbuf), pos, action=std::move(action)](size_t ret) mutable {
                        return seastar::futurize_apply(std::move(action),
                                                       std::move(rbuf),
                                                       std::move(i),
                                                       std::move(count))
                        .finally([action=std::move(action)](){});
                    });
                });
            });
        });
    });
}

seastar::future<> write_blocks(blocks_vector& blocks, seastar::sstring fname) {
    return seastar::open_file_dma(fname,seastar::open_flags::rw|seastar::open_flags::create|seastar::open_flags::truncate)
    .then([&blocks](seastar::file f) mutable {
        return seastar::do_for_each(boost::counting_iterator<uint32_t>(0),
                                    boost::counting_iterator<uint32_t>(blocks.size()),
                                    [f, &blocks](auto& i) mutable {
            auto wb = blocks[i].get();
            f.dma_write(i * block_size, wb, block_size);
        }).then([f]() mutable {
            return f.flush().finally([f]{});
        });
    });
}

}
