# Pocket Jazz Radio — Cardputer ADV

## Новый исходный кандидат preview v0.8.0 — Jazz Journey, Jazz Notes, Spark (15.11.2026)

После последней публичной v0.7z16 подготовлена **ещё не опубликованная** кодовая и редакционная версия: 24 урока Jazz Journey, 30 Learn to Listen, Jazz Notes с 1 498 карточками, 595 точными подписями Now Playing/Explained, 150 Spark, 36 альбомными рекомендациями, дневником, поиском по офлайн-индексу. Из 1 498 биографий **63 исходные + 532 дополнительные получили индивидуальную редакционную базу**, ещё **903 остаются с исходными/автоматически обогащёнными данными без персонального источникового подтверждения**. Их предварительный массовый отбор документирован отдельно (748 ссылок-зацепок, 103 недостаточных, 52 неоднозначных) и **не называется доказательством**. Новые функции должны быть проверены свежей сборкой ESP32-S3 и host-регрессией; физический звук и работа клавиатуры Cardputer ADV ещё не прошли новую приёмку. Публикация GitHub и каталоги не подразумевают перепрошивку устройства, изменения M5Launcher/NVS или личной microSD.


## Актуальный статус на 14.11.2026

Последний **публичный app-only preview — v0.7z16**, [GitHub Releases](https://github.com/lavalava45/cardputer-pocket-jazz-radio/releases/tag/v0.7z16). Рабочее дерево содержит **НЕ собранные** изменения после этого релиза: объединённый `R`-отчёт диагностики, Jazz Journey (4×6 уроков), 30 Learn to Listen, личный паспорт/дневник и связанные переходы Jazz Notes / Spark. Содержимое SD и установленный M5Launcher не менялись. **Не устанавливать рабочее дерево как прошивку:** новая ESP32-компиляция, RAM и физические проверки ещё не выполнялись по требованию владельца. См. `ROADMAP.md`, `docs/CHANGELOG.md` и `docs/JAZZ-JOURNEY.md`. Публичный GitHub уже обновлён концепцией «Not just another internet radio. A pocket-sized journey through jazz.»; функции в публикуемом описании обозначены как будущее.

---

## Последний immutable software release — v0.7z5 (12 ноября 2026)

**Для физической проверки звука и диагностики:** `dist/releases/0.7z5/firmware/PocketJazzRadio-v0.7z5-app-only.bin` — **1 591 488 Б**, SHA-256 `6983c0d82ba14ecf87a0544adb006a3828f2048756b18069a299bc2f9a715ba4`. ZIP `PocketJazzRadio-v0.7z5-Launcher3-SD.zip`, SHA-256 `7f26e8f64042ac54bec9dc58a99bb16325ea09319a0f128c3e70bf86a24fd9ca`. Software gate **233/233 Python, 47/47 MSVC Release CTest, ESP32-S3 main + 3 compile-only PASS**; все манифесты и архивы прошли независимую SHA/CRC проверку. [Отчёт v0.7z5](docs/RELEASE-V07Z5-VERIFICATION.md), [руководство по 8 страницам Diagnostics](docs/DIAGNOSTICS-GUIDE.md) и [актуальные задачи](ROADMAP.md).

**В v0.7z5:** один фиксированный шрифт для всех восьми страниц, понятные имена метрик и ошибок, `NEXT ATTEMPT IN` вместо возраста `RETRY` в миллисекундах. По фото v0.7z4 стало ясно: TLS уже проходил, но крупнейший блок памяти после него **31 732 Б**, меньше требуемых для MP3 ring **32 768 Б**. Поэтому ring в v0.7z5 резервируется **до TLS**, а codec — только после prefill. **Это пока эксперимент; реальный звук на v0.7z5 не проверен.** Пользовательский M5Launcher, разделы и SD не изменялись.

## Предыдущий immutable software release — v0.7z4 (12 ноября 2026)

**Архивный выпуск v0.7z4:** `dist/releases/0.7z4/firmware/PocketJazzRadio-v0.7z4-app-only.bin` (**1 587 600 Б**, SHA-256 `e4d82a9cb5b04de1f9a3357af427ee7ba25ed8fc3e05c3d61af4e298451efcb9`). SD ZIP `PocketJazzRadio-v0.7z4-Launcher3-SD.zip`, SHA-256 `91572561a185ed51fcb8b4f878e3c94b3ae8630e8aae4aeae2fcdcaa59466866`. Software gate **230 Python / 46 C++ Release CTest**, ESP32-S3 + 3 compile-only PASS, SHA 256/256 и ZIP CRC PASS. В отличие от v0.7z3, MP3 ring/codec освобождаются до TLS retry, Diagnostics сохраняет постоянные белые шрифты. **Физический тест v0.7z4 проведён: Swiss Jazz, Sonic Universe и WBGO не играли; HTTPS READY, FAIL=2 и после TLS largest 31 732 Б.** [Проверка](docs/RELEASE-V07Z4-VERIFICATION.md) · [Эмуляторы и TLS/RAM тест](docs/EMULATOR-VALIDATION-20261112.md) · [ROADMAP](ROADMAP.md).

## Предыдущий immutable software release — v0.7z3 (12 ноября 2026)

**v0.7z3** опубликована отдельно: app-only BIN **1 587 664 Б**, SHA-256 `41a94b6b095226236a79d2952cb2138289ada65e9a84b8709913c527802921ee`, 226/226 Python и 45/45 CTest, ESP32-S3 + три compile-only PASS. [Проверка](docs/RELEASE-V07Z3-VERIFICATION.md) · [актуальный ROADMAP](ROADMAP.md). В прошивке добавлены живые фазы диагностики PLAYER, раздельные события сети и PCM ожидания, восьмая страница SYSTEM HEALTH и ручной экспорт `audio-runtime.tsv`. **Позднее физический тест выявил Swiss Jazz TLS raw=-32512 (MBEDTLS_ERR_SSL_ALLOC_FAILED); звук на v0.7z3 не заработал в этой сессии.** Ранее использованный консервативный предел размера был правилом упаковщика, **не доказанной спецификацией M5Launcher**; новый упаковщик не блокирует app-only BIN по этому порогу. Использовать только app-only BIN, не merged/bootloader/partitions.

**Host-only политика безопасности Wi-Fi (входит в исходники v0.7z4):** добавлена host-only политика безопасной миграции Wi-Fi-паролей, 2/2 целевых C++ Release CTest PASS. **Это ещё не шифрование паролей на Cardputer:** SDK NVS encryption отключён, аппаратный ключ не подтверждён, приложение всё ещё пишет plaintext NVS. [Что реализовано и чего пока нет](docs/WIFI-CREDENTIAL-SECURITY.md).

## Исторический выпуск — v0.7z2 (11 ноября 2026)

**Опубликован immutable v0.7z2; полный software gate PASS (212/212 Python, 43/43 MSVC Release CTest, ESP32-S3 + 3 compile-only, 243/243 source SHA-256, ZIP CRC PASS). Физический звук NOT TESTED.** BIN **1 578 000 Б**, SHA-256 `03cadf6b658963f3512a177fba345ea1dadb52903cadce562fd3f840ecceb7df`; Launcher3 ZIP SHA-256 `e9ffa97d8b56413e86986d438c39fb192e684f77ec143c0008a1f03f7b0325de`. [Отчёт](docs/RELEASE-V07Z2-VERIFICATION.md). **Без read-only подтверждения APP-слота PMan на M5Launcher 3 НЕ УСТАНАВЛИВАТЬ**: BIN превышает консервативные 1 376 256 Б.  Подтверждённый дефект v0.7z1: MP3 decoder ошибочно завершал любой живой поток с конечным `Content-Length` при первом кратком отсутствии данных, даже когда `getPos()<getSize()`. Теперь временный провал обрабатывается тем же ограниченным ожиданием (4 с), а не ложным EOF. Дополнительная рецензия выявила, что `Content-Length` включает ICY-метаданные, а `getPos()` учитывает только MP3: добавлен отдельный счётчик прочитанных байтов HTTP-body и прогнозируемый конец аудио, чтобы после реального конца конечного ICY не возникало новой лишней 4-секундной паузы. HTTP chunk framing в body не учитывается; CA/hostname остаются обязательными. Инкрементальный ICY не переписывался.

**Для аппаратной диагностики:** `I → Down×4` открывает 5-ю страницу с максимальными длительностями source pull, MP3 decoder loop и M5.Speaker `flush` (включая ожидание освобождения PCM-буфера), `UIgap`, `Under`/`Buf`. Это числовые показатели из прежней audio task без второго сокета и Serial в audio callback; **они не доказывают акустическую стабильность**. `I → Down×3` по-прежнему показывает HTTPS DNS/TCP+TLS/GET, heap и Buf min, `X` сбрасывает отображаемые максимумы. Прежние функции и разделы SD/NVS сохранены; ring/prefill и FreeRTOS приоритеты не повышались. [Минимальная аппаратная приёмка](docs/AUDIO-ACCEPTANCE-V07Z2.md) · [ROADMAP](ROADMAP.md).


**Ретроспективный Codex-аудит frozen v0.7w:** три P1 уже исправлены в v0.7y и сохранены в v0.7z2: неблокирующая ICY metadata в plain HTTP, безопасный потоковый парсер длинного `StreamTitle`, dechunk plain HTTP до ICY/MP3. Существующие MSVC Release-тесты проверяют фрагментацию, длинные/некорректные названия, chunked, тайм-ауты и reconnect. Нового кода по этому старому аудиту не требуется; физические многосекундные PCM-паузы остаются неподтверждёнными на v0.7z2.

## Предыдущий immutable software release — v0.7z1 (11 ноября 2026)

В рабочей версии после immutable v0.7z устранена подтверждённая несовместимость допустимого URL `https://host?query` с Arduino-ESP32 3.3.9: для HTTP→HTTPS upgrade добавляется `/` перед `?` с проверкой 192-байтного буфера; прямой HTTPS и HTTPS→HTTPS редиректы нормализуются аналогично. CA, проверка hostname/SNI и запрет HTTPS→HTTP downgrade сохранены.

**Второе исправление по исходникам:** прежняя проверка *наличия любого сообщения* в `audioQueue` во время GET/prefill прерывала соединение даже при простом изменении громкости (в том числе плавном fade будильника). Теперь PLAY/STOP/смена URL публикуют поколение намерения перед командой, а volume-only поколение не меняет. Проверки отмены установлены до и после `decoder->begin()`, перед первым PCM loop; старое инкрементальное HTTP ICY из v0.7y и ring/prefill сохранены. Синхронный DNS/TLS/GET по-прежнему нельзя немедленно остановить, а аппаратная устойчивость звука на Cardputer **не доказана**. Это программные исправления, а не подтверждение исчезновения Swiss Jazz/TSF/KJazz RETRYING или PCM-пауз. [Незавершённое](ROADMAP.md) · [Аппаратный журнал](docs/HARDWARE-OBSERVATIONS-20261110.md).

**Полный release gate PASS: 204/204 Python, 41/41 MSVC Release CTest (с действующими assert при NDEBUG), ESP32-S3 + 3/3 compile-only PASS.** App-only BIN: `dist/releases/0.7z1/firmware/PocketJazzRadio-v0.7z1-app-only.bin` (1 576 448 Б), SHA-256 `0eefb81e8ce934add809737db8b0af3d166a9d7e3d765ac333f78606ae4c563a`. SD ZIP: `dist/releases/0.7z1/PocketJazzRadio-v0.7z1-Launcher3-SD.zip`, SHA-256 `80da0c5c337eb10d24df8eabc79f3026ba44fc910160386e486092bc50d67074`. Frozen `source/` **234/234 SHA-256 PASS**, отчёты Python/CTest/Arduino, BUILD-INPUTS, manifest, ZIP CRC PASS. [Итоговый verification v0.7z1](docs/RELEASE-V07Z1-VERIFICATION.md). **Hardware gate NOT TESTED; NOT CLEARED FOR INSTALL** до проверки фактического размера APP-слота через PMan; M5Launcher, flash, NVS, SD без изменений.

## Ещё более ранний immutable software release — v0.7z (11 ноября 2026)

**v0.7z — immutable software release, gate PASS:** **201/201 Python, 40/40 MSVC Release CTest, ESP32-S3 и 3 compile-only PASS**. Plain-HTTP 301/302/303/307/308 → HTTPS теперь выполняется единственным audioWorker через проверенный и ограниченный `Location`: HTTP socket закрывается до создания `SecureICYSource` с CA/hostname. После задержанного GET/prefill новая команда STOP или выбора станции не даёт начать прежний MP3 decoder. Сохранены исправления v0.7y: неблокирующий ICY, dechunk до MP3, bounded HTTPS metadata, Buf/Buf min и UIgap. App-only BIN: `dist/releases/0.7z/firmware/PocketJazzRadio-v0.7z-app-only.bin`, **1 575 552 Б**, SHA-256 `4445b0527aef112e0477db3a696759f2b00e83c4aa1b4453814760d5720ea189`. Launcher SD ZIP: `dist/releases/0.7z/PocketJazzRadio-v0.7z-Launcher3-SD.zip`, SHA-256 `965fa77f0c4c39611735b694dbbe554b19887105d16bdb6f7b3e616505f5a2d2`. Frozen source (230 файлов), BUILD-INPUTS, CTest/Python/Arduino отчёты и manifest — в том же каталоге. [Достоверный release gate](docs/RELEASE-V07Z-VERIFICATION.md). **Аппаратный звук не проверен.**
**Ограничение:** установленная v0.7v имела Swiss Jazz/TSF/KJazz `RETRYING`, многосекундные PCM-паузы и рывки marquee. v0.7z не испытана на физическом ADV; работа прямого HTTPS и длительного эфира не доказана. Синхронный GET всё ещё нельзя отменить мгновенно во время handshake. `NOT CLEARED FOR INSTALL`: BIN 1 575 552 Б превышает консервативный APP-лимит 1 376 256 Б; до ручной установки нужна read-only проверка фактического APP-слота PMan. Ни M5Launcher 3, ни разделы flash, SD/NVS автоматически не изменялись.


## Предыдущий immutable software release — v0.7y (11 ноября 2026)

**Текущая задача P0:** на физическом ADV с v0.7v Swiss Jazz DIRECT/REDIRECT, TSF Jazz France и KJazz Los Angeles не подключались; многие другие станции переходили `CONNECTING → BUFFERING → ON AIR` с несколькими секундами тишины, ростом `Underruns` и заметными рывками прокрутки. v0.7w и v0.7x имеют отдельные software-релизы, но на железе эти отказы не устранены и не объявлены исправленными.

**v0.7y — исправления общего аудиопути, полный software gate PASS и отдельная аппаратная приёмка.** Обычный HTTP `AudioFileSourceICYStream::readNonBlock()` раньше ждал неполный ICY block до 2,5 секунды, удерживая аудиозадачу. Теперь метаданные обрабатываются инкрементально с фиксированной памятью и сохранением состояния, общий deadline остаётся 2,5 с, и неблокирующий вызов не ждёт байты. Добавлены обработка HTTP chunked перед ICY/MP3, ограниченный per-poll budget для HTTPS ICY (256 Б), fair scheduling UI при TLS/open, исправление stale Buf из v0.7x, диагностика фаз и min fill. `MP3 ring=32768`, `prefill=2048`, строгий TLS CA/hostname и прочие функции сохранены. **198/198 Python, 39/39 MSVC Release CTest, ESP32-S3 и 3 compile-only PASS**, SHA256 checked. App-only `dist/releases/0.7y/firmware/PocketJazzRadio-v0.7y-app-only.bin`, **1 574 640 Б**, SHA-256 `39c8608e42af926714e35a1235e8342c2365892e106072c405bf2cdc350cfefc`. SD ZIP `dist/releases/0.7y/PocketJazzRadio-v0.7y-Launcher3-SD.zip`, SHA-256 `28b2ac9e249499823110012ba0c3a60fb10c9cd266ad85120c199648c9889070`. [Точный release-gate, source и отчёты](docs/RELEASE-V07Y-VERIFICATION.md) · [Клавиши](docs/KEYS.md) · [Аппаратный журнал](docs/HARDWARE-OBSERVATIONS-20261110.md).

**Граница достоверности:** компиляция и host-тесты не измеряют реальную работу на ES8311. Новый релиз не меняет M5Launcher 3, загрузчик, разделы, NVS или пользовательскую SD и не прошивался автоматически; до отдельного ручного испытания эффект на реальные паузы и Swiss Jazz остаётся неизвестным. **Hardware gate NOT TESTED; установка не разрешена без проверки реального APP-слота PMan:** BIN 1 574 640 Б превышает консервативный лимит упаковщика 1 376 256 Б. Не менять разметку без отдельного решения.

## История: v0.7w (11 ноября 2026)

**v0.7w — каталог 56 MP3-станций и полный сброс аудиосчётчиков по X.** Из стандартного набора убраны два отдельные блюзовые канала (`181.fm True Blues` и `Jazz Radio Blues`), добавлены 14 HTTPS/MP3 станций, проверенных на ПК по реальному потоку и встроенной CA-цепочке. Лимит 64: восемь мест для дополнительных станций, если пользовательская SD не содержит других M3U. `I → X` теперь сбрасывает **отображаемые `Under`, `Opens`, `PCM frames` и PCM/STOP историю** без изменения внутренних raw-счётчиков. **Software gate PASS: 191/191 Python, 37/37 C++ MSVC Release, ESP32-S3 и 3 compile-only**; BIN `dist/releases/0.7w/firmware/PocketJazzRadio-v0.7w-app-only.bin` (**1 570 128 Б**, SHA-256 `214c2668f657e7b5ac764efcc2129aac8b85d55141b34515ee411fcd4299af39`), SD ZIP `dist/releases/0.7w/PocketJazzRadio-v0.7w-Launcher3-SD.zip` (SHA-256 `06bb2042d57695c116dce9ff13d97e787b45318af01be420e21b75b5e2c5db7f`). **На физическом ADV v0.7w пока НЕ ИСПЫТАНА; заикания v0.7v НЕ исправлены данным релизом.** [Каталог и безопасная миграция SD](docs/STATION-CATALOG-56-20261111.md) · [Текущий hardware log](docs/HARDWARE-OBSERVATIONS-20261110.md). Личная `04-my-stations.m3u` и прочие пользовательские файлы не изменялись автоматически.

**v0.7v — уменьшение постоянно занятой RAM и кандидат на восстановление HTTPS после подтверждённой пользователем версионной разницы v0.7o (звук есть) и v0.7u (CONNECTING/RETRYING, `Fail 1/-1`, `WiFi STABLE`).** Backup settings workspace перенесён из постоянной DRAM в heap *только на время ручного Backup*, с RAII-освобождением и без использования UI stack. По данным ESP32-S3 compile statically occupied RAM **124 180 → 111 108 Б (−13 072 Б)**. `TLS err` переименован в более точное `TLS raw` (положительное значение может быть дескриптором, не ошибкой), на Diagnostics page 2 добавлен `Largest` heap. **Software gate PASS:** 186/186 Python, 36/36 C++ Release, ESP32-S3 app, 3 compile-only. App BIN **1 570 032 Б**, SHA-256 `962483461a3dd14762af3b25da4594d02429be6968e810ca4b9e0d976ab166d2`; ZIP **1 046 618 Б**, SHA-256 `02719bb1751544d62916701ebdfc149d4ca1a3ca3a245e4e228a4f3ea86f7549`. **Физическая работа v0.7v ещё НЕ ПРОВЕРЕНА: это гипотеза, а не гарантированное исправление.** [Отчёт v0.7v](docs/RELEASE-V07V-VERIFICATION.md). Никакой flash/partition/NVS на ADV локально не изменялся.

**v0.7u — диагностическая, НЕ доказанное исправление HTTPS.** На физическом ADV установленная v0.7t постоянно показывает `CONNECTING → RETRYING`, страница Diagnostics 2: `Fail: 1/-1` (отказ открытия HTTPClient, точная причина ещё неизвестна). В новой v0.7u на **Help** и **Diagnostics** появляется фактическая версия из параметра сборки; на Diagnostics page 2 — `TLS err` (числовой mbedTLS-код), а Serial 115200 выводит ограниченные числовые ошибки без приватных URL/паролей. **Software PASS:** 183/183 Python, 36/36 MSVC C++ Release, ESP32-S3 app и 3 compile-only. APP-only **1 574 576 Б**, SHA-256 `c228b99a9305e1b80dbf3065226b001e72514f0e05029486bc4c6a30373b0cb1`; SD ZIP **1 046 382 Б**, SHA-256 `54814bcaac577ce37fdb538bc2e87708f9f27cdc6e61e6353d057644b8497163`. [Отчёт v0.7u](docs/RELEASE-V07U-VERIFICATION.md). **Новый BIN ещё не проверялся на устройстве; автоматической прошивки не было.** Размер слота больше не блокирует программную разработку; будущую переразметку рассматриваем отдельно.

**v0.7t — новый пользовательский ежедневный радиобудильник.** `Fn+T` — экран HH:MM, пресет 1–9, включение/отключение, хранение в NVS и плавное увеличение громкости за 60 секунд после начала реального PCM. Нужны включённый Cardputer и подтверждённое NTP-время; выключенное устройство не пробуждается. **Software gate PASS:** 179/179 Python, 36/36 MSVC C++ Release, ESP32-S3 app + три compile-only. APP-only **1 573 856 Б**, SHA-256 `537b0b066adc46e9bd9885a419d11078122f43941c00903930a1c65e85497e97`; SD ZIP **1 045 888 Б**, SHA-256 `b8f693b61e492747ce072ac8f6a7b043eae6e1100c937f818a5e7a689124c34e`. [Релизный отчёт](docs/RELEASE-V07T-VERIFICATION.md) · [инструкция будильника](docs/DAILY-RADIO-ALARM.md). Физическая аудио-/NTP-приёмка ещё впереди. Размер слота **не блокирует разработку**; переразметка и установка на устройство — отдельные будущие решения.

**v0.7s — отдельный immutable software PASS.** 175/175 Python, 35/35 MSVC C++ Release (действующие assert), ESP32-S3 main + 3 compile-only PASS. Экран Signal (S) показывает битрейт последнего успешно декодированного MPEG-кадра (libmad, не ICY) и метку VBR, если наблюдались разные bitrate кадров, вместе с PCM Hz. APP-only BIN 1 569 120 Б, SHA-256 9b85ba4c1352164eb1d8d2ca9f26529020a87ebda45d47ff8cfef21587efcd45; SD ZIP 1 042 614 Б, SHA-256 d67185a363e14553d540f6d1eab5794ed5cd6b776225017e5e588e93dd469249. Проверены 209/209 source SHA и 15 ZIP entries. [Полный отчёт v0.7s](docs/RELEASE-V07S-VERIFICATION.md). **NOT CLEARED FOR INSTALL** без read-only PMan. Предыдущие immutable релизы не изменены.

**Полная проверка трёх выпущенных за эту сессию версий, решения по восьми идеям Клода и открытые аппаратные приёмочные тесты:** [отчёт 11.11.2026](docs/RELEASE-VERIFICATION-20261111.md). Полный технический инвентарь функций, пригодный для обсуждения с другой нейросетью: [CURRENT-FEATURES-20261110.md](docs/CURRENT-FEATURES-20261110.md). Незавершённое — в [ROADMAP](ROADMAP.md); **не прошивать без проверки PMan**.

**v0.7r — новый immutable software PASS:** **173/173 Python, 34/34 MSVC C++ Release**, ESP32-S3 app + три compile-only пробы, отдельно SDK **23/23 PASS**. Исправлен реальный дефект PCM-телеметрии v0.7p/v0.7q: округление каждого 512-кадрового блока @44,1 кГц до 12 мс создавало ~100 мс накопленной погрешности за ~260 блоков и могло скрывать цифровые провалы. Теперь дробный остаток переносится между блоками, учёт длительности открытой нехватки прекращается на момент пользовательского STOP, X-reset не создаёт фиктивную новую станцию. APP-only BIN **1 568 624 Б**, SHA-256 `7fee4030fa42e972dc3149d3f4dfde4b7a8b924f235f2a189368db68dd4c02a5`; SD ZIP **1 070 256 Б**, SHA-256 `e0605c0c424de7b5a1ea136c893313f05b7ff4477c07289838f5edd2217676b0`. Релиз: `dist/releases/0.7r/` (source snapshot, SHA-256, BUILD-INPUTS, manifest и отчёты). **NOT CLEARED FOR INSTALL**: размер настоящего APP-слота PMan неизвестен; звук/SD/Wi-Fi v0.7r на Cardputer не испытывались. Архивы v0.7o/v0.7p/v0.7q неизменны.

**v0.7q — immutable software gate PASS:** **173/173 Python, 34/34 MSVC Release C++** с активными assert, **ESP32-S3 основная компиляция + 3 compile-only PASS**, отдельно Audio Router SDK **23/23 Release PASS**. APP-only BIN **1 568 480 Б**, SHA-256 `1d3ee836657c44bac62bc76a6bbc61aa65b7a2f810fe69c87aa78e1b27f55af1`; SD ZIP SHA-256 `f25df5418aabe85e1df10f6b9e2537583dfa270866638cb19c191e2ef278afaa`. Отдельный неизменяемый архив `dist/releases/0.7q/` содержит BIN, ZIP, `BUILD-INPUTS.txt`, исходный снимок с проверяемыми SHA-256, отчёты и manifest. **NOT CLEARED FOR INSTALL**, поскольку реальный APP-слот M5Launcher 3/PMan неизвестен. К прежней v0.7p добавлен **ручной MP3-плеер из личных файлов** SD: в RADIO сначала STOP, затем **0** открывает `/PocketJazz/offline/`; Up/Down выбирают до 24 MP3, Enter запускает/останавливает, `−/+` громкость, Del закрывает файл и возвращает к радио. Нет предзаписанной музыки, автоматического offline fallback, записи MP3 на SD, второго аудиовладельца или Spotify OAuth. ID3v2.2–2.4 и безопасные пути прошли host-тесты; реальная SD latency/звук **не проверены**. [Аппаратный протокол SD](docs/SD-OFFLINE-LIBRARY.md).

**v0.7p — новая immutable software-сборка после полного gate:** **169/169 Python, 33/33 MSVC Release C++ (assert работают)**, ESP32-S3 основная компиляция и три compile-only пробы, отдельно **Audio Router SDK 23/23 host tests**. Только app-only BIN **1 563 408 байт**, SHA-256 `d0eafc33085023faf393384a5fca6f96148c0a80d8fba9fdc539588a3181ad17`; M5Launcher 3 SD ZIP SHA-256 `8ebb5758fbcde5a29832e9d8f28545d353fdb6316685bc4da59c92710ff04d79`. Новый релиз: `dist/releases/0.7p/` с исходниками, проверенными хешами, отчётами, манифестом и BUILD-INPUTS. **NOT CLEARED FOR INSTALL** до проверки реального APP-слота PMan; физический звук, переключение двух Wi-Fi, импорт/экспорт SD ещё не приняты. Добавлены цифровые PCM-gap/STOP latency, два Wi-Fi-профиля, ручная SD-копия настроек и предупреждения о повторных отказах станций. [Актуальный полный каталог](docs/CURRENT-FEATURES-20261110.md) · [Проверки пяти станций](docs/AUDIO-FIVE-STATION-PCM-ACCEPTANCE.md) · [Wi-Fi и SD](docs/SETTINGS-BACKUP-AND-WIFI-PROFILES.md).

**v0.7o — полный software gate PASS, но физическая установка НЕ разрешена без проверки PMan.** 162/162 Python, 29/29 C++ MSVC Release, ESP32-S3 app compile и три compile-only PASS. Отдельный immutable `dist/releases/0.7o/`: app-only **1 545 200 Б**, SHA256 `e19fabb16e8685a187f8fc9a34e8822c2e2e2800b8305d02214fb9e2cb6b490f`; SD ZIP SHA256 `750666e7702bc76fda23ad927e68e61f5dfaf699e26c8b1081ac7031f124d28b`. Новое: ограниченный per-call бюджет синхропоиска/ошибочных кадров MP3 для отзывчивости STOP/смены станции в случае некорректного потока; остальные функции v0.7n сохранены. **Это software PASS, а не доказательство устранения заиканий на ES8311.** [Полный перечень возможностей](docs/CURRENT-FEATURES-20261110.md) · [Оставшаяся работа](ROADMAP.md) · [PMan только для чтения](docs/PMAN-READONLY-V07N.md) (байты инструкции относятся к v0.7n) · [CHANGELOG](docs/CHANGELOG.md).

**Первая реализация этих направлений выпущена в v0.7p и сохранена в v0.7q:** метрики PCM-handoff gaps/STOP drain/смены станции (`I` страница 2, `X` сброс, `E` ручной TSV), два Wi-Fi-профиля с двухбанковым NVS/readback, ручной экран **B → E/E экспорт или I/I импорт** настроек с SD (только STOP), RAM-предупреждения об отказах станций. Физические проверки этих функций открыты. [Wi-Fi и SD](docs/SETTINGS-BACKUP-AND-WIFI-PROFILES.md) · [Аппаратный аудиопротокол](docs/AUDIO-FIVE-STATION-PCM-ACCEPTANCE.md).

**Ниже сохранена история предыдущих версий; их клавиши не обязательно актуальны для v0.7q.** Текущая карта — [CURRENT-FEATURES](docs/CURRENT-FEATURES-20261110.md) и [KEYS](docs/KEYS.md). По сообщению владельца отсутствие мерцания на физическом LCD при статичной станции, Wi-Fi-сканировании и переходах **подтверждено**; версия установленного в ходе наблюдения BIN неизвестна ([отчёт](docs/HARDWARE-OBSERVATIONS-20261110.md)).
**История v0.7l и её последующего v0.7m:** проверка MP3-sync границы, защита ICY от drip-feed, host-only TimeShift FILE* и ограниченный ПК-бенчмарк пяти HTTPS потоков уже **выпущены в v0.7m**. Нельзя приписывать эти исправления frozen v0.7l. [История](docs/CHANGELOG.md).

**v0.7l — software gate PASS, физическая установка НЕ разрешена до просмотра PMan.** В новой версии исправлены две конкретные программные ошибки аудиотракта: кратковременное отсутствие данных живого MP3 больше не означает немедленный EOF/reopen (ограниченное 4-с ожидание), а чтение метаданных на plain HTTP больше не может зациклиться без ограничения (2,5-с deadline). При ожидании live-MP3 на экране состояние `BUFFERING`, размер буфера **32 КиБ** и prefill **2048 Б** прежние. У QR-поиска устранена ошибка при большом числе ведущих пробелов в ICY. Физическая эффективность исправлений ещё требует замеров Groove Salad, FIP, Jazz24 и Manouche.

Полный выпуск: **152/152 Python**, **23/23 MSVC Release CTest** с действующими проверками, **ESP32-S3 основная компиляция + 3 compile-only PASS**; архив в [`dist/releases/0.7l/`](dist/releases/0.7l/). APP-only `firmware/PocketJazzRadio-v0.7l-app-only.bin`: **1 543 104 байта**, SHA256 `da2b28708da3c1bbef62b0f8c1f2e810216775621dabc6820a620846e042774a`; SD ZIP SHA256 `dd55a12b3755c2a18654e315580ab6f58848a53d81eef220e05f7bff230857d6`. Отдельный 8-МиБ merged образ `dist/emulator/0.7l/` — **только для эмулятора, никогда не прошивать на ADV**. Для оценки APP-слота: [только чтение PMan v0.7l](docs/PMAN-READONLY-V07L.md).

**Velxio теперь действительно запускает Pocket Jazz BIN:** штатный Windows picker `File → Upload firmware` позволил загрузить v0.7k, затем v0.7l. На виртуальном Cardputer проверены H/все 17 страниц Help, Up/Down с циклическими переходами, Del, Clock без фальшивого NTP, +30 минут и QR радиостанции. Интернет-радио/звук/пользовательская SD в виртуальном устройстве не проверены из-за Wi-Fi/SD fixtures. [Подробный отчёт](docs/VELXIO-V07L-RUNTIME.md).

**Новый локальный путь для Likes:** `tools/likes_bridge.py` импортирует **копию** `liked.txt` на ПК, без записи в файл на Cardputer и без Spotify OAuth. Создаёт идемпотентный список, HTML/JSON с поисками, принимает вручную подтверждённую Spotify track URL. [Использование, ограничения и безопасность](docs/LIKES-PC-BRIDGE.md). Для сравнения серверной подачи MP3 есть `tools/probe_streams.py` и [результаты восьмисекундной проверки](docs/PC-STREAM-PROBE-20261110.json), **не** аппаратный тест аудио.

**Посмотреть меню Help без перепрошивки:** открыть `preview/help-v07k.html` в браузере (при необходимости заново получить его командой `python tools/render_help_preview.py`). Макет выводит 17 страниц по четыре строки на экране 240×135; Up/Down перелистывают, Del возвращает. **Это только визуальная проверка разметки**, а не эмулятор Cardputer ADV; на реальной клавиатуре стрелки пока не проверены. Релиз v0.7k не затронут.

**Перед любым запуском v0.7k в Launcher:** читать [только просмотр PMan](docs/PMAN-READONLY-V07K.md) — реальный APP-слот ещё неизвестен; **не** использовать `dist/emulator/` для установки. Для Groove Salad/FIP/Manouche/Jazz24 есть [план двухминутных сравнительных замеров](docs/AUDIO-FOUR-STATION-DIAGNOSTIC.md); без них причина слышимых прерываний не установлена.

**Актуальные материалы:** [ROADMAP.md](ROADMAP.md) — функции до v0.8+ и критерии приёмки; [docs/KEYS.md](docs/KEYS.md) — все клавиши; [docs/CHANGELOG.md](docs/CHANGELOG.md) — версии.

**v0.7k — QR-поиск последнего лайка (IMMUTABLE SOFTWARE RELEASE):** `K` сохраняет композицию на SD, `Q` открывает сначала QR текущей станции, а `Up/Down` переключают Spotify Search и Google Search по последнему успешному лайку **этого сеанса**. В браузере телефона открывается только поиск; без Spotify API/OAuth, токенов и добавления в плейлист. Gate: **129/129 Python**, **21/21 MSVC CTest**, Arduino ESP32-S3 main и три compile-only PASS. App-only BIN **1 542 480 байт**, SHA256 `71f4c71a8af81bafcc464fdc924858075fccd6932a4db5f256aa21f59c43992d`, [архив v0.7k](dist/releases/0.7k/). QR/LCD/SD/MP3 на физическом ADV ещё не проверены; **NOT CLEARED FOR INSTALL** до проверки реального APP-слота в PMan. [Контракт Likes](docs/LIKES-QR.md), [дальнейшие интеграции](ROADMAP.md).

**Дополнительный инструмент разработчика после релиза (без изменения BIN):** [локальный предпросмотр Help на 240×135](preview/help-v07k.html) с 17 страницами из настоящего C++ исходника; Up/Down листают, Del возвращает. Пересоздать: `python tools/render_help_preview.py`. Это макет геометрии интерфейса, **не эмулятор ESP32, M5GFX, Wi-Fi или звука**. Новый host-only тест предпросмотра 2/2 PASS; полный Python-прогон рабочего дерева после добавления инструмента **131/131 PASS**, но immutable v0.7k snapshot не изменялся. [План сравнения проблемных станций](docs/AUDIO-FOUR-STATION-DIAGNOSTIC.md).

**v0.7j — крупная справка и единые клавиши (IMMUTABLE):** 17 страниц Help по 4 белые строки шрифтом 2×, физические Up/Down (печатные `;`/`.`), одна клавиша выхода `Del`, Station Search — `Tab`, Spark — Up/Down, часы — только ±30 минут с сохранением в NVS. Программный gate: **127/127 Python**, **20/20 CTest**, ESP32-S3 app compile и три compile-only пробы **PASS**; app-only BIN **1 540 512 байт**, SHA256 `d1de8992f066d737fe4c316e85e77efca505cb476b56ef74cc144d71480b2888`. [Архив](dist/releases/0.7j/). Эмуляторный merged BIN только для Velxio в `dist/emulator/0.7j/`; физическое использование **NOT CLEARED FOR INSTALL** до PMan. Реальные кнопки и звук ещё не приняты.

**v0.7i — новая справка H:** 11 страниц по 6 отдельных строк, все подписи белые; переходы W/S и A/D. Software gate **127/127 Python**, **20/20 CTest**, Arduino ESP32-S3 и 3 compile-only **PASS**. [Новый релиз](dist/releases/0.7i/), app-only BIN 1 541 984 байта. Нужна аппаратная проверка LCD, PMan APP-слота и Wi-Fi/MP3; **NOT CLEARED FOR INSTALL**. Архив v0.7h не затронут.

**v0.7h: расширенная офлайн-энциклопедия и интерфейс.** Архивный релиз с исходниками, app-only BIN, SD ZIP и проверочными отчётами — в [`dist/releases/0.7h/`](dist/releases/0.7h/) после успешного релизного gate. Обновлены 4 страницы Help (W/S и A/D в обе стороны), поиск с выбором и перелистыванием всех найденных Jazz Notes, ручной часовой пояс UTC со стабильным хранением в NVS, Jazz Spark: 150 редакционных фактов плюс 5 атрибутированных цитат. Ещё 1498 карточек музыкантов доступны на microSD, включая 63 новые редакционные биографии (остальные преимущественно короткие справочные описания, не развёрнутые биографии). Автоматический Spark показывается не чаще раза в 12 минут на 10 секунд и управляется T внутри Z. Никакие ссылки-источники на LCD не выводятся. **Новая версия не прошла аппаратную приёмку**: Wi-Fi, FIP/underruns, SD-задержки и память требуют реального Cardputer. Порог свободного APP в M5Launcher 3 PMan отдельно не подтверждён; **NOT CLEARED FOR INSTALL**. Не устанавливать merged/bootloader/partitions BIN и не переразбивать flash. Старые immutable-релизы не изменять.

**Отдельный Audio Router SDK:** самый поздний immutable **v0.8f-host-only** (22/22 MSVC Release, ESP32-S3 compile-only, 56 source SHA, MIT LICENSE), ZIP SHA-256 `015f1f330185159e9583f28466ffdfd549217051a51da735af4d42f3766affbf`. Есть `CardputerPcmMirror<256,3>`, bounded SD MP3 framing и Arduino File adapter, но **реальная SD-пауза, USB/BLE и дополнительный PCM consumer не включены в радио**. [Контракт безопасного подключения](docs/AUDIO-ROUTER-PCM-INTEGRATION.md). Единственный владелец ES8311/`M5.Speaker` остаётся прежним.

**Независимая проба SDK→Pocket Jazz (после v0.7d):** `tests/pcm_mirror_app_compile/pcm_mirror_app_compile.ino` собрана для ESP32-S3 (**PASS**, 503 075 Б программа / 34 908 Б статика), используя настоящий `AudioOutputM5Speaker` и SDK mirror через один composite monitor. Это **compile-only**, не активированное зеркалирование внутри радиопрошивки и не аппаратный тест.

**Подборка станций v0.7f (обновление данных, BIN не пересобирается):** **44 MP3 радиостанции (+32 новых)** в пяти тематических M3U и отдельном персональном шаблоне; 6 проверенных резервных URL. Вторая сетевая проверка **50/50 PASS**: HTTPS/CA/hostname, HTTP 200, 8192 байт и три согласованных MP3 кадра. [Методика, источники и ограничения](docs/STATION-STREAM-VERIFICATION.md), [JSON-результаты](docs/STATION-STREAM-CHECK-20261110.json). Обновляются плейлисты с SD, не двоичный код приложения; пользовательскую `04-my-stations.m3u` не перезаписывать. Исполнение на Cardputer не проверялось.

A very small internet radio, inspired visually by [Coders Cafe's minimalistic Wi-Fi radio](https://coderscafetech.com/blog/diy-wi-fi-internet-radio). The interface uses a midnight-blue background, amber highlights, a small radio tower, station number, volume bar and five curated jazz station groups (plus one editable personal template). **This is not a copied PCB or firmware from Coders Cafe.**

## What it does

- Plays live **MP3 internet-radio streams** via Wi-Fi on the Cardputer ADV **ES8311 internal speaker/headphone audio path**. Uses the same M5Unified/ESP8266Audio adapter approach as [RADIO DAYS U10](https://github.com/kariagepompadour/RADIODAYS), but with separately written, simpler UI and stream management.
- Station list selected from **editable plain-text M3U files** on SD; user-supplied stations of any genre work just like the jazz defaults. Built-in four-station jazz fallback when the SD card is unavailable.
- Dedicated decoding task; display and keypad handled separately. Reconnects if a stream ends, or when Wi-Fi becomes available again. Enter is a **stop/reopen of live radio**, not timeshift with a saved buffer. The emerging v0.2a source adds `#PJ-ALT:` secondary streams and local PLS reading.
- v0.2a source adds `SecureICYSource` with trusted CA roots, HTTPS certificate/hostname validation, NTP time synchronization and strict HTTPS-only redirects. There is no automatic HTTPS-to-HTTP downgrade. **TLS and audio playback on physical ADV are not yet verified.**
- Wi-Fi first-run selection and password entry directly on Cardputer (no credentials embedded in source). Preferences use the **`pocketjazz` NVS namespace** only; station index/volume writes are debounced to avoid flash wear.
- **No backend server, cloud account, phone, USB audio hardware or additional screen is needed**. Requires internet and a station whose stream remains online.

## Interface controls — историческая таблица до v0.7j, не карта v0.7o

**Важно:** таблица ниже описывает архивные сочетания ранних выпусков (включая уже удалённую B/Blue Note, Help на четыре страницы и W/S вместо Up/Down). **Не используйте её как инструкцию для v0.7o.** [Полная действующая карта](docs/CURRENT-FEATURES-20261110.md).

| Cardputer key | Action |
| --- | --- |
| **A / D** (or , / .) | Previous / next jazz station within current playlist |
| **P** | Next playlist group |
| **− / =** | Volume lower / higher |
| **Enter** | Stop and reopen the live radio stream (no paused buffer) |
| **W** | Scan and configure Wi-Fi; during setup A/D select, Enter confirms |
| **Tab** | Show/hide Wi-Fi password during entry |
| **Backspace** | Erase a password character, or exit Wi-Fi network list |
| **R** | Reload station playlists from SD without recompiling |
| **H** | Four help pages; W/S or A/D moves backward/forward, H/Enter/Del exits |
| **I** | Three-page Wi-Fi/audio/system diagnostics (v0.5b): A/D switches pages; I/Enter/Backspace exits |
| **1–9** | Recall saved preset |
| **Fn+1–9** | Save current station by URL/name; physical Fn untested |
| **L** | Recent stations: A/D select, Enter play, Backspace/L exit |
| **M** | Toggle mute |
| **T** | Sleep timer 30 → 45 → 60 minutes → off |
| **O** | Toggle/save autoplay |
| **S held during boot** | Safe start without autoplay on that boot |
| **Tab or /** (radio screen) | Search stations; Fn+A/D moves through matches, Enter plays, Tab exits, Backspace edits |
| **V** (new v0.2b source) | Toggle PCM-driven analog VU needle versus radio icon |
| **B** (new v0.2b source) | Blue Note / Night screen with UTC clock and ICY track when available |
| **G** (new v0.3a source) | Mood Tuner map: A/D adjust energy, W/S adjust acoustic level, Enter chooses nearest tagged station, G/Backspace exits |
| **K** (new v0.3a source) | Append current station, ICY track when present, and UTC/unknown time to SD `/PocketJazz/liked.txt` |
| **N** | Offline Jazz Notes from current `Artist - Track` ICY; W/S pages, N/Enter/Del back |
| **Q** (v0.3b source) | Show scannable QR for a bounded public HTTPS station URL; Q/Enter/Backspace returns; secret/private/long links are refused |
| **J** (v0.4a source) | Manual Tap Tempo: first J opens the screen and counts a beat; repeat J, C clears, Enter/Backspace returns |
| **C** | Jam Session Clock: A/D +/-30 min or W/S +/-60 min fixed UTC offset persisted to NVS; 0 resets to optional SD `clock.txt` POSIX DST rule; Enter/Backspace/C returns; no absolute time before NTP sync. In J/Tap screen C clears BPM instead |
| **S** (v0.5a source) | Network/Buffer meter: actual Wi-Fi RSSI, received MP3 buffer and approximate battery mV; Enter/Backspace/S returns. In G/Mood screen S moves vertically |
| **Y** (v0.5c source) | Session history of up to five observed ICY titles from the tuned station, without network polling or SD writes. Y/Enter/Backspace returns |
| **E** (v0.7a, context only) | When `I` diagnostics or `Y` history screen is open, manually append one bounded snapshot to `/PocketJazz/diagnostics.tsv` or up to 5 ICY titles to `/PocketJazz/icy-history.tsv`; not active in other screens, no background recording. See [docs/SD-EXPORTS.md](docs/SD-EXPORTS.md) |
| **F** (v0.7c source) | Open station browser without retuning the live stream. A/D moves, P browses a different playlist without playing it, Enter tunes explicitly, Del/F returns. Optional HTTPS-only ICY title sampling of visible *other* stations requires a user-created `/PocketJazz/peek.cfg` with exactly `PEEK=1`; **OFF by default**, not an accurate schedule and not hardware-tested. |
| **U** (v0.7h) | Search the 1498-record ASCII offline jazz directory (fallback 36 verified MusicBrainz cards); Enter searches, W/S or A/D navigates **all** results in pages of three, Enter opens, Del edits, Tab exits. No live search API. |
| **Z** (v0.7h) | Jazz Spark: one of 150 curated facts or five attributed quotations. W/S or Enter next; **T** on Spark toggles automatic ten-second cards every twelve minutes, persisted in NVS; Z/Del back. No source links on LCD. |

**Historical Wi-Fi/Help context (not current):** earlier versions used A/D and four Help pages. **v0.7o uses physical Up/Down, Help 17 pages, Del to leave.** See [current inventory](docs/CURRENT-FEATURES-20261110.md) and [KEYS](docs/KEYS.md). USB/BLE output, AAC/HLS, timeshift and on-device playlist editing remain unavailable.

The current source automatically dims the LCD after 60 seconds of keyboard inactivity and restores brightness on the next keypress without triggering its action. The battery percentage is an estimate, not a precision fuel gauge. **Mono note:** M5Unified 0.2.25 combines both input PCM channels in its Cardputer ADV mono-speaker path; no duplicate L/R downmix has been added. Physical left-only/right-only audio and power tests remain pending.

**v0.2b compiled source (hardware check pending):** the HTTPS/HTTP ICY metadata callback sends a bounded copy of `StreamTitle` from the decoder task to the UI through a one-slot queue, without cross-task display calls. The VU needle measures the **peak absolute value of either decoded stereo source channel** via the existing M5Unified monitor and updates only its own small rectangle. It is **not** a calibrated level at the mono speaker. Night mode draws a dark UTC clock, current station and real (not guessed) track metadata. v0.3b source repairs separate HTTPS/prefill deadlines, 100%→99% battery overpaint and VU anti-phase behavior; C++ host tests cover prefill/stereo cases, not the physical audio output.

**Mood Tuner v0.3a source (not yet hardware-tested):** M3U accepts `#PJ-MOOD:energy=4,acoustic=2` before a station URL. Both numbers must be exactly 0–4; untagged/invalid entries do not appear in the 5×5 Mood map. Horizontal position is **energy** (0 calm → 4 energetic); vertical position is **acoustic** (0 electronic → 4 acoustic). Enter selects the nearest tagged *station*, never an individual piece of music, and manual A/D/P tuning still works. Initial labels in shipped playlists are **subjective editorial approximations**, not metadata asserted by broadcasters. `K` writes a bounded text-only Like entry, not a recording.

**Pocket Jazz Notes and QR:** the SD ZIP bundles `/PocketJazz/artist-notes.tsv` and matching `artist-notes.idx` from the MusicBrainz generator. From v0.7b the curated dataset contains **36 officially API-verified artist+album pairs**, including the original Miles Davis, John Coltrane and Bill Evans. [Full provenance with MusicBrainz IDs](data/ARTIST-NOTES-PROVENANCE.md). N attempts lookup from an explicit `Artist - Track` ICY title. The index contains FNV-1a hashes, exact names and byte offsets; firmware verifies the TSV SHA-256 before index use and falls back to a bounded sequential scan. No artist inferred from a station name; no copyrighted chord chart supplied. Q shows a 90×90 QR using the bundled M5GFX encoder, ECC medium; URLs over 84 bytes or potentially private/secret-bearing are refused rather than truncated. Phone scanning on 240×135 hardware remains untested.

**v0.4a:** клавиша J вычисляет приблизительный BPM исключительно по ручным нажатиям, используя медиану до пяти интервалов. Автоматического распознавания темпа по PCM нет. Самостоятельный host-прототип будущего SD-кольца описан в [анализе time-shift](docs/SD-TIMESHIFT-FEASIBILITY.md); он **не подключён к прошивке**. Клавиши PAUSE / LIVE / −30 секунд пока не существуют, Enter по-прежнему останавливает и открывает эфир заново.

**v0.5a (source, hardware unverified):** B and C clocks use a shared optional `/PocketJazz/clock.txt` POSIX TZ rule; default `UTC0` and `TIME NOT SET` before NTP. S shows *actual Wi-Fi RSSI*, compressed MP3 buffer bytes, and estimated battery voltage (not radio-transmitter power or calibrated battery level). The ZIP ships only `clock.example.txt`: the user's `clock.txt` is not overwritten. See [clock setup](docs/CLOCK-SETUP.md). Previous host C++ tests had assertions compiled out in Release; this has been fixed in current source, and the C++ test suite is rerun with active checks before the next release.

**v0.5b (BIN/ZIP archived, hardware unverified):** I cycles Wi-Fi, audio, and system diagnostic pages. System values include minimum free heap, uptime, PCM frame count, reset reason, and approximate battery millivolts. UI updates the diagnostic content region only once per second. The release script rejects missing/failing C++ tests, blocks simultaneous publication to the same release folder, and verifies that its archived sketch source matches the source used by the compiler. A prebuilt `PackageOnly` BIN cannot be proven to match the current source; it requires an explicit unverified-provenance opt-in and is **NOT CLEARED FOR INSTALL**.

**v0.5c (BIN/ZIP archived, hardware unverified):** Y shows a bounded RAM-only history of ICY titles actually received while the current station was PLAYING. It skips consecutive identical messages, retains the broadcaster name and discards history on power loss. ICY messages are not accurate song-boundary timestamps; the radio does not poll every station in the background. NVS loading preserves legacy v0.1 values but refuses to write unknown newer schemas, and validates stored station URLs before attempting playback. v0.5c: **1 515 088 bytes**, SHA-256 `c730309591cd724618ede4d6035e707f1757ed94075e562e11c114b585d9c833`, Python 62/62, C++ Release 7/7.

**v0.6a (BIN/ZIP archived, physical validation pending):** optional self-supplied 64×64 RGB565 logos from `/PocketJazz/logos/<FNV64-of-URL>.pjlg`, with size/header/CRC checks and a single 8192-byte UI cache. The existing radio icon remains when a logo is absent/invalid; `V` shows the VU instead, `R` reloads SD resources. Convert a PNG on the PC using [docs/STATION-LOGOS.md](docs/STATION-LOGOS.md). Also rejects raw NVS URLs that would be truncated before validation, and prevents repeated full clearing of Night/Clock/Signal regions. The diagnostic distinguishes an NTP sync event from a merely plausible system date. v0.6a BIN: **1 517 008 bytes**, SHA-256 `a1323eb12b8fdcd272b05fe4e9b606cdc267742df4cbcef00f043c29609b05e1`, 70 Python and 8 C++ Release PASS, 73 source hashes verified. **The v0.6a ZIP contains Windows backslash separators in entry names**, which some cross-platform extractors may misinterpret. Use v0.6b or later ZIP when it is released; do not edit v0.6a retroactively.

**v0.6b (BIN/ZIP archived, physical validation pending):** ZIP entries use portable `/` separators; full Python 70/70, C++ Release 8/8, SHA for 73 source files and Arduino compile succeeded. v0.6b BIN: **1 517 008 bytes**, SHA-256 `59ac6d410d78df81ee7a78d5f1a6694fe306d079ed462f41fbbcc4745e768832`. Source-level NTP callback ordering also corrected.

**v0.7a (BIN/ZIP archived, physical validation pending):** `E` while on the Diagnostics (`I`) or ICY history (`Y`) screen explicitly requests a bounded 16 KiB app-owned SD TSV export; it does not create any files by merely opening the screens. Export excludes Wi-Fi passwords/SSIDs and stream URLs, and never asserts that ICY receipt marks song boundaries. Files are neither auto-deleted nor auto-rotated. BIN **1 519 600 bytes**, SHA-256 `44b6f23c385a10b9be8cc48c09377396fa194adbbe64ac0781c7b7d34255fa86`, 78 Python, 9 C++ Release, 78 source hashes verified. Details and acceptance checklist: [docs/SD-EXPORTS.md](docs/SD-EXPORTS.md).

**v0.7b (BIN/ZIP archived, physical validation pending):** bounded incremental `StreamTitle` parser for verified HTTPS avoids 4KiB dynamic metadata Strings, accepts only valid `icy-metaint`, normalizes only valid UTF-8 (without guessing legacy encodings) and applies a cumulative 2.5s metadata deadline; shared HTTP/HTTPS callback safely truncates UTF-8 to whole codepoints. User-triggered TSV append checks existing LF tail and verifies bytes after flush/reopen; Like text is sanitized too. [ICY details](docs/ICY-METADATA.md). Officially verified MusicBrainz notes expanded from 3 to 36 entries, original 3 preserved. **Arduino compile PASS, 85/85 Python, 10/10 C++ Release, 84/84 source SHA**; app-only BIN **1 520 720 bytes**, SHA-256 `fc4d74fa4f2284345651748d77609e17445bd757a972428caaea555e883172cd`. The plain HTTP upstream ICY source remains outside these HTTPS parser changes. Hardware stream/SD timing still unverified, and the Audio Router SDK is not integrated.

**v0.7c (immutable BIN/ZIP/source archived; physical gate pending):** F provides a station browser and an **opt-in** low-priority, one-connection HTTPS ICY title cache for at most four visible stations; 90-second positive TTL, 45-second negative TTL, 10-second spacing and 3.5-second/64-KiB **post-open reading** budget (not an overall DNS/TLS deadline). Scheduling is gated by Browse, Wi-Fi, audio buffer and underruns, free heap ≥120,000 bytes and largest free block ≥80,000 bytes. HTTPS requires verified CA/hostname and at most one safe HTTPS redirect. P1/P2 safety changes invalidate stale probe results on Browse navigation/playlist reload, avoid claiming unknown metadata as a song and keep off-channel titles out of listening history. Plain HTTP off-channel peek remains disabled because its upstream metadata reader can block; **ordinary HTTP playback is unchanged**. U provides bounded user-triggered SD search of the 36 MusicBrainz cards (max 50 supported by the format), with verified IDX/TSV fallback; [offline lookup contract](docs/JAZZ-NOTES-SEARCH-TECHNICAL.md). Host-only experiments include authenticated-envelope ESP-NOW state, Home Assistant MQTT/TLS and Audio Router SDK with MP3 time-shift state/framing; **none is enabled in the firmware**. App-only BIN: **1 532 720 bytes**, SHA-256 `b4d998d762c0737689d662d532e64930a26ed7b7bd0429492d2fee8eb7b9337a`; 99 Python, 16 C++ Release and Arduino PASS, but **NOT CLEARED FOR INSTALL**.

**v0.7d (immutable BIN/ZIP/source, no firmware runtime changes):** published and verified separately from v0.7c. Additional *host-only* ESP-NOW authenticated-envelope signer uses Windows CNG HMAC-SHA256 (known-answer host test) and ESP32 mbedTLS (compile-only); `BoundPeerExchange` binds key and callback lifetimes, with no provisioned secrets or radio driver. Separate Home Assistant TLS/MQTT Arduino probe also compiled; neither experiment is enabled in the radio. The ZIP includes root GPLv3 `LICENSE`, `COPYRIGHT-NOTICES.md`, upstream dependency notices and source provenance. **No device flash, SD write or physical acceptance.** The separately archived SDK **v0.8b-host-only** has 19/19 MSVC tests; its subsequent **v0.8c PJMF SD-ring candidate** had preliminary 20/20 host tests but was **not released or hardware-validated** when recorded. SDK code is not linked into Pocket Jazz. [v0.7d release gate](docs/V07D-RELEASE-GATE.md).

To deliberately enable optional HTTPS station peeks, manually create `/PocketJazz/peek.cfg` on the SD with exactly `PEEK=1` (one text line) and restart the radio. The distributed SD ZIP intentionally **does not** include this opt-in file. Remove it or write `PEEK=0` to disable. Do not enable before a controlled audio/heap stress test on the Cardputer; no zero-underrun guarantee is implied by source-level gates.

Open [preview/index.html](preview/index.html) on Windows to see an interactive simulated **240×135** screen (it does **not** play the streams). Real device LCD UI is drawn by `PocketJazzRadioApp.cpp`.

## Station lists (microSD)

Copy the files from `playlists/` onto the microSD into:

```text
/PocketJazz/01-essentials.m3u
/PocketJazz/02-modern-jazz.m3u
/PocketJazz/03-mellow-bossa.m3u
/PocketJazz/04-my-stations.m3u
```

An ordinary station entry:

```m3u
#EXTM3U
#EXTINF:-1,My Favorite Jazz Station
https://example.net/radio.mp3
```

You can name a new file `05-my-rock.m3u` and the application will include it **regardless of genre**. Empty files are skipped. Use UTF-8 without BOM if possible; ASCII labels are safest with the small LCD font. Current v0.2a source reads local M3U (`#EXTINF`, optional `#PJ-ALT:`) and local PLS (`FileN`/`TitleN`). **Remote playlists, HLS/M3U8 and AAC decoding are not supported.** Example explicit SomaFM backup:

```m3u
#EXTINF:-1,SomaFM Sonic Universe
#PJ-ALT:https://ice2.somafm.com/sonicuniverse-128-mp3
https://ice1.somafm.com/sonicuniverse-128-mp3
```

Maximum **12 playlists and 64 total streams**, with max **55-byte station names and 191-byte URLs**. For stable initial operation use one of the supplied live MP3 links. In case the SD file system cannot be mounted, four jazz stations are compiled into flash as a minimal fallback. Restart/reload stations with **R**. Users' M3U files are never written by the firmware.

### Source verification

The sample streams are listed by their broadcasters or established stream directories:

- [Radio Swiss Jazz official MP3 128k](https://www.radioswissjazz.ch/en/reception/internet)
- [Jazz24 official direct MP3 128k](https://www.jazz24.org/listening)
- [WBGO & WBGO Too official MP3 streams](https://www.wbgo.org/internal/mediaplayer/?type=stream)
- [FIP Jazz live MP3 listings](https://www.fluxradios.com/Radios-France/France-Radio-France/Fip-Webradios-France)
- [Jazz Radio official directories](https://www.fluxradios.com/Radios-France/France-Espace-Group/Jazz-Radio-France)
- [SomaFM Sonic Universe direct streams](https://somafm.com/sonicuniverse/directstreamlinks.html) and [Bossa Beyond direct streams](https://somafm.com/bossa/directstreamlinks.html)
- [WRTI All-Jazz official direct stream](https://www.wrti.org/listen-live-to-wrti)
- [RelaxingJazz official direct MP3](https://relaxingjazz.com/listen/)

The built-in player targets MP3 128–192 kbps. **Twelve supplied stream URLs were previously network-verified on Windows**, but this does not demonstrate TLS verification, audio decoding or stable operation on ADV. Streams and geographic availability can change. SomaFM playlists now declare explicit `#PJ-ALT:` alternatives; no automatic insecure downgrade is allowed. Streams are not redistributed.

Validate supplied files from Windows:

```powershell
python apps/pocket-jazz-radio/tests/test_playlists.py
python apps/pocket-jazz-radio/tools/probe_streams.py
```

## Build (Windows, compile only)

This project keeps Arduino CLI in `E:\Downloads\devspace-test\tools\arduino-cli` and installed toolchains under `...\toolchains\arduino-data`, **not in the project root**. Recommended tested versions for the speaker adapter are from RADIO DAYS U10:

- M5Stack board manager `m5stack:esp32@3.3.9`, board `M5Cardputer`, includes ADV runtime detection;
- `M5Cardputer 1.1.1`, `M5Unified 0.2.25`, `M5GFX 0.2.32`;
- `ESP8266Audio 2.4.1` (GPL-3.0-or-later), retained as a local copy within `arduino-sketchbook/libraries/`;
- `Flash Size=8MB`, `Partition Scheme=8M with spiffs`, `PSRAM=disabled`, `USB CDC On Boot=Enabled`, `USB Mode=Hardware CDC and JTAG`. No auto-upload.

Run from the Cardputer project folder:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File apps/pocket-jazz-radio/tools/build.ps1 -Version 0.2a
```

**Security/flash protection:** after a release passes its checks and **only after verifying PMan capacity**, select only the matching `dist/releases/<version>/firmware/PocketJazzRadio-v<version>-app-only.bin` in M5Launcher 3. Existing packages from v0.1.0 onward remain immutable. **Never select `.merged.bin`, `bootloader.bin` or `partitions.bin`**. Check real PMan free APP capacity and **Cancel** replacement/repartition prompts. No flash/upload/erase commands are invoked by the build script.

### Installation is a separate, voluntary step

1. Place the **app-only** firmware .bin under SD `/firmware/` (or wherever M5Launcher 3 lists SD apps), and the `/PocketJazz/` directory at the SD root.
2. Back up and verify the current SD contents; do not overwrite existing firmware files.
3. Compare real M5Launcher **free APP bytes** with the built .bin length; require margin and check board/partition compatibility.
4. Choose the new app from Launcher **only if no repartition is requested**. Cancel unexpected prompts.
5. Press W for Wi-Fi credentials; then use A/D, P, Enter, − and =.

**v0.2a has not been verified on the physical Cardputer ADV.** Earlier v0.1 hardware playback of one station does not verify current TLS, UI or new controls. M5Launcher 3 must remain intact.

## Code provenance

Original implementation: `PocketJazzRadioApp.cpp` under GPL-3.0-or-later. `AudioOutputM5Speaker.h` is copied from RADIO DAYS U10 which explicitly attributes it to the **MIT-licensed M5Unified WebRadio example (Copyright 2021 M5Stack)**. `ESP8266Audio` contains GPL-licensed decoding code; binaries including it must meet applicable GPL distribution requirements. The appearance is original and inspired by the minimal Coders Cafe layout, not a redistributed screenshot, graphic or exact theme.

