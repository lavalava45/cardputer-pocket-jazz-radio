# Curated Jazz 56 — обновление каталога от 11 ноября 2026

**Статус:** 56 основных уникальных ссылок HTTPS/MP3 в пяти тематических M3U, шестой `04-my-stations.m3u` — пустой шаблон пользователя. Ограничение загрузчика в прошивке — **64 станции, 12 списков**; остаётся **8 свободных слотов**, если не добавлены другие личные или диагностические файлы на SD. Это **проверка данных на ПК, не физический hardware gate ESP32-S3**.

Убраны две **целиком блюзовые** станции `181.fm True Blues` и `Jazz Radio Blues`: пользователь сообщил, что жёсткий блюз стилистически не подходит основному джазовому каталогу. Отдельные джазовые каналы с элементами soul/roots продолжают быть доступны; пользователь может добавить любые жанры в свой M3U.

## 14 новых станций

| Список | Станция | Первичный HTTPS/MP3-адрес | Проверка с ПК |
| --- | --- | --- | --- |
| Essentials | JAZZ.FM91 | `https://jazzfm91.streamb.live/SB00009` | 200, MP3, CA OK |
| Essentials | JazzRadio Berlin | `https://streaming.radio.co/s774887f7b/listen` | 200, MP3, CA OK |
| Essentials | KNKX 88.5 | `https://knkx-live-a.edge.audiocdn.com/6284_128k` | 200, MP3, CA OK |
| Modern | JAZZ.FM91 Fusion | `https://jazzfm91.streamb.live/SB00466` | 200, MP3, CA OK |
| Modern | JAZZ.FM91 Notes From Canada | `https://jazzfm91.streamb.live/SB00137` | 200, MP3, CA OK |
| Modern | Musica Jazz Radio | `https://ice16.fluidstream.net/mjr.mp3` | 200, MP3, CA OK |
| Mellow | SmoothJazz Monterey | `https://smoothjazz.cdnstream1.com/2585_128.mp3` | 200, MP3, CA OK |
| Mellow | SmoothLounge Monterey | `https://smoothjazz.cdnstream1.com/2586_128.mp3` | 200, MP3, CA OK |
| Mellow | Sublime Jazz | `https://stream.sublime.nl/web21_mp3` | 200, MP3, 1 HTTPS redirect, CA OK на обеих точках |
| Swing/Vocals | JAZZ.FM91 Oscar Peterson | `https://jazzfm91.streamb.live/SB00023` | 200, MP3, CA OK |
| Swing/Vocals | JAZZ.FM91 RESPECT Vocal | `https://jazzfm91.streamb.live/SB00024` | 200, MP3, CA OK |
| Swing/Vocals | Illinois Street Lounge | `https://ice5.somafm.com/illstreet-128-mp3` | 200, MP3, CA OK |
| Latin/Soul | Sublime Soul Classics | `https://stream.sublime.nl/web22_mp3` | 200, MP3, 1 HTTPS redirect, CA OK на обеих точках |
| Latin/Soul | WWOZ New Orleans | `https://wwoz-sc.streamguys1.com/wwoz-hi.mp3` | 200, MP3, CA OK |

Метод: ПК на Windows, реальные `GET` с первым блоком MP3 (не только HEAD или название HTML), `Content-Type: audio/mpeg`; TLS 1.2 и hostname цепочка проверены через OpenSSL `s_client -CAfile` с **содержимым текущего `PocketJazzRadio/RadioTrustedRoots.h`**, для перенаправленных потоков — на первом и конечном хосте. Первые байты содержали корректный синхрозаголовок MPEG (`0xFFE.../0xFFF...`). Внешние проекты-первоисточники: `https://jazz.fm/streaming/`, `https://www.knkx.org/stream-knkx-online`, `https://www.jazz24.org/streaming-links/`. Кандидаты с TLS error, AAC, нестандартной рамкой вместо MP3 или непроверенным исходным CA исключены. Результат **не доказывает** отсутствие dropout на ADV, регулярную стабильность потоков или совместимость каждого редиректа с ESP32.

## SD миграция без потери пользовательских файлов

Шестой пакет теперь называется `06-latin-soul-jazz.m3u` вместо старого `06-latin-soul-blues.m3u`. Если на пользовательской SD остался **старый** `06-latin-soul-blues.m3u`, загрузчик продолжит видеть его отдельным списком со старыми блюзовыми станциями — простое копирование нового пакета **не удалит** его. После резервной копии владелец может **вручную** удалить только старый M3U. Не удалять целиком `/PocketJazz`, `04-my-stations.m3u`, `settings.bak`, `liked.txt`, `clock.txt`, `logos` или пользовательские офлайн-MP3; архивный пакет содержит пустой шаблон `04-my-stations.m3u`, поэтому **не перезаписывать личную версию** шаблоном.

При загрузке нескольких дополнительных пользовательских/диагностических списков реальный резерв меньше 8 слотов. `R` перечитывает M3U/PLS, `P` переключает списки. Следующий hardware gate: не просто наличие имён в браузере, а реальная HTTPS/MP3 доставка, стабильные `Buf/Under/PCM gap`, управление STOP/сменой и отсутствие рывков UI.

**Исторический** `docs/STATION-STREAM-VERIFICATION.md` относится к исходным 44 станциям и **не подтверждает** новые 56; исторический отчёт оставлен без ретуширования.
