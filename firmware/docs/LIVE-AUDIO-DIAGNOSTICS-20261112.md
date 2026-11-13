# Live diagnostics PLAYER — рабочие исходники 12.11.2026

Изменения после immutable v0.7z2. **Диагностическая v0.7z3 отдельно опубликована с полным software gate PASS (226 Python, 45 MSVC CTest, ESP32-S3 app + 3 compile-only).** Ни физическая установка, ни причина зависаний не подтверждены. [Отчёт проверки](RELEASE-V07Z3-VERIFICATION.md). frozen source документации в архиве отражает её состояние на момент упаковки; дальнейшие документальные обновления не переписывают immutable релиз.

## Что реализовано

`AudioRuntimeDiagnostics.h` содержит bounded probe, независимый UI observer и RAM-журнал на 16 событий. Аудиозадача публикует согласованный snapshot в короткой секции portMUX; observer в главном UI loop опрашивает их не чаще 100 мс. Пока аудиозадача заблокирована, UI вычисляет возраст стадии и heartbeat самостоятельно. Периодические записи — максимум раз в секунду; новая ошибка, изменение Opens/Under/reject/STOP/Wi-Fi drops/audio state вызывают внеочередную запись при ближайшем опросе. Это наблюдатель в существующей UI task, не дополнительная FreeRTOS task или сетевой клиент. Сам UI может задерживаться при своих операциях; это отражается временем наблюдения и UI gap.

Стадии и enum ID: IDLE=0, DNS=1, TCP/TLS=2, HTTP=3, PREFILL=4, DECODE=5, PCM FLUSH=6, RETRY=7, STOP=8, NET READ=9, META=10, OPEN=11. OPEN означает локальную подготовку и создание HTTP request; `http.begin()` не считается сетевым подключением. DNS публикуется непосредственно перед единственным `hostByName`, TCP/TLS непосредственно перед существующим host-validated SDK connect. После успешного connect и до возврата GET наблюдается HTTP — **GET + response headers вместе**. Separate TCP/TLS и отдельное чтение заголовков внутри SDK не заявлены. Каждый redirect hop публикует свой номер; attempt — монотонный raw Opens, не сбрасываемый X. RETRY имеет явный абсолютный `retryAt` и начало backoff; его можно отличить от одного заблокированного сетевого вызова.

Подключение probe **per-instance, opt-in**: `RadioTracedTlsClient::runtime` изначально nullptr. Только два HTTPS пути audioWorker (прямой и upgrade) вызывают `setRuntimeProbe(&audioRuntime)`. `stationPeekWorker` создаёт отдельный SecureICYSource, устанавливает только peek limits и не подключает PLAYER probe. Его параллельные DNS/TLS/GET не меняют stage PLAYER. Используемая обоими задачами память остаётся общей: исключение PEEK из фаз не доказывает, что его TLS никогда не влияет на ресурсный запас PLAYER. Plain HTTP имеет внешнюю OPEN-стадию; раздельных DNS/TCP hooks для upstream HTTPClient здесь нет.

Ошибки HTTPS фиксируются в `reportFailure` до close/reset/следующего hop, даже если уже поступила команда STOP. HTTP и TLS — signed int32. Сохраняются timestamp, stage/start, attempt/hop, heap/largest непосредственно после отказа и значения перед SDK TCP/TLS. До TCP/TLS значение before=0 означает «не измерено», не исчерпание памяти. Отказы prefill/codec и decoder также фиксируются в worker. Успешное открытие не является ошибкой и не увеличивает журнал ошибок.

## Безопасность обмена между задачами

Один писатель probe — audioWorker и его синхронные вызовы. PEEK, speaker release callback и ISR в него не пишут. На целевом ESP32-S3 сборка Arduino-ESP32 3.3.9 включает -mdisable-hardware-atomics, поэтому прежние требования lock-free атомарности не выполняются. Используем portMUX_TYPE: только копирование фиксированных структур и счетчиков под коротким lock; никаких millis/heap/Serial/сети/SD внутри секции. В host-тестах аналогичная защита обеспечивается std::mutex. UI кратко ожидает lock, но не сетевой open. stage/start/attempt/hop/retry deadline/heartbeat согласованы; Scope не восстанавливает устаревший outstanding.

Передача ошибок — кольцо из 16 слотов, защищенное тем же portMUX, с публикацией serial при завершении записи. UI копирует записи под тем же lock и проверяет поколение; data race на обычных полях отсутствует. При переполнении/пропусках растет coalesced. Обычный AudioRuntimeJournal::events_ используется только UI task для записи/экспорта.

Размеры ограничены static_assert: probe <=1024 Б, journal <=2048 Б. Отдельная host-проверка после перехода на portMUX: probe 952 Б, journal 1872 Б (MSVC x64). Arduino-компилятор показал **114 044 Б** статической RAM против **111 212 Б** для v0.7z2 (разница +2832 Б суммарно по прошивке); это не измерение реального heap во время TLS. При низком ресурсном запасе учитывать накладные расходы на Cardputer. В аудиозадаче нет новых String, Serial, SD writes, malloc или второго сокета.

