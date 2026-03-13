#include "../include/register_file.hpp"

// Explicit instantiation — generate code for RV32 and RV64 in this TU only
template class RegisterFile<32>;
template class RegisterFile<64>;