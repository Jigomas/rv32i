# RV32I CPU Model

Программная модель процессора RISC-V на C++17.

---

## О проекте

Симулятор реализован как шаблонная библиотека, параметризованная по разрядности (`XLEN=32/64`),
с явными инстанциациями в отдельных `.cpp`-файлах. Поддерживает базовую архитектуру RV32I
с расширениями целочисленного умножения (M) и атомарных операций (A). Архитектура Von Neumann,
single-cycle: один вызов `step()` — один цикл fetch → decode → execute. Для взаимодействия
с хостом симулятор перехватывает инструкцию `ecall` через C++ callback (`setEcallHandler`).
Между процессором и памятью можно вставить `CacheModel` — LRU-кэш, который собирает
статистику попаданий и промахов в реальном времени.

### Архитектурные решения

**Single-cycle, Von Neumann.** Один `step()` — один fetch/decode/execute без кэша и конвейера.
Единая память для кода и данных. Состояние процессора полностью определяется PC
и регистровым файлом — удобно для пошаговой отладки ОС.

**Header-only + явные инстанциации.** Шаблоны параметризованы по `XLEN`.
Каждый компонент (ALU, Decoder, MemoryModel и др.) инстанциирован явно в отдельном `.cpp` —
компиляция происходит один раз, без дублирования кода.

**ECALL через callback.** Симулятор не знает ничего про OS — при встрече инструкции `ecall`
вызывается зарегистрированный хендлер. Это позволяет запускать любую bare-metal программу
с произвольным набором системных вызовов.

**CacheModel как сменный слой памяти.** `RVModel` принимает тип памяти как шаблонный параметр
(`RVModel<XLEN, MemT>`), что позволяет подставить `CacheModel<32>` вместо `MemoryModel<32>`
без изменения ядра симулятора. `CacheModel` реализует LRU-кэш с политикой write-through
и read-allocate: промах при чтении загружает слово в кэш, запись всегда проходит насквозь
в `MemoryModel`. Размер кэша — 64 слова (256 байт). После исполнения симулятор печатает
статистику: количество попаданий, промахов и hit rate. При прогоне ядра XorOS получается
около 87% попаданий.

---

## Структура проекта

```plaintext
rv32i/
├── include/
│   ├── types.hpp           # XlenTraits<XLEN>: UWord/SWord/Addr, ByteT/HalfT/WordT
│   ├── config.hpp          # Config: EXT_M, EXT_A; флаги расширений
│   ├── isa.hpp             # Opcode/Funct3/Funct7/Funct5AMO; extractBits, signExtend;
│   │                       # геттеры полей (getOpcode/getRd/...); декодеры immediate (I/S/B/U/J)
│   ├── decoder.hpp         # DecodedInstr<XLEN>; Decoder<XLEN>::decode(raw)
│   ├── disasm.hpp          # Disasm::disassemble(DecodedInstr) → строка; ABI-имена регистров
│   ├── register_file.hpp   # RegisterFile<XLEN>: 32 рег, x0=0;
│   │                       # RegRole (Preserved/NonPreserved/Special), ABI_NAMES[]
│   ├── memory_model.hpp    # MemoryModel<XLEN>: плоская память, LR/SC reservation
│   ├── alu.hpp             # ALU<XLEN>::execute(Op, a, b): RV32I + M-ext
│   ├── instr_builder.hpp   # Кодировщики R/I/S/B/U/J + псевдоинструкции + AMO + CSR
│   ├── csr_file.hpp        # Адреса CSR, CsrFile<XLEN>: read/write/csrrw/csrrs/csrrc
│   └── rv_model.hpp        # RVModel<XLEN>: PC, step/run/halt, ECALL, CSR, Context/FullContext
├── src/
│   ├── main.cpp            # Демо (3+4)*5=35; run_os(path) — загружает OS бинарник
│   └── *.cpp               # Явные инстанциации шаблонов для XLEN=32 и XLEN=64
├── cache_src/              — git subtree: Jigomas/LFU_cache (алгоритм заменён на LRU)
│   ├── include/
│   │   ├── cache_model.hpp # CacheModel<XLEN>: LRU-кэш, write-through, read-allocate; hit/miss
│   │   ├── lfu_cache.hpp   # оригинал: LFU-кэш (не используется симулятором)
│   │   └── ideal_cache.hpp # оригинал: идеальный кэш Belady (не используется симулятором)
│   ├── src/
│   │   ├── lfu_cache.cpp   # оригинал: точка входа LFU
│   │   └── ideal_cache.cpp # оригинал: точка входа Belady
│   ├── test/               # оригинальные тестовые данные
│   ├── CMakeLists.txt      # оригинальный CMake (не подключён к проекту)
│   └── README.md           # оригинальный README
├── tests/
│   └── test.cpp            # Набор тестов (MemoryModel, RegisterFile, ALU, Decoder, RVModel,
│                           #              Alignment, Sv32 vmem, CSR, Disasm) — 100 тестов
├── CMakeLists.txt
└── README.md
```

