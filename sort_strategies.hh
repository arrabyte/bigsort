#pragma once

#include <seastar/core/sstring.hh>
#include <seastar/core/future.hh>
#include "block.hh"

namespace sort_algorithm {

seastar::future<> external_sort(seastar::sstring root_filename, int files_count);

}
