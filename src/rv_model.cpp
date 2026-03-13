#include "../include/rv_model.hpp"

// Explicit instantiation — keeps link symbols in one TU for faster incremental builds
template class RVModel<32>;
template class RVModel<64>;