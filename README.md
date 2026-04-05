# RV32I CPU Model

Программная модель процессора RISC-V на C++17.

![C++17](https://img.shields.io/badge/C%2B%2B-17-blue)
![RISC-V](https://img.shields.io/badge/arch-RISC--V%20RV32IA-green)
![CMake](https://img.shields.io/badge/build-CMake-informational)

---

## О проекте

Симулятор реализован как шаблонная библиотека, параметризованная по разрядности (`XLEN=32/64`),
с явными инстанциациями в отдельных `.cpp`-файлах. Поддерживает базовую архитектуру RV32I
с расширениями целочисленного умножения (M) и атомарных операций (A). Архитектура Von Neumann,
single-cycle: один вызов `step()` — один цикл fetch → decode → execute. Симулятор отслеживает
текущий уровень привилегий (`PrivMode`: M/S/U) и маршрутизирует исключения через `fireTrap` —
включая `ecall`, который кидает `EXC_ECALL_U/S/M` в зависимости от режима. Между процессором
и памятью можно вставить `CacheModel` — LRU-кэш, который собирает статистику попаданий
и промахов в реальном времени.

### Архитектурные решения

**Single-cycle, Von Neumann.** Один `step()` — один fetch/decode/execute без конвейера.
Единая память для кода и данных. Состояние процессора полностью определяется PC
и регистровым файлом — удобно для пошаговой отладки ОС.

**Header-only + явные инстанциации.** Шаблоны параметризованы по `XLEN`.
Каждый компонент (ALU, Decoder, MemoryModel и др.) инстанциирован явно в отдельном `.cpp` —
компиляция происходит один раз, без дублирования кода.

**Privilege tracking + ECALL через fireTrap.** Симулятор хранит текущий уровень привилегий
в поле `priv_mode_` (`PrivMode::M/S/U`). Инструкция `mret` восстанавливает режим из `mstatus.MPP`,
`fireTrap` сохраняет его туда же. При встрече `ecall` симулятор не вызывает никакой C++
функции — он вызывает `fireTrap(EXC_ECALL_U/S/M)`, устанавливает `mepc` и передаёт управление
на `mtvec`. Обработка системных вызовов полностью на стороне ОС. CSR-инструкции проверяют
право доступа: биты [9:8] адреса CSR кодируют минимальный уровень привилегий; обращение
из недостаточного режима → `fireTrap(EXC_ILLEGAL_INSN)`.

**CacheModel как сменный слой памяти.** `RVModel` принимает тип памяти как шаблонный параметр
(`RVModel<XLEN, MemT>`), что позволяет подставить `CacheModel<32>` вместо `MemoryModel<32>`
без изменения ядра симулятора. `CacheModel` реализует LRU-кэш с политикой write-through
и read-allocate: промах при чтении загружает слово в кэш, запись всегда проходит насквозь
в `MemoryModel`. Размер кэша — 64 слова (256 байт). После исполнения симулятор печатает
статистику: количество попаданий, промахов и hit rate. При прогоне ядра XorOS получается
около 79% попаданий.

```plaintext
┌─────────────────────────────────────────────────────────┐
│                    RVModel<XLEN, MemT>                  │
│                                                         │
│  PC · RegisterFile · CsrFile · PrivMode (M/S/U)         │
│  ┌──────────┐  ┌──────────┐  ┌──────────┐               │
│  │  Decoder │  │   ALU    │  │  CsrFile │               │
│  └──────────┘  └──────────┘  └──────────┘               │
│                                                         │
│  fetch / load / store  ──►  MemT (шаблонный параметр)   │
└────────────────────────────┬────────────────────────────┘
                             │
          ┌──────────────────┼──────────────────┐
          │                                     │
┌─────────▼──────────┐             ┌────────────▼────────────┐
│  MemoryModel<XLEN> │             │    CacheModel<XLEN>     │
│  плоская память    │             │  LRU · write-through    │
│  LR/SC reservation │             │  read-allocate · 64 слов│
│  MMIO regions      │◄────miss────┤  hits / misses stats    │
└────────────────────┘             └─────────────────────────┘
```

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
│   ├── memory_model.hpp    # MemoryModel<XLEN>: плоская память, LR/SC reservation, MMIO regions
│   ├── alu.hpp             # ALU<XLEN>::execute(Op, a, b): RV32I + M-ext
│   ├── instr_builder.hpp   # Кодировщики R/I/S/B/U/J + псевдоинструкции + AMO + CSR
│   ├── csr_file.hpp        # Адреса CSR, CsrFile<XLEN>: read/write/csrrw/csrrs/csrrc
│   ├── rv_model.hpp        # RVModel<XLEN>: PC, PrivMode, step/run/halt, fireTrap, Context/FullContext
│   │                       # setStepHook / setTrapHook — колбеки для Dumper
│   └── dumper.hpp          # Dumper<XLEN>: трейс инструкций/трапов в .txt; dumpState/dumpMemHex
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
│                           #              Alignment, Sv32 vmem, CSR, Disasm, MMIO) — 108 тестов
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
# Из корня репозитория:
cmake -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build -j$(nproc)
```

### Release

```bash
# Из корня репозитория:
cmake -B build-release -DCMAKE_BUILD_TYPE=Release
cmake --build build-release -j$(nproc)
```

### Запуск демо

```bash
./build/rv32i/rv32i_cpu
```

### Запуск OS бинарника

```bash
# обычный запуск
./build/rv32i/rv32i_cpu os/build/xoros.bin

# с дампом регистров/CSR/памяти в stderr по завершении
./build/rv32i/rv32i_cpu os/build/xoros.bin --debug

# запись трейса инструкций + трапов в файл (plain text)
./build/rv32i/rv32i_cpu os/build/xoros.bin --trace trace.txt

# оба режима одновременно
./build/rv32i/rv32i_cpu os/build/xoros.bin --debug --trace trace.txt
```

Симулятор загружает flat binary по адресу `0x0`, выделяет 64 KiB памяти, стартует с PC=0.
Системные вызовы (`ecall`) маршрутизируются через `fireTrap` — обрабатываются ОС.

`--debug` по завершении выводит в stderr: все 32 регистра, ключевые CSR (mcause с именем),
состояние CLINT (`mtime` / `mtimecmp`), hex+ASCII дамп первых 512 байт текста и BSS (0x3000-0x4000).

`--trace` пишет в файл построчно: `[00000000] addi sp, sp, -16` для каждой инструкции
и `!TRAP cause=0x80000007 (INT_TIMER_M) mepc=0x... mtime=...` для каждого трапа.

### Запуск тестов

```bash
./build/rv32i/rv32i_tests
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
| CSR-доступ ниже требуемого привилегия  | `fireTrap(EXC_ILLEGAL_INSN)`       |
| LW/SW/AMO невыровненный адрес          | `fireTrap(EXC_LOAD/STORE_MISALIGN)`|
| LH/SH нечётный адрес                   | `fireTrap(EXC_LOAD/STORE_MISALIGN)`|
| JAL/JALR/BRANCH невыровненный target   | `fireTrap(EXC_INSN_MISALIGN)`      |
| Обращение вне памяти (fetch/load/store)| `fireTrap(EXC_*_FAULT)`            |
| Sv32 страница не отображена            | `fireTrap(EXC_*_PAGE_FAULT)`       |
| U-mode обращение к странице без PTE.U  | `fireTrap(EXC_*_PAGE_FAULT)`       |
| Неизвестная операция в ALU             | `std::invalid_argument`            |

В Debug-сборке дополнительно срабатывают `assert` на:

- Выход за границы регистрового файла
- Невыровненный PC (не кратен 4)
- Вызов `step()` на остановленном CPU
- Передача неподдерживаемого расширения в `Config` (F/D/C)
