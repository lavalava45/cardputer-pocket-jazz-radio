# Pocket Jazz Radio v0.7z3 — проверка выпуска, 12.11.2026

**Итог: immutable SOFTWARE PASS / HARDWARE NOT TESTED.** Собран app-only BIN для M5Launcher 3; на физический Cardputer ADV ничего не записывалось. Каталог `dist/releases/0.7z3/` создан впервые и не заменяет `0.7z2`.

## Результаты независимой проверки архива

| Артефакт | SHA-256 |
| --- | --- |
| `firmware/PocketJazzRadio-v0.7z3-app-only.bin` | `41a94b6b095226236a79d2952cb2138289ada65e9a84b8709913c527802921ee` |
| `PocketJazzRadio-v0.7z3-Launcher3-SD.zip` | `16fb06cf76022ec5121f22a191daf23f70f81972a5cd0ff2180c90ca7e4c419f` |
| `TEST-REPORT.txt` | `9ae3c0f104e2aae7dd3b8f68754fb0d7d2a32a2cf3c506ce5fce9d2262845b04` |
| `HOST-CTEST-REPORT.txt` | `4b489a4376575cbdcfd6aceb5f0b3a9c1db46bf3e32d7022093e94d71a13163f` |
| `BUILD-INPUTS.txt` | `2d9ed2e2d6cb35c27106af7f0af8c13c1ba9cb8964bf14fcd3056a5739ce20c4` |
| `ARDUINO-COMPILE-REPORT.txt` | `a3fa4b08064d06f9fb597b0a594ffcef93919f60a9ccb2b6156d7b1df29232c3` |

Все **6/6** записей SHA256-MANIFEST были независимо пересчитаны и совпали. **251/251** исходных файлов в frozen `source/SHA256-SOURCES.txt` совпали по SHA-256; недостающих и лишних файлов нет. ZIP: **15 записей**, уникальные пути, CRC (`ZipFile.testzip`) PASS, встроенный BIN побайтово совпал с BIN отдельного каталога. `AudioRuntimeDiagnostics.h`, `SecureICYSource.cpp`, `PocketJazzRadioApp.cpp` совпадают между текущим деревом и frozen snapshot.

## Gate и память

- **226/226 Python unittest PASS** (`Ran 226 tests in 82.160s` / `OK`). Три старых source-contract теста были обновлены для восьмой страницы Diagnostics, ручного экспорта runtime TSV и `icy-metaint: 0`; их отдельный прогон 11/11 PASS.
- **45/45 MSVC x64 Release CTest PASS**, включая два новых теста живых стадий/ошибок/конкурентного snapshot и PCM delayed release. `ThreadSanitizer` не запускался; host-тест не моделирует полную сеть и два ядра ESP32.
- **Основное Arduino-ESP32-S3 приложение PASS** на m5stack:esp32 3.3.9, PSRAM disabled. Sketch **1 587 515 Б**; app-only BIN **1 587 664 Б**. Статическая RAM **114 044 Б**, остаётся **213 636 Б** от объявленного компилятором бюджета **327 680 Б**. Это не текущий свободный TLS heap и не largest contiguous block.
- **3/3 compile-only probes PASS**: ESP-NOW HMAC, HA MQTT с TLS, PCM mirror с pinned Audio Router SDK 0.8e.
- Историческая v0.7z2 сохранена: SHA-256 её BIN всё ещё `03cadf6b658963f3512a177fba345ea1dadb52903cadce562fd3f840ecceb7df`.

По сравнению с v0.7z2: app-only BIN **+9 664 Б** (1 578 000 → 1 587 664), статическая RAM **+2 832 Б** (111 212 → 114 044). Размер `AudioRuntimeProbe` на host x64 **952 Б**, `AudioRuntimeJournal` **1 872 Б**; в прошивке размер защищён `static_assert` (1024/2048 Б соответственно).

## Исправление перед выпуском

Первая попытка firmware compile остановлена корректными статическими проверками: реальный ESP32 SDK компилирует с `-mdisable-hardware-atomics`, поэтому предположение Codex о всегда lock-free `std::atomic<T>` не подходит. Вместо простого снятия проверки применяется краткий `portMUX_TYPE` для согласованных структур PLAYER telemetry/failure ring. В критических секциях нет I/O, heap и сетевых вызовов. После изменения оба host CTest 2/2 PASS и полный ESP32/host gate PASS.

Причина многоминутного `CONNECTING` остаётся неизвестной. Реализация только измеряет текущие стадии и сохраняет RAM-историю; `setHandshakeTimeout(5s)` ограничивает handshake, но не DNS, TCP или общий бюджет HTTP. Не менялись prefill 2048 Б, live watchdog 4000 мс, MP3 ring 32768 Б, CA roots, аудиоприоритеты и владение PCM-буферами. KJazz +GTS R4 **не применён**.

## Аппаратная приёмка ещё впереди

**Устаревшее предупреждение упаковщика:** v0.7z3 выпущена по старой политике, которая опиралась на неподтверждённый порог размера, а не на спецификацию M5Launcher 3. Этот порог удалён из действующего ROADMAP и сборщика; уже опубликованные BIN/ZIP/manifest не меняются. Для ручного использования нужен именно app-only BIN через штатный M5Launcher; не выбирать merged/full-flash/bootloader/partition образ и отменять предложения перезаписать разделы, приложения или личные данные.

После добровольного ручного запуска: **I → Diagnostics → страница 8 SYSTEM HEALTH** показывает live stage, время стадии, heartbeat, попытку/редирект. **E** на странице 8 вручную дописывает RAM-историю в `/PocketJazz/audio-runtime.tsv` на SD. **X** сбрасывает отображаемые аудиосчётчики, не удаляя runtime history. При первом длительном пропадании звука сохранить журнал, отметить станцию и сравнить текущую фазу с Wi-Fi, заполнением MP3 ring, отказами PCM и памятью. Это не доказательство непрерывного реального звука до прослушивания на ADV.
