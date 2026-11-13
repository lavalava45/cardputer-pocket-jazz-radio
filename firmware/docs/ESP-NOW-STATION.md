# ESP-NOW Station Exchange — проект безопасной передачи станции

**Статус на 10 ноября 2026:** immutable **v0.7d опубликована и прошла software gate**, но её app-only BIN **побайтово совпадает с v0.7c**, поэтому ESP-NOW radio-driver не появился. Host-only эксперименты: `experiments/StationExchangePacket.h` (PJST v1, CRC32), `StationExchangeSecure.h` (PJS2: authenticator, anti-replay, bounded Offer/ACK), `StationExchangeHmac.h` (HMAC-SHA256, Windows CNG/ESP32 mbedTLS) и `BoundPeerExchange` (совместное владение ключом и состоянием). Нет реального provisioning, доверенного pairing UX, приёма/отправки по радио, подтверждения URL в приложении либо автосмены станции. **NOT CLEARED FOR INSTALL** до проверки PMan. [Gate v0.7d](V07D-RELEASE-GATE.md).

## Формат пакета v1

`pocketjazz::exchange::Packet` содержит `Type` (Offer=1 или Acknowledge=2), `uint32_t nonce`, ASCII `name[33]` и `url[192]`. `encode` и `decode` работают с буфером вызывающего кода, без выделения памяти; байтовый пакет максимум **250 байт** (консервативный предел совместимости с legacy ESP-NOW), заголовок 12 байт, CRC32/IEEE 4 байта.

| Смещение | Поле |
| --- | --- |
| 0–3 | ASCII `PJST` |
| 4 | версия 1 |
| 5 | тип: Offer=1 / Acknowledge=2 |
| 6–9 | nonce, little-endian |
| 10 | длина name |
| 11 | длина URL |
| 12… | name и URL без NUL в wire format |
| последние 4 байта | CRC32/IEEE пакета перед CRC, little-endian |

Offer требует name длиной 1–32 и URL длиной 1–191; Acknowledge не содержит ни того ни другого, только nonce. Декодер проверяет сигнатуру, версию, тип, точную длину, CRC, управляющие символы и допустимость URL, после чего копирует данные в NUL-terminated поля. Нельзя считать CRC механизмом аутентификации или защитой от намеренной подмены.

## Защитные условия и оставшиеся риски

Код принимает только ASCII printable name и URL со схемой `http://` либо `https://`; блокирует `?`, `#`, `@`, обратную косую черту, пробелы, очевидные literal LAN/loopback адреса и пустой/безточечный host. Таким образом, URL с парольными токенами в query или userinfo не должны передаваться. Но **это лишь синтаксический фильтр**, не проверка DNS или общедоступности адреса: возможны другие private/ULA IPv6 адреса, DNS rebind, перенаправления и домены локальной сети. Некоторые формы IPv4 также нельзя считать полностью отфильтрованными.

Ни nonce, ни Acknowledge, ни CRC **не доказывают личность отправителя**. В будущей реализации обязательно: явное согласие на отправку/приём, pairing или allowlist MAC, ограничение по времени и частоте запросов, защита от replay (nonce + окно времени/одноразовая таблица), контроль Wi-Fi канала и сосуществования со streaming Wi-Fi. Приём не должен автоматически открывать URL или играть звук. Перед вставкой в плейлист нужно показать имя/адрес и получить отдельное подтверждение; затем применить обычную политику stream URL, TLS/redirect и записи M3U. Не разглашать приватные URL и credentials в широковещательных кадрах.

## Экспериментальный PJS2 — что реализовано и что отсутствует

`StationExchangeSecure.h` помещает полный PJST v1 внутрь аутентифицируемого кадра `PJS2`: magic 4 Б, pairwise session epoch 8 Б, sender/receiver unicast MAC IDs по 6 Б, длина 2 Б, исходный PJST (до 208 Б), внешний tag 16 Б. Суммарный предел **250 Б** сохраняется: накладные расходы 42 Б; максимально допустимые имя и URL нельзя одновременно отправить в защищённом кадре. Аутентификация охватывает идентичности, session, длину и весь вложенный пакет; после проверки full frame дополнительно проверяется синтаксис PJST.