---

## Требования

| Инструмент  | Версия |
|-------------|--------|
| CMake       | ≥ 3.16 |
| GCC / Clang | C++17  |

---

## Сборка и запуск

### Debug (ASan + UBSan + assert)

```bash
cmake -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build -j$(nproc)
```

### Release

```bash
cmake -B build-release -DCMAKE_BUILD_TYPE=Release
cmake --build build-release -j$(nproc)
```

### Запуск демо

```bash
./build/rv32i_cpu
```

### Запуск OS бинарника

```bash
./build/rv32i_cpu kernel.bin
```

Симулятор загружает flat binary по адресу `0x0`, выделяет 64 KiB памяти, стартует с PC=0.
ECALL-хендлер: `a7=1` → `putchar(a0)`, `a7=10` → halt.

### Запуск тестов

```bash
./build/rv32i_tests
```

---

## Поддерживаемые инструкции

| Группа     | Инструкции                                               |
|------------|----------------------------------------------------------|
| Арифметика | `ADD` `SUB` `ADDI` `LUI` `AUIPC`                         |
| Логика     | `AND` `OR` `XOR` `ANDI` `ORI` `XORI`                     |
| Сдвиги     | `SLL` `SRL` `SRA` `SLLI` `SRLI` `SRAI`                   |
| Сравнение  | `SLT` `SLTU` `SLTI` `SLTIU`                              |
| Ветвления  | `BEQ` `BNE` `BLT` `BGE` `BLTU` `BGEU`                    |
| Переходы   | `JAL` `JALR`                                             |
| Загрузка   | `LB` `LH` `LW` `LBU` `LHU`                               |
| Запись     | `SB` `SH` `SW`                                           |
| Прочее     | `ECALL` `MRET` `FENCE` `SFENCE.VMA` (NOP)                |
| M-ext      | `MUL` `MULH` `MULHSU` `MULHU` `DIV` `DIVU` `REM` `REMU`  |
| A-ext      | `LR.W` `SC.W`+ AMO(SWAP/ADD/XOR/AND/OR/MIN/MAX/MINU/MAXU)|
| CSR        | `CSRRW` `CSRRS` `CSRRC` `CSRRWI` `CSRRSI` `CSRRCI`       |

---

## Обработка ошибок

| Ситуация                               | Поведение                          |
|----------------------------------------|------------------------------------|
| Обращение за границы памяти            | `std::out_of_range`                |
| Нелегальный опкод (mtvec=0)            | `std::runtime_error`               |
| Нелегальный опкод (mtvec≠0)            | `fireTrap(EXC_ILLEGAL_INSN)`       |
| M/A-инструкция при выключенном EXT_M/A | `fireTrap(EXC_ILLEGAL_INSN)`       |
| LW/SW/AMO невыровненный адрес          | `fireTrap(EXC_LOAD/STORE_MISALIGN)`|
| LH/SH нечётный адрес                   | `fireTrap(EXC_LOAD/STORE_MISALIGN)`|
| JAL/JALR/BRANCH невыровненный target   | `fireTrap(EXC_INSN_MISALIGN)`      |
| Обращение вне памяти (fetch/load/store)| `fireTrap(EXC_*_FAULT)`            |
| Sv32 страница не отображена            | `fireTrap(EXC_*_PAGE_FAULT)`       |
| Неизвестная операция в ALU             | `std::invalid_argument`            |

В Debug-сборке дополнительно срабатывают `assert` на:

- Выход за границы регистрового файла
- Невыровненный PC (не кратен 4)
- Вызов `step()` на остановленном CPU
- Передача неподдерживаемого расширения в `Config` (F/D/C)
