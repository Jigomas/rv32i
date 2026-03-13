#include "../include/alu.hpp"

// Explicit instantiation — generate code for RV32 and RV64 in this TU only
template class ALU<32>;
template class ALU<64>;