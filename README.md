# RV32I CPU Model

Программная модель процессора RISC-V RV32I на C++17.

---

## Возможности

- Полный набор инструкций RV32I (кроме `ecall`, `ebreak`, `fence.i`)
- Опциональное расширение **M** — целочисленное умножение и деление
- Little-endian плоская модель памяти
- Регистр x0 жёстко привязан к нулю
- Правило 5 на всех классах
- `assert` на все инварианты в Debug-сборке
- Исключения при ошибках времени выполнения
- Режим трассировки инструкций

---

## Структура проекта

```
rv32i/
├── include/
│   ├── types.hpp           # Word, SWord, ByteT, HalfT, WordT, Addr
│   ├── config.hpp          # XLEN, флаги расширений (EXT_M и др.)
│   ├── isa.hpp             # Опкоды, funct3/7, геттеры полей, декодеры immediate
│   ├── memory_model.hpp    # MemoryModel — плоская байтовая память
│   ├── register_file.hpp   # RegisterFile — 32 регистра общего назначения
│   ├── rv_model.hpp        # RVModel — ядро процессора, PC живёт здесь
│   ├── alu.hpp             # ALU — статическая утилита без состояния
│   ├── decoder.hpp         # Decoder — декодирование 32-битных инструкций
│   └── instr_builder.hpp   # InstrBuilder — сборка инструкций из полей
├── src/
│   ├── memory_model.cpp
│   ├── register_file.cpp
│   ├── rv_model.cpp
│   ├── alu.cpp
│   ├── decoder.cpp
│   └── main.cpp
├── tests/
│   └── test.cpp
├── CMakeLists.txt
├── .clang-format
├── .gitignore
└── README.md
```

---

## Требования

| Инструмент | Версия     |
|------------|------------|
| CMake      | ≥ 3.16     |
| GCC        | ≥ 9        |
| Clang      | ≥ 10       |
| MSVC       | ≥ 2019     |
| C++        | 17         |

---

## Сборка и запуск

### Структура директорий

Перед сборкой убедитесь, что файлы разложены по папкам:

```
rv32i/
├── include/        - все .hpp файлы
├── src/            - все .cpp файлы кроме test.cpp
├── tests/
│   └── test.cpp
└── CMakeLists.txt
```

### Debug (ASan + UBSan + assert)

```bash
cmake -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build -j$(nproc)
```

### Release (оптимизирован, assert выключены)

```bash
cmake -B build-release -DCMAKE_BUILD_TYPE=Release
cmake --build build-release -j$(nproc)
```

### Запуск демо

```bash
./build/rv32i_cpu
```

### Пересборка с нуля

```bash
rm -rf build
cmake -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build -j$(nproc)
```

### Запуск тестов

```bash
./build/rv32i_tests
```

### Запуск тестов через CTest

```bash
cd build && ctest --output-on-failure
```

Ожидаемый вывод:

```
===========================================
  RV32I CPU — Test Suite
===========================================

[ MemoryModel ]
  PASS  write/readByte
  PASS  write/readHalf
  ...

===========================================
  63 passed,  0 failed
===========================================
```

---

## Пример использования

```cpp
#include "config.hpp"
#include "instr_builder.hpp"
#include "memory_model.hpp"
#include "rv_model.hpp"

int main() {
    MemoryModel mem(4096);
    Config      cfg(Config::EXT_M);

    InstrBuilder::loadProgram(mem, {
        InstrBuilder::ADDI(10, 0,  7),        // a0 = 7
        InstrBuilder::ADDI(11, 0,  6),        // a1 = 6
        InstrBuilder::MUL (12, 10, 11),       // a2 = 42
        InstrBuilder::HALT()
    });

    RVModel cpu(cfg, mem);
    cpu.init(0);          // стартовый PC = 0
    cpu.setDebug(true);   // трассировка инструкций
    cpu.run();

    cpu.regs().dump();
    // x12 (a2): 0x0000002a  (42)
}
```

---

## Интерфейс классов

### `MemoryModel`

Плоская байтовая память с little-endian порядком байт.

```cpp
MemoryModel mem(size);               // размер в байтах, по умолчанию 1 MiB

// Загрузка программы
mem.loadProgram(bytes, base = 0);    // throws std::out_of_range если не влезает

// Чтение
ByteT b = mem.readByte(addr);
HalfT h = mem.readHalf(addr);
WordT w = mem.readWord(addr);        // throws std::out_of_range при выходе за границы

// Запись — перегруженный write()
mem.write(addr, ByteT(val));
mem.write(addr, HalfT(val));
mem.write(addr, WordT(val));

mem.dump(from, count);               // hex-дамп в stdout
```