## PCM и журнал

В flush наблюдаются playRaw (detail=1), ожидание следующего буфера (2), stop-drain (3), их enter/exit/age и outstanding count. Занятые буферы считаются существующим audio owner чтением atomic busy flags. Release callback сохраняет прежнее владение и только освобождает действительно возвращённый буфер. **Никакого принудительного busy=false по таймауту, переиспользования занятой памяти, новых retries или recovery не добавлено.** Реально бесконечное ожидание остаётся бесконечным, но становится наблюдаемым.

Записи включают buffer, raw Opens/frames/Under/drops, rejected PCM, возраст последнего успешного PCM enqueue, live-input waiting, последнюю завершённую STOP latency, Wi-Fi/RSSI, audio state, текущую память. `pcm_age=UINT32_MAX` — PCM ещё не наблюдался с запуска. Эти данные позволяют сопоставлять пустой ring, 4-с live-input watchdog, повторный open и PCM waits/reject; сами по себе они не являются акустическим измерением и не устанавливают причинность. Prefill 2048, watchdog 4000, ring 32768 и приоритеты сохранены.

Для delayed failure timestamp/stage/attempt/hop/codes/heap before/after взяты непосредственно из error latch; остальные counters/Wi-Fi/buffer/PCM относятся ко времени UI-наблюдения `observed`, а не притворяются снимком точного момента отказа. UI может не увидеть краткую стадию между опросами; длительный DNS/TLS/GET/PCM wait остаётся виден. Журнал последних событий может вытеснить старую ошибку последующими наблюдениями — это ограниченный history, не постоянный аудит.

SYSTEM HEALTH остаётся восьмой страницей Diagnostics: live stage/age/detail, heartbeat age, attempt/hop. Полные поля retry deadline/outstanding доступны в журнале. **E на странице 8** вручную дописывает retained rows в `/PocketJazz/audio-runtime.tsv` через существующий bounded append (16 КиБ/файл, без удаления/ротации/перезаписи). E на остальных страницах сохраняет прежний diagnostics.tsv. Повторный E добавит повторный snapshot журнала. X сбрасывает прежние аудиосчётчики, но не стирает runtime history, failure serial и исторический minimum heap. Автоматической SD-записи нет.

## KJazz отдельно

Подготовлен переносимый **неприменённый** `KJazz-GTS-R4-candidate.diff`, добавляющий один самоподписанный GTS Root R4 к четырём существующим корням; git apply --check и применение к отдельной копии PASS. +764 байта PEM в flash; DER=525 байт. SHA256 DER `349dfa4058c5e263123b398ae795573c4e1313c83fe68f93556cd5e8031b3c7d`. В предшествующей строгой PC-проверке исходный bundle дал Verify 20, временный +R4 дал Verify 0 с hostname validation. **Рабочий RadioTrustedRoots.h не менялся.**

525 байт DER не равны heap delta mbedTLS: дополнительно нужны x509 context, ключи и накладные расходы allocator. Честная численная оценка RAM сейчас ограничена этой структурной разницей (4→5 разобранных CA), а фактический прирост peak/heap остаётся НЕ ИЗМЕРЕН. Измерять A/B в одинаковом состоянии на том же ESP32/SDK после разрешения сборки: before connect, минимальный запас во время handshake и after cleanup. Исторический глобальный minimum нельзя приписывать одной попытке или одному корню. CA-патч не объединён с диагностикой.

## Проверки и ограничения

После переключения синхронизации: 2 новых MSVC Release CTest PASS (согласованные snapshots/concurrent reader, ring overwrite, fault latch, PCM reject и задержанный release). Это host-сборки, не физическое устройство.

После Gemini доработаны attempt/backoff/hop/outstanding и добавлены C++ сценарии blocked PLAYER + unattached PEEK, возраст стадии, retry deadline, сохранение ошибки предыдущего attempt и отсутствие восстановления старого outstanding. **После перехода на portMUX оба новых MSVC Release CTest 2/2 PASS, весь CTest 45/45 PASS, Python 226/226 PASS, основной ESP32-S3 и три compile-only PASS.** ThreadSanitizer и аппаратный запуск не выполнялись. Fake transport упражняет API probe; полноценный SecureICYSource/SDK GET не эмулируется.

На финальных исходниках **18 Python source-contract checks PASS**: 5 runtime wiring, 8 Diagnostics/X-reset, 4 HTTPS trace, 1 handshake setter. Проверены per-instance PEEK opt-out, порядок DNS/TCP/HTTP, запись ошибки до отмены, сохранение настроек и отсутствие второго writer в speaker callback. Структурные checks не заменяют компиляцию приложения, TSan или физическую приёмку. Root cause периодического CONNECTING ещё не объявлен найденным.
