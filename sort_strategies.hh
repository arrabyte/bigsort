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

seastar::future<> external_sort(seastar::sstring root_filename, int files_count);

}
