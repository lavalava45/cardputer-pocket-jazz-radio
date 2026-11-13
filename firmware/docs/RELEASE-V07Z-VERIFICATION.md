# Pocket Jazz Radio — v0.7z: выпуск и проверки 11.11.2026

**Итог:** immutable software release v0.7z готов; **hardware gate NOT TESTED**. Предыдущие v0.7w, v0.7x и v0.7y сохранены без изменений, автоматической прошивки, изменения M5Launcher 3, разделов, NVS и пользовательской SD не было.

## Изменения после v0.7y

1. **HTTP→HTTPS redirect.** В установленной Arduino-ESP32 3.3.9 `HTTPClient::setURL()` отклоняет URL другой схемы, поэтому обычные HTTP-потоки с 301/302/303/307/308 на HTTPS раньше завершались ошибкой, хотя ресурс мог существовать. Новый `SafeHttpsUpgrade.h` принимает только короткий (до 191 байта) абсолютный HTTPS URL без userinfo, запрещённых символов, fragment, недопустимого порта и недопустимой схемы. Plain HTTP socket закрывается до создания следующего `SecureICYSource`. Проверка CA и исходного имени хоста/SNI сохранена; нет HTTPS→HTTP downgrade или одновременного второго соединения. Промежуточные HTTP→HTTP перенаправления по-прежнему выполняет HTTPClient, максимум пять.
2. **Устаревший запрос после задержанного GET/prefill.** Если UI успел поставить STOP или смену станции в очередь во время синхронного GET либо prefill, audioWorker теперь освобождает старый источник и не начинает старый decoder. После успешного GET `audioWorker` восстанавливает исходный приоритет 2; в GET на core1 он временно использует приоритет 1 (UI priority), предотвращая явный priority starvation в прежней конфигурации.
3. Сохранены v0.7y: инкрементальный plain-HTTP ICY без 2,5-с блокировки в `readNonBlock`, dechunk HTTP до ICY/MP3, bounded HTTPS metadata, исправленный Buf/минимальное заполнение, фазовая HTTPS-диагностика и UIgap. Размер MP3 ring 32768 Б, prefill 2048 Б, NVS/Backup/Wi-Fi/offline MP3/Clock/Alarm/Likes/Diagnostics не изменялись.

## Фактически выполненные software проверки

Команда: `powershell -NoProfile -ExecutionPolicy Bypass -File tools/build.ps1 -Version 0.7z -PackageUnverifiedPManSize`. **Код возврата 0**.

| Проверка | Результат | Подтверждение внутри релиза |
|---|---|---|
| Python unittest discovery | **201/201 PASS**, 76,913 с | `TEST-REPORT.txt` |
| MSVC x64 Release CTest | **40/40 PASS**, 1,13 с | `HOST-CTEST-REPORT.txt` |
| C++ Buf reset/minimum | **CTest №9 PASS**, asserts включены в Release | `HOST-CTEST-REPORT.txt` |
| C++ ICY partial/chunked/timeout/reconnect | **CTest №16 PASS**, asserts включены в Release | `HOST-CTEST-REPORT.txt` |
| C++ HTTPS upgrade security negatives | **CTest №17 PASS**, asserts включены в Release | `HOST-CTEST-REPORT.txt` |
| Основная ESP32-S3 Arduino 3.3.9 | **PASS**, flash 1 575 403 Б (47%), static RAM 111 188 Б (33%) | `ARDUINO-COMPILE-REPORT.txt` |
| Compile-only ESP-NOW HMAC/mbedTLS, HA MQTT/TLS, PCM mirror v0.8e | **3/3 PASS** | `ARDUINO-COMPILE-REPORT.txt` |
| Source SHA256, artifact SHA256, ZIP CRC и uniqueness | **PASS** | `source/SHA256-SOURCES.txt`, `SHA256-MANIFEST.txt`; отдельно проверено Python + `Get-FileHash` |

Проверено **230 отдельных файлов** в frozen `source` с SHA256 каждого файла и отсутствием незарегистрированных файлов. Среди них `PocketJazzRadio/SafeHttpsUpgrade.h`, `arduino-sketchbook/libraries/ESP8266Audio/src/AudioICYIncremental.h`, `AudioFileSourceICYStream.cpp/.h`, `tests/safe_https_upgrade_test.cpp`, `tests/radio_buffer_level_policy_test.cpp`, `tests/icy_incremental_stream_test.cpp`, `tests/pcm/CMakeLists.txt`, `docs/RELEASE-V07Y-VERIFICATION.md`, `docs/HTTPS-TRACE-V07X.md`. Архив ZIP — 15 записей, CRC PASS, без дублей. Build provenance зафиксирован, но **полный toolchain и все транзитивные зависимости не входят в frozen snapshot**: это выбранный проверенный набор исходников, а не hermetic архив.

## Проверенные контрольные суммы

Корень: `E:\Downloads\devspace-test\cardputer-adv\apps\pocket-jazz-radio\dist\releases\0.7z\`.

| Файл в релизе | Размер, байт | SHA-256 |
|---|---:|---|
| `firmware/PocketJazzRadio-v0.7z-app-only.bin` | 1 575 552 | `4445b0527aef112e0477db3a696759f2b00e83c4aa1b4453814760d5720ea189` |
| `PocketJazzRadio-v0.7z-Launcher3-SD.zip` | 1 050 660 | `965fa77f0c4c39611735b694dbbe554b19887105d16bdb6f7b3e616505f5a2d2` |
| `TEST-REPORT.txt` | 33 349 | `b632a84042cf698661fcee476dd6fe21c0262e5b682829c0802763ea9a3c375f` |
| `HOST-CTEST-REPORT.txt` | 23 026 | `fa5f7bcd743a22f56e51e262214bcf6d65e56fa8906982884639d60d92fc331f` |
| `BUILD-INPUTS.txt` | 8 007 | `78fa70c3bd392c2bd17e6bb37c4c3017c33dd77562238bc28fb302e2c1383c44` |
| `ARDUINO-COMPILE-REPORT.txt` | 1 060 | `5245486f07c97a773b30ddf0becf66b138edff026507855a760118ec61cc9490` |

Хеши проверены после создания неизменяемого каталога, не взяты только на доверии из выводов упаковщика. Frozen `dist/releases/0.7z/` **не изменять**. Настоящий документ создан после freeze в рабочей документации, а точные machine-readable отчёты уже сохранены в релизе.

## Что ещё неизвестно и что нельзя делать

- **Физический эффект не измерен:** отказ Swiss Jazz (оба ранее проверенных DIRECT/REDIRECT), TSF Jazz France и KJazz, многосекундные паузы на Bossa Beyond/181.fm, рост Underruns и рывки marquee исходной v0.7v нельзя считать исправленными только по host PASS. v0.7z не меняет алгоритм TLS handshake для уже прямых HTTPS URL; остаётся возможной отдельная причина их отказа (включая ограничение contiguous heap).
- **STOP во время уже выполняющегося `HTTPClient::GET()` ещё не может немедленно прервать DNS/TCP/TLS/header read.** Проверка новой очередной команды предотвращает дальнейший запуск устаревшего потока после GET/prefill, но отдельная гонка между последней проверкой очереди и первым коротким шагом декодера может дать краткий переход PLAYING до следующей обработки команды. Архитектурно безопасное прерывание network call без лишнего TLS и риска RAM остаётся в ROADMAP. Реальные STOP latency/PCM gap ещё не измерены.
- **Физический APP-слот M5Launcher 3 неизвестен.** Бинарник 1 575 552 Б превышает консервативный предел упаковщика 1 376 256 Б. `NOT CLEARED FOR INSTALL` означает необходимость read-only проверки PMan и согласия владельца перед ручным запуском; **никаких автоматических flash/repartition/NVS/SD операций**.

## Минимальный аппаратный протокол (после подтверждения вместимости APP-слота)

Один раз вручную запустить v0.7z через M5Launcher 3 на существующей SD, не переписывая пользовательские плейлисты/настройки. Уже проведённый DIRECT/REDIRECT Swiss A/B **не повторять**. Проверить одну прежде неработающую станцию (Swiss Jazz **или** TSF Jazz) и одну прерывавшуюся (например, Bossa Beyond), по 1–2 минуты; записать звук/паузы/рывки и два экрана Diagnostics: `I → Down` (`Fail`, `Buf`, `Under`, `PCM gap`) и `I → Down×3` (`HTTPS phase/hop`, DNS/TCP+TLS ms, heap/largest до/после, `Buf min`, `UIgap`). Если они стабильны, отдельный полный hardware gate — пять станций по 30 мин, ≥2 ч непрерывного эфира, STOP/switch, Wi-Fi recovery, offline SD и RTC/NTP/alarm. Ничего из этого не включено в software PASS.