`Authenticator` принимает внешние callback подписания и проверки. Исходный host-only тест автомата использует **специально некриптографический fake-MAC**; его PASS доказывает контракт переходов, но не криптографическую подлинность. Для следующего исходного этапа **реализован настоящий `StationExchangeHmac.h`**: HMAC-SHA256 с 256-битным ключом и 128-битным усечённым тегом, Windows CNG host known-answer test и ESP32 mbedTLS compile-only. Дайджест сравнивается без раннего выхода по байтам, ключ стирается при reset/destruction. **Никаких реальных ключей, паролей и токенов в прошивке нет.** Ключ и аутентифицированный peer должны быть переданы внешним доверенным provisioning-механизмом; без него данный код ещё не обеспечивает действительный pairing.

После независимого ревью устранён риск raw-callback use-after-free: `PeerHmacSha256::authenticator()` приватен, а `BoundPeerExchange` не копируется/не перемещается и хранит MAC-провайдер и `StationExchangeSecure` в одном объекте с общим сроком жизни. Приложение должно использовать именно bound-wrapper и вызывать его **только из одной управляющей задачи**, не из RF callback/ISR. Это всё ещё **не драйвер ESP-NOW, не UX pairing и не подтверждённая безопасность устройства**.

Автомат держит одного peer, одну исходящую незавершённую Offer и один входящий ожидающий решения запрос. Pairing и принятие конкретной станции требуют **отдельного согласия человека**; pairing-предложение и входящий Offer имеют окна по 30 с. Исходящий nonce монотонен; session epoch должен быть новым, непредсказуемым и выданным через внешний доверенный канал после reboot/re-pair. Идентичные принятые Offer не показываются вторично; отказанные/старые или чужие отклоняются. Исходящая Offer — одна отправка плюс не более трёх повторов с паузой ≥700 мс, общий deadline 5 с; принятый Offer допускает не более четырёх ACK. Это **не** реализует собственно процедуру обмена ключами, доверенный pairing, тайминг ESP-NOW или защиту от RF flood.

Существующий `tests/station_exchange_secure_test.cpp` проверяет повреждения каждого байта, неправильные session/key/peer, длины, URL/имена, отсутствие подтверждения, replay, deadline, retry/ACK и reset со **слабым детерминированным fake-MAC**, не предназначенным для устройства. Подробный host-only test contract: [ESP-NOW-SECURITY-TESTS.md](../tests/ESP-NOW-SECURITY-TESTS.md).

## Проверка и этапы интеграции

`tests/station_exchange_packet_test.cpp` проверяет Offer/ACK, CRC, длины и URL; `station_exchange_secure_test.cpp` — host-only PJS2 с **заведомо некриптографическим fake-MAC**; `station_exchange_hmac_test.cpp` — настоящий Windows CNG HMAC-SHA256 с независимым known-answer vector, подменой тега, sender mismatch, round-trip и lifetime bound. **Опубликованный v0.7d gate: MSVC Release 17/17 PASS, отдельный ESP32 mbedTLS HMAC compile-only PASS (288 115 Б sketch, 21 676 Б globals)**. Независимая HA TLS compile-only сборка — 291 091 Б / 23 596 Б. Исторический v0.7c остаётся 16/16. Физический RF, HMAC на самом ESP32 с provisioned ключом и pairing **не проверялись**; runtime firmware v0.7d не менялась.

Дальнейший порядок: (1) **аутентифицированный** ключевой provisioning и pairing/fingerprint UX без секретов в исходниках (host KAT уже есть), (2) защита от RF flood *до* дорогостоящих HMAC verify, (3) радио/ESP-NOW adapter с каналом Wi-Fi и сериализацией доступа к callback, (4) отдельное подтверждение **каждого** предложенного URL и его DNS/redirect проверка без auto-play, (5) тест двух Cardputer с действующим HTTPS MP3 и измерением latency/heap/underrun. Нельзя объявлять Station Exchange работающим по host-протоколу и compile-only.
