#pragma once
#include <cstdint>

using Word   = uint32_t;  // machine word
using SWord  = int32_t;   // signed machine word
using DWord  = uint64_t;  // double machine word
using SDWord = int64_t;   // signed double machine word
using Addr   = uint32_t;  // address

using ByteT       = uint8_t;
using HalfT       = uint16_t;
using WordT       = uint32_t;
using DoubleWordT = uint64_t;

static_assert(sizeof(Word) == 4, "Word must be 4 bytes");
static_assert(sizeof(SWord) == 4, "SWord must be 4 bytes");
static_assert(sizeof(DWord) == 8, "DWord must be 8 bytes");
static_assert(sizeof(Addr) == 4, "Addr must be 4 bytes");