### `RegisterFile`

32 регистра общего назначения. x0 всегда равен нулю — запись в него игнорируется.

```cpp
RegisterFile regs;

Word val = regs.get(idx);            // idx 0..31, x0 всегда 0
regs.set(idx, val);                  // запись в x0 — no-op

regs.dump();                         // вывод всех регистров с ABI-именами
```

### `RVModel`

Ядро процессора. PC хранится как прямое поле `RVModel`.

```cpp
RVModel cpu(config, mem);

// Инициализация (сброс всего состояния)
cpu.init(startPC = 0, stackPointer = 0);

// Выполнение
cpu.execute();                       // один шаг: fetch → decode → execute
cpu.run(maxSteps = 0);               // цикл до HALT или maxSteps шагов

// PC
Word pc = cpu.getPC();
cpu.setPC(addr);                     // addr должен быть кратен 4

// Состояние
RegisterFile& regs    = cpu.regs();
bool halted           = cpu.isHalted();
uint64_t count        = cpu.instrCount();

// Вспомогательное
cpu.setDebug(true);                  // печатать каждую инструкцию
cpu.reset();                         // эквивалент init(0, 0)
```

### `Config`

```cpp
Config cfg;                          // RV32I, без расширений
Config cfg(Config::EXT_M);          // RV32IM
Config cfg(Config::EXT_A);          // throws std::invalid_argument — не реализовано

cfg.hasExtension(Config::EXT_M);    // true/false
```

### `InstrBuilder`

Namespace с функциями для сборки машинных инструкций из полей.

```cpp
// Базовые форматы
InstrBuilder::R(funct7, rs2, rs1, funct3, rd, opcode)
InstrBuilder::I(imm, rs1, funct3, rd, opcode)
InstrBuilder::S(imm, rs2, rs1, funct3, opcode)
InstrBuilder::B(offset, rs2, rs1, funct3, opcode)
InstrBuilder::U(imm, rd, opcode)
InstrBuilder::J(offset, rd, opcode)

// Псевдоинструкции
InstrBuilder::ADDI(rd, rs1, imm)
InstrBuilder::ADD (rd, rs1, rs2)
InstrBuilder::SUB (rd, rs1, rs2)
InstrBuilder::MUL (rd, rs1, rs2)    // требует EXT_M
InstrBuilder::LW  (rd, rs1, offset)
InstrBuilder::SW  (rs2, rs1, offset)
InstrBuilder::JAL (rd, offset)
InstrBuilder::HALT()                 // 0x00000000 — сигнал остановки

// Загрузка программы
InstrBuilder::loadProgram(mem, { ... }, base = 0)
```

---

## Поддерживаемые инструкции

| Группа     | Инструкции                                              |
|------------|---------------------------------------------------------|
| Арифметика | `ADD` `SUB` `ADDI` `LUI` `AUIPC`                        |
| Логика     | `AND` `OR` `XOR` `ANDI` `ORI` `XORI`                    |
| Сдвиги     | `SLL` `SRL` `SRA` `SLLI` `SRLI` `SRAI`                  |
| Сравнение  | `SLT` `SLTU` `SLTI` `SLTIU`                             |
| Ветвления  | `BEQ` `BNE` `BLT` `BGE` `BLTU` `BGEU`                   |
| Переходы   | `JAL` `JALR`                                            |
| Загрузка   | `LB` `LH` `LW` `LBU` `LHU`                              |
| Запись     | `SB` `SH` `SW`                                          |
| Прочее     | `FENCE` (NOP)                                           |
| M-ext      | `MUL` `MULH` `MULHSU` `MULHU` `DIV` `DIVU` `REM` `REMU` |

**Не реализованы:** `ecall`, `ebreak`, `fence.i`

---

## Обработка ошибок

| Ситуация | Исключение |
|---|---|
| Обращение за границы памяти | `std::out_of_range` |
| Fetch fault (PC вне памяти) | `std::runtime_error` |
| Load/store fault | `std::runtime_error` |
| Нелегальный опкод | `std::runtime_error` |
| M-инструкция при выключенном EXT_M | `std::runtime_error` |
| Неизвестная операция в ALU | `std::invalid_argument` |
| Включение нереализованного расширения | `std::invalid_argument` |

В Debug-сборке дополнительно срабатывают `assert` на:

- Выход за границы регистрового файла
- Невыровненный PC (не кратен 4)
- Вызов `execute()` на остановленном CPU

---
