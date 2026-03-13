#include "../include/decoder.hpp"

// Explicit instantiation — generate code for RV32 and RV64 in this TU only
template class Decoder<32>;
template class Decoder<64>;
template struct DecodedInstr<32>;
template struct DecodedInstr<64>;