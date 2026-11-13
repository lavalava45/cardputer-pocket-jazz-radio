# Pocket Jazz Radio v0.7z1 — полный software gate и immutable release, 11.11.2026

## Результат и предел доказательства

**Software gate PASS**: `powershell -NoProfile -ExecutionPolicy Bypass -File tools/build.ps1 -Version 0.7z1 -PackageUnverifiedPManSize`, **exit code 0**. **Hardware gate NOT TESTED**; не утверждать, что Swiss Jazz, TSF Jazz France, KJazz, Bossa Beyond или другие станции уже стабильно звучат на физическом Cardputer ADV. Никаких flash/upload, изменений M5Launcher 3, flash partition, NVS или пользовательских файлов SD не выполнялось. Архивы v0.7y/v0.7z не менялись.

## Причины и исправления, реально вошедшие в новый BIN

1. **HTTP→HTTPS с query без `/`:** frozen v0.7z принимал корректный `Location: https://radio.example.org?stream=1`, но Arduino-ESP32 3.3.9 `HTTPClient::beginInternal()` искал границу hostname только по `/` и выполнял запрос к неверному `radio.example.org?stream=1`. В `PocketJazzRadio/SafeHttpsUpgrade.h` добавлена безопасная нормализация в `https://radio.example.org/?stream=1`; увеличение на один символ учитывает предел 192-байтного буфера. Порт, hostname, query, CA, SNI и hostname verification сохраняются; HTTP→HTTPS выполняется последовательно после закрытия первого HTTP source, без второго одновременного TLS-клиента и без downgrade. `SecureICYSource` выполняет такую же нормализацию для прямых HTTPS и HTTPS→HTTPS редиректов и отклоняет превышение длины после вставки `/`.
2. **Ложная отмена при volume-only:** проверки `uxQueueMessagesWaiting(audioQueue)>0` в v0.7z при GET/prefill воспринимали даже команду изменения громкости как STOP, ошибочно освобождали успешное соединение и усиливали reconnect. В `AudioIntentPolicy.h` и `PocketJazzRadioApp.cpp` UI публикует атомарное поколение намерения до `xQueueOverwrite`; STOP/смена URL обновляют поколение, изменение только громкости — нет. На audioWorker реализован контроль поколения на входе, до/после prefill, до/после `AudioGeneratorMP3::begin()` и непосредственно перед PCM loop. Это **не** мгновенная отмена уже выполняющегося `HTTPClient::GET()`; безопасная сетевая отмена без второго socket-owner остаётся открытой задачей.
3. **Сохранён ранее исправленный общий аудиотракт:** инкрементальные не блокирующие 2,5 с ICY deadline (до 256 B metadata за poll), декодирование HTTP Transfer-Encoding chunked перед ICY/MP3, фиксированная память, cooperative `AudioFileSourceBuffer`, bounded MP3 sync/budget, корректный Buf/minimum reset, фазовая TLS-диагностика и UIgap. Нет необоснованного изменения ring 32768 B, prefill target 2048 B, FreeRTOS приоритетов, CA или ES8311. При анализе ESP32 SDK подтверждено: `HTTPClient::getStreamPtr()` отдаёт raw body, а не уже dechunked байты.

## Фактически выполненные проверки

| Gate | Результат | Frozen доказательство |
| --- | --- | --- |
| Python unittest discovery, включая новые URL и intent integration | **204/204 PASS** за 93,821 с | `TEST-REPORT.txt` |
| MSVC x64 Release CTest, все реально исполняемые targets | **41/41 PASS** | `HOST-CTEST-REPORT.txt` |
| CTest №9 Buf reset/minimum | **PASS** | `HOST-CTEST-REPORT.txt` |
| CTest №16 ICY partial/chunked/ring/reconnect | **PASS** | `HOST-CTEST-REPORT.txt` |
| CTest №17 HTTPS upgrade negative/host?query/SDK-совместимый authority split | **PASS** | `HOST-CTEST-REPORT.txt` |
| CTest №18 STOP/switch vs volume-only generation | **PASS** | `HOST-CTEST-REPORT.txt` |
| Main Arduino-ESP32 3.3.9 ESP32-S3 | **PASS**, sketch 1 576 307 B (47%); static DRAM 111 196 B (33%) | `ARDUINO-COMPILE-REPORT.txt` |
| ESP-NOW HMAC/mbedTLS, opt-in HA MQTT/TLS, non-owning PCM mirror SDK v0.8e | **3/3 compile-only PASS** | `ARDUINO-COMPILE-REPORT.txt` |
| Source snapshot | **234/234 files SHA-256 PASS**, no missing/extra files | `source/SHA256-SOURCES.txt` |
| Launcher ZIP | **15 entries, CRC PASS, no duplicate members** | independent Python `ZipFile.testzip()` |

Ключевые C++ тесты имеют `#undef NDEBUG` **до** подключения `<cassert>`, поэтому negative asserts действительно работают в MSVC Release; CTest `live_radio_buffer_minimum_reset_after_decoder_stop_and_reconnect` включён в CMake и реально выполнен. До полного gate отдельно выполнены **6/6** фокусированных Python и **4/4** фокусированных MSVC Release CTest (Buf, ICY, HTTPS URL, intent generation). Независимая рецензия кода не выявила новой доказанной регрессии, но не заменяет аппаратных замеров.

## Проверенные артефакты

Каталог: `E:\Downloads\devspace-test\cardputer-adv\apps\pocket-jazz-radio\dist\releases\0.7z1\`.

| Относительный путь | Байты | SHA-256 (независимо пересчитан) |
| --- | ---: | --- |
| `firmware/PocketJazzRadio-v0.7z1-app-only.bin` | 1 576 448 | `0eefb81e8ce934add809737db8b0af3d166a9d7e3d765ac333f78606ae4c563a` |
| `PocketJazzRadio-v0.7z1-Launcher3-SD.zip` | 1 051 439 | `80da0c5c337eb10d24df8eabc79f3026ba44fc910160386e486092bc50d67074` |
| `TEST-REPORT.txt` | 33 966 | `8c4c9ba22e402f53d0ea50bf4f6c248e75dbb24f9960aa7a065d81d6acca4bc4` |
| `HOST-CTEST-REPORT.txt` | 23 605 | `d095e415a47719a73ac209fc43c75d3e94782b36abb479da6121f1c913f60ff5` |
| `BUILD-INPUTS.txt` | 8 111 | `546c46741591cc7d96f4a55a685320f0b81f56dd6a69a81aae902b25ffbb04be` |
| `ARDUINO-COMPILE-REPORT.txt` | 1 060 | `4f28bfd3ef3c39766cdf539ba0b890b8946aa417030788b4579c15bf6d62b74b` |

Все шесть SHA совпали с `SHA256-MANIFEST.txt`. Frozen snapshot содержит новые `AudioIntentPolicy.h`, `audio_intent_policy_test.cpp`, `test_v07z1_url_and_intent_integration.py`, обновлённые HTTPS/Audio app/ICY integration, CMake, vendored headers и `docs/RELEASE-V07Z-VERIFICATION.md`. Полный компилятор и все транзитивные Arduino зависимости в snapshot **не вложены** — точная их версия/артефакты описаны в `BUILD-INPUTS.txt`. Настоящий итоговый документ записан **после freeze** в рабочую документацию, поэтому не находится в frozen `source/`; это не изменение релизного архива.

## Что остаётся открытым

- **На физическом Cardputer v0.7z1 ещё не запущена.** HTTPS DIRECT/REDIRECT Swiss Jazz и TSF Jazz France ранее не работали на v0.7v; KJazz также FAIL. WRTI играла, Bossa Beyond и 181.fm могли временно играть, затем были многосекундные тишины и рывки marquee. Нельзя считать, что только два исправления выше полностью решают каждую из этих проблем. Значение `TLS raw +9984` не означает проверенный сертификатный сбой: это может быть положительный socket descriptor.
- **STOP во время синхронного DNS/TCP/TLS/GET** не гарантирует немедленного освобождения audioWorker до окончания SDK call. Новая generation-проверка предотвращает запуск старого декодера при замеченном новом намерении, но не обеспечивает hard real-time отмену уже выполняющейся сетевой операции или декодерного шага.
- **Неизвестен фактический доступный APP-слот M5Launcher 3/PMan**: app-only 1 576 448 B превышает консервативный предел упаковщика 1 376 256 B. Это **NOT CLEARED FOR INSTALL**. Продолжать software разработку можно, но ни прошивать, ни переписывать разделы/SD/NVS автоматически нельзя.

## Минимальная физическая приёмка (только после read-only проверки APP-слота)

Вручную загрузить v0.7z1 через имеющийся M5Launcher 3 **только если** подтверждено, что app-only BIN помещается в существующий слот; не трогать partition/NVS/личные SD плейлисты. Не повторять уже выполненные DIRECT/REDIRECT Swiss Jazz A/B. На одном Wi-Fi по 1–2 мин проверить одну ранее отказавшую HTTPS-станцию (Swiss Jazz *или* TSF Jazz) и одну ранее прерывавшуюся (например, Bossa Beyond), зафиксировать `CONNECTING/BUFFERING/ON AIR`, реальные паузы и скорость marquee, а также Diagnostics `I → Down` (`Fail/Buf/Under/PCM gap`) и `I → Down ×3` (`HTTPS phase/hop`, DNS/TCP+TLS ms, heap/largest, Buf min, UIgap). Затем проверить STOP/смену во время подключения и volume-only без нового reconnect; задержку STOP считать отдельно от GET. Только после короткого успеха делать полный gate: пять станций ×30 мин, ≥2 ч непрерывного эфира, Wi-Fi reconnect, offline SD, 2 Wi-Fi профиля, Backup, Clock/NTP/Alarm/SD/QR/клавиши.
