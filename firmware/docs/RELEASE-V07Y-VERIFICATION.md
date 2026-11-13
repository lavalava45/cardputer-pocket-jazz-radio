# Pocket Jazz Radio — software release v0.7y / аппаратный статус

**Версия:** v0.7y, отдельный immutable software-релиз после v0.7x. **Полный программный gate PASS 11.11.2026; физический Cardputer ADV НЕ ПРОВЕРЕН.** Не объявлять исправление всех HTTPS-отказов, слуховых пауз, underruns или marquee до реального теста.

## Подтверждённые изменения в исходниках

- `AudioFileSourceICYStream::readNonBlock()` перестал выполнять blocking ICY metadata drain (до 2500 мс, повторяющиеся `delay(1)` в аудиозадаче). Вместо этого fixed-memory persistent `IncrementalMetadata`, максимум 256 Б метаданных за один вызов, deadline через `millis()` с wrap, отсутствие внутреннего reconnect в nonblocking режиме; повреждённый title не передаётся в UI, остаток ICY блока всё равно потребляется, MP3 byte boundary сохраняется.
- Plain HTTP `HTTPClient::getStreamPtr()` принимает сырые chunked bytes. Использует bounded `HttpChunkedBody` перед MP3/ICY; не принимает `Transfer-Encoding` кроме `identity`/`chunked`. В HTTPS отдельный dechunk parser остаётся включённым. Теория о «двойном dechunk» не подтверждена установленным SDK, его отключение было бы регрессией.
- HTTPS `SecureICYSource::consumeMetadata(nonBlocking)` имеет per-call бюджет 256 Б (при сохранении metadata state). Во время сетевого `audioWorker` open priority=1 вместо 2, restore=2 сразу после open/isOpen; UI loop priority1 на core1 получает timeslices. Для текущего активного decode приоритет2 сохраняется.
- `Buf`/`Buf min`/HTTPS-фазы v0.7x сохранены, heap/largest до/после TCP+TLS, максимум UI-gap, no insecure/downgrade/second TLS, MP3 ring 32768, prefill 2048. Два новых C++ tests из `tests/pcm/CMakeLists.txt`, оба с действующими assert в MSVC Release.

## Проверка software gate

Фактически выполнено `powershell -NoProfile -ExecutionPolicy Bypass -File tools/build.ps1 -Version 0.7y -PackageUnverifiedPManSize`, код возврата **0**. Результаты из файлов релиза:

- **Python unittest: 198/198 PASS**, `TEST-REPORT.txt`, время 77,378 с.
- **MSVC x64 Release CTest: 39/39 PASS**, `HOST-CTEST-REPORT.txt`, время 1,14 с; CTest действительно выполнил `live_radio_buffer_minimum_reset_after_decoder_stop_and_reconnect` (тест 9/39) и `plain_http_icy_partial_chunked_metadata_timeout_ring_and_reconnect` (тест 16/39), оба с действующими `assert` при Release/NDEBUG.
- **ESP32-S3 main compile: PASS**, FQBN с `PSRAM=disabled`, static RAM 111 188 байт из 327 680, код приложения 1 574 491 байт из 3 342 336 (47%). Это отчёт компилятора, а не фактический размер свободного APP-слота M5Launcher 3.
- **Три compile-only PASS**: ESP-NOW HMAC/mbedTLS, opt-in HA MQTT/TLS, non-owning PCM mirror SDK v0.8e.
- Источник релиза: `dist/releases/0.7y/source/` + `source/SHA256-SOURCES.txt`, в том числе `AudioICYIncremental.h`, `IcyMetadataText.h`, `HttpChunkedBody.h`, `tests/pcm/CMakeLists.txt`, два новых C++ теста. Скрипт фиксирует выбранные source/dependency файлы и их хеши до/после компиляции; **полный установленный toolchain и транзитивные библиотеки не архивируются**, см. `BUILD-INPUTS.txt`.

## Релизные артефакты и независимая SHA-256 проверка

Корень `E:\Downloads\devspace-test\cardputer-adv\apps\pocket-jazz-radio\dist\releases\0.7y\`:

| Относительный путь | Размер, байт | SHA-256 |
|---|---:|---|
| `firmware/PocketJazzRadio-v0.7y-app-only.bin` | 1 574 640 | `39c8608e42af926714e35a1235e8342c2365892e106072c405bf2cdc350cfefc` |
| `PocketJazzRadio-v0.7y-Launcher3-SD.zip` | 1 049 998 | `28b2ac9e249499823110012ba0c3a60fb10c9cd266ad85120c199648c9889070` |
| `TEST-REPORT.txt` | 32 761 | `eadce6ff1ee76cc37c6acf401a4bd35dcf766aaf866fe301792bc95e9df5c129` |
| `HOST-CTEST-REPORT.txt` | 22 094 | `2084beaaf289a0f581d59e0dc777c1dfd9c6024cfc42854521c42b7a2af06441` |
| `BUILD-INPUTS.txt` | 7 905 | `cad9409e75f50e5666ee91a2decc0c4db3f0838482c86d1debb905c88c81863d` |
| `ARDUINO-COMPILE-REPORT.txt` | 1 060 | `3b2dca146e75a31b6b2056c35570ab25ebe7c0a61c540922a3a547c7879daf7f` |

Повторный `Get-FileHash` BIN и ZIP и отдельная Python SHA-проверка содержимого показали совпадение с `SHA256-MANIFEST.txt`; ZIP тестируется `ZipFile.testzip()` (15 записей, без повторяющихся имён). **Frozen `dist/releases/0.7y/` не изменять**. Отчёт подготовлен и обновлён в живых `docs/` уже после создания frozen snapshot, поэтому его отсутствие в `source/docs` v0.7y не меняет сохранённые в релизе исходники и SHA; при следующих версиях добавлять release verification/HTTPS trace в snapshot явно.

**Статус установки:** скрипт помечает BIN `NOT CLEARED FOR INSTALL` — консервативный предел 1 376 256 байт, BIN 1 574 640 байт, фактическая APP-capacity PMan неизвестна. Никакой автоматической прошивки/разметки/записи SD/NVS не было. Чтобы вручную запустить через Launcher3, сначала убедиться, что реальный APP-слот в текущей таблице разделов вмещает бинарник, иначе оставить установленную версию; никаких изменений flash layout без отдельного решения.

## Интеграционные инварианты

Сохранены строгие CA и original hostname/SNI для HTTPS; никакого `setInsecure()`, downgrade или фонового вторичного запроса. Один `audioWorker`, очередь команд с копированием URL, один MP3 decoder/M5.Speaker/ES8311; SD backup только ручной при STOP; Wi-Fi-профили и schema NVS прежние; offline MP3, alarm, likes, Diagnostics не переписываются. Static DRAM и largest heap следует подтверждать компиляцией/устройством, поскольку SDK compiled с PSRAM disabled.

## Минимальная физическая проверка после software PASS

Через M5Launcher 3 вручную запустить **только app-only BIN** с собственной SD. Не менять загрузчик, разделы, NVS, личные `.m3u`/`settings.bak`. Уже выполненные Swiss DIRECT/REDIRECT сравнения не повторять. Одна неработавшая HTTPS-станция (Swiss Jazz либо TSF Jazz), затем одна прерывавшаяся HTTP/HTTPS (например, Bossa Beyond): записать `I → Down` (`Fail`, `Buf`, `Under`, `PCM gap`, `TLS raw`) и `I → Down×3` (`HTTPS phase/hop`, DNS/secure ms, memory before/after, `Buf min`, `UIgap`) вместе с субъективными паузами. Если эта короткая проверка проходит, отдельно 5×30 минут и ≥2 ч непрерывного эфира; без этих наблюдений hardware gate НЕ пройден.
