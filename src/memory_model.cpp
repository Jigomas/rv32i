#include "../include/memory_model.hpp"

// Explicit instantiation — keeps link symbols in one TU for faster incremental builds
template class MemoryModel<32>;
template class MemoryModel<64>;