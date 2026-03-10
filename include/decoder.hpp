#pragma once
#include <string>

#include "types.hpp"

struct DecodedInstr {
    uint8_t opcode;  // [6:0]  — код операции
    uint8_t rd;      // [11:7] — регистр-назначение
    uint8_t rs1;     // [19:15]— регистр-источник 1
    uint8_t rs2;     // [24:20]— регистр-источник 2
    uint8_t funct3;  // [14:12]— поле функции 3 бита
    uint8_t funct7;  // [31:25]— поле функции 7 бит
    SWord imm;  // Знакорасширенный immediate (формат sssзависит от opcode)
    Word raw;  // Сырое слово инструкции

    std::string toString() const;  // Форматированный вывод для отладки
};

class Decoder {
public:
    static DecodedInstr decode(Word raw);
};