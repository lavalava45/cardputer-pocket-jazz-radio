# Pocket Jazz Radio v0.7z5 — software verification (12.11.2026)

**Immutable software release PASS; REAL AUDIO / DISPLAY ON ADV NOT TESTED.**

Каталог: `dist/releases/0.7z5/`. Установке на реальное устройство не подвергался; предыдущие выпуски и пользовательские microSD/NVS/eFuse/Launcher не изменялись.

| Артефакт | Проверенные данные |
| --- | --- |
| `firmware/PocketJazzRadio-v0.7z5-app-only.bin` | **1 591 488 Б**, SHA-256 `6983c0d82ba14ecf87a0544adb006a3828f2048756b18069a299bc2f9a715ba4` |
| `PocketJazzRadio-v0.7z5-Launcher3-SD.zip` | SHA-256 `7f26e8f64042ac54bec9dc58a99bb16325ea09319a0f128c3e70bf86a24fd9ca` |
| Программный gate | **233/233 Python unittest PASS**, **47/47 MSVC x64 Release CTest PASS** |
| Arduino-ESP32-S3 | Основной app **PASS**, **3/3 compile-only probes PASS**, flash 8 MB, PSRAM disabled, m5stack Arduino core 3.3.9 |
| Статическая память | **114 044 Б**, компилятор сообщает оставшиеся **213 636 Б** из объявленного бюджета 327 680 Б (не измеренный runtime TLS heap) |
| Независимый контроль | 6/6 release manifest SHA-256, **261/261 frozen исходных файлов**, ZIP CRC/15 уникальных entries, BIN внутри ZIP совпадает побайтово, version header `v0.7z5` |
| Неизменность прошлой версии | v0.7z4 app SHA-256 остаётся `e4d82a9cb5b04de1f9a3357af427ee7ba25ed8fc3e05c3d61af4e298451efcb9` |

## Что изменено

1. **Diagnostics 1–8/8:** один `setTextFont(1)` / `kDiagFontScale=1` без условий по длине строки, странице или текущему статусу. Все показатели белые, названия разделов золотые. Обновлены заголовки и имена измерений; убраны отдельные криптические `UI/A/H/D/RETRY:XXXXms`, числовая причина reset переводится в человекочитаемый текст. Сохранены 8 страниц, навигация, клавиши `E/X/DEL`, текущие метрики и штатная секундная частота redraw. [Справочник экрана](DIAGNOSTICS-GUIDE.md).
2. **Countdown:** `NEXT ATTEMPT IN: Ns` теперь означает время **до будущего открытия** (секунды, не прошедшие миллисекунды). Сброс с 0 до 8 секунд после очередной неуспешной попытки — закономерный новый цикл backoff, а не скачок старого счётчика возраста стадии. Алгоритм `retrySecondsRemaining` в отдельном host-testable `DiagnosticDisplayPolicy.h`, CTest с `millis()` rollover.
3. **Аудио память:** фото реального v0.7z4 показало `TLS AFTER 91080/31732` (largest **31 732 Б**), хотя последующая allocation MP3 ring требовала непрерывные **32 768 Б**. При TLS success и `FAIL: 2/0`, BUF0 это согласуется с тем, что ring не мог быть создан (хотя в прежнем коде номер 2 объединял разные причины). Теперь `ring` резервируется **до** TLS, а `libmad codec=29192 Б` — только после реального MP3 prefill. Оба блока освобождаются строго после удаления decoder/source. Это гипотеза, а не подтверждённое исправление на ADV. CA/hostname, SDK TLS и размеры MP3 ring/prefill/watchdog/PCM не ослаблялись.
4. **Разделены failure codes:** 1 network open, 2 buffer object, 3 decoder init, 4 decoder loop, 5 contiguous ring allocation, 6 codec allocation, 7 no MP3 bytes after successful open, 0 none. Страница 4 показывает номер вместе с расшифровкой. Это позволит независимо диагностировать следующий эксперимент.

## Аппаратная приёмка (не проведена)

**Дополнительная проверка после заморозки выпуска:** esp-emulator v0.51.0 в WSL/Ubuntu, ESP32-S3, 8-МБ flash, PSRAM=0: реальный merged v0.7z5 ROM/bootloader запускается, но полноценное приложение останавливается на неподдерживаемом `Keyboard: Unsupported board type: 137`. Отдельный **эмуляторный тест TLS/heap** с близким объёмом статической памяти (113 876 Б против 114 044 Б) в **3/3 независимых запусках** успешно резервировал ring 32 768 Б до TLS, подключался к `stream.srg-ssr.ch:443` со строгим CA/hostname и выделял codec 29 192 Б при ещё открытом TLS. Последний крупнейший блок 31 732 Б наблюдался *после* обеих аллокаций; это не повторение ошибки из v0.7z4, где 31 732 Б оставалось *до* выделения ring. [Данные и логи](EMULATOR-VALIDATION-20261112.md). **Эмулятор не подтвердил работу M5-дисплея/клавиатуры/аудио и не воспроизвёл долгосрочную фрагментацию, поэтому аппаратный статус остаётся NOT TESTED.**

На настоящем ADV загрузить **только app-only BIN через существующий M5Launcher 3**. Не записывать в устройство `*.merged.bin`, bootloader/partition/NVS и не менять личную SD. Проверить 1) единый размер строк на всех восьми страницах, 2) ясность `NEXT ATTEMPT IN` и `Failure`, 3) одну станцию Swiss Jazz, затем WBGO. При неудаче до reset сохранить `HTTPS 3/8`, `RADIO STREAM 4/8`, `FREE MEMORY 7/8`, `SYSTEM STATUS 8/8` и `E` на 8/8 для `/PocketJazz/audio-runtime.tsv`. По одному только успешному Arduino build/host CTest нельзя заявлять о настоящем звуке или достаточном непрерывном heap в текущем runtime.
