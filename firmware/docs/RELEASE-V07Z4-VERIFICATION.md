# Pocket Jazz Radio v0.7z4 — software gate, 12.11.2026

**Immutable software release PASS; звук на Cardputer ADV NOT YET TESTED.**

Путь: `dist/releases/0.7z4/`.

| Артефакт | Размер / SHA-256 |
| --- | --- |
| `firmware/PocketJazzRadio-v0.7z4-app-only.bin` | **1 587 600 Б**, `e4d82a9cb5b04de1f9a3357af427ee7ba25ed8fc3e05c3d61af4e298451efcb9` |
| `PocketJazzRadio-v0.7z4-Launcher3-SD.zip` | **1 056 766 Б**, `91572561a185ed51fcb8b4f878e3c94b3ae8630e8aae4aeae2fcdcaa59466866` |

Проверено независимо **6/6 SHA256-манифестов**, **256/256 SHA256 исходных файлов**, **15 уникальных записей ZIP**, ZIP CRC PASS, firmware внутри ZIP **побайтово совпадает** с отдельным BIN. Frozen v0.7z3 BIN SHA-256 `41a94b6b095226236a79d2952cb2138289ada65e9a84b8709913c527802921ee` остался неизменным.

- **230/230 Python unittest PASS** (source contracts, диагностика, безопасность, станции, регрессии).
- **46/46 MSVC x64 Release CTest PASS**, включая `wifi_encrypted_migration_no_downgrade_two_profiles_power_cut_and_cleanup` (само шифрование на ADV не активировано) и тесты audio runtime.
- Основное приложение Arduino-ESP32-S3 (m5stack core 3.3.9, **8 MB flash, PSRAM disabled**) **PASS**, как и 3 compile-only probes (ESP-NOW HMAC, HA MQTT TLS, PCM mirror).
- Sketch **1 587 455 Б**, статическая RAM **114 044 Б**, reported free from static memory budget **213 636 Б** из **327 680 Б**. Эти числа **не равны** свободному runtime heap/крупнейшему блоку TLS.

## Что поменялось относительно v0.7z3

1. **TLS memory hypothesis:** раньше `audioWorker` постоянно держал `STREAM_BYTES=32768` Б и `MP3_CODEC_BYTES=29192` Б во время `CONNECTING/RETRYING`. Теперь ring выделяется **только после успешного source open**, codec — непосредственно перед декодированием. `cleanup()` закрывает decoder/buffer/source **до освобождения** ring+codec. Offline MP3 выделяет только codec. Размеры, SSL CA/hostname verification, timeout и ring-fill policy **не менялись**.
2. **Diagnostics UI:** диагностические значения теперь белые и имеют фиксированный размер шрифта для определённой строки/страницы; заголовки остаются золотыми. Ранее `textWidth(line)` в зависимости от длины счётчика переключал масштаб 1×/2×, а DNS/TLS перекрашивал строку в зелёный или красный.
3. **Wi-Fi credentials:** лишь host-side эксперимент с миграцией (запрет неявного downgrade) — **не** рабочее шифрование. Реальное NVS на ADV продолжает хранить открытые копии паролей.

**Почему релиз экспериментальный:** аппаратный v0.7z3 показал `TLS raw=-32512=MBEDTLS_ERR_SSL_ALLOC_FAILED`, без звука на Swiss Jazz, позднее пользователь сообщил отсутствие звука на других станциях в этой сессии. Новая схема освобождает 61 960 Б **до** TLS, но **после** успешного TLS эти блоки ещё нужно выделить для воспроизведения; если heap исчерпан, звук не начнётся. Для доказательства нужны физический `MEMORY 7/8` и `SYSTEM HEALTH 8/8` + TSV, а не только CTest.

**Запуск:** использовать только `app-only.bin` через действующий M5Launcher 3. Не путать с `dist/build/*.merged.bin` (8-МБ образ **только эмулятора**), не менять загрузчик, таблицу разделов, NVS и личную microSD. См. [аппаратный журнал](HARDWARE-OBSERVATIONS-20261110.md) и [план](../ROADMAP.md).
