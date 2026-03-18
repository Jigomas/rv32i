#include "../include/decoder.hpp"

template class Decoder<32>;
template class Decoder<64>;
template struct DecodedInstr<32>;
template struct DecodedInstr<64>;