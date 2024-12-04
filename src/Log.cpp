#include "Log.hpp"


namespace out {

int level = 3;

__NullBuffer __null_buffer{};
std::ostream __null_stream{&__null_buffer};

} // namespace out