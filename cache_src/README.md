# cache_src

Git subtree из [Jigomas/LFU_cache](https://github.com/Jigomas/LFU_cache).
Оригинальный алгоритм (LFU + идеальный кэш Belady) сохранён без изменений.
Поверх него добавлен `include/cache_model.hpp` — LRU-кэш для симулятора RISC-V.

---

## Файлы

### Добавлено в XorOS

`include/cache_model.hpp` — `CacheModel<XLEN>`: LRU-кэш, подключаемый между `RVModel` и
`MemoryModel`. Гранулярность — 4 байта (одно слово). Политика: write-through, read-allocate.
Структура данных: `std::list<Line>` + `std::unordered_map<Addr, iterator>` — O(1) поиск и
O(1) продвижение в начало списка через `splice`. Хранит счётчики hits/misses.

### Оригинал (Jigomas/LFU_cache)

| Файл                     | Описание                                      |
|--------------------------|-----------------------------------------------|
| `include/lfu_cache.hpp`  | LFU-кэш                                       |
| `include/ideal_cache.hpp`| Идеальный кэш (алгоритм Belady)               |
| `src/lfu_cache.cpp`      | Точка входа: stdin → LFU                      |
| `src/ideal_cache.cpp`    | Точка входа: stdin → Belady                   |
| `test/`                  | Тестовые данные                               |
| `CMakeLists.txt`         | Сборка оригинала (не подключена к XorOS)      |

---

## Использование в симуляторе

```cpp
MemoryModel<32>             mem(0x10000);
CacheModel<32>              cache(mem, 64);   // 64-словный LRU
RVModel<32, CacheModel<32>> cpu(cfg, cache);
cpu.run();

std::cout << cache.hits() << " hits / " << cache.misses() << " misses\n";
std::cout << cache.hitRate() * 100.0 << "% hit rate\n";
```
