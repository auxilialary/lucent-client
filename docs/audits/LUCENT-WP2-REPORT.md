# Lucent WP2 — Map Download 0.7

Дата: 2026-10-23. Scope: только WP2 / N2 из LUCENT-CORE-AUDIT.md.

## Root cause

В CClient::ProcessServerPacket, NETMSG_MAP_DATA, после записи очередного
незавершающего chunk увеличивается m_MapdownloadChunk. Для 0.7 запрос ID 23
отправлялся на границе окна, однако между границами выполнялась общая ветвь
else: создавался запрос 0.6 с ID 19 и chunk index. RepackMsg оставляет этот ID
без перевода. Сервер 0.7 интерпретирует ID 19 как ENTERGAME.
При window=1 промежутков нет, поэтому этот размер окна не воспроизводит дефект.

## Исправление

Общая ветвь else заменена на else if(!IsSixup()). Теперь 0.7 отправляет
начальный запрос и запросы только после полностью полученных незавершающих
окон. После последнего chunk запрос не отправляется. Путь 0.6 продолжает
запрашивать каждый следующий chunk с ID 19 и прежним index.

HTTP fallback, размер окна, wire format, packer, запись и завершение карты
в production не изменены. Новых зависимостей нет.

## Изменённые файлы

- src/engine/client/client.cpp — одно условие ветвления.
- src/test/map_download_test.cpp — C++ regression harness и три теста.
- scripts/extract_map_download_test.py — извлечение исходного обработчика,
  SendMapRequest и RepackMsg для сборки тестов.
- CMakeLists.txt — включение теста и генерация include с зависимостью от client.cpp.
- scripts/integration_test.py — два локальных теста реального скачивания.
- docs/audits/LUCENT-WP2-REPORT.md — этот отчёт.

## Автоматизированные тесты

MapDownload.Window1, Window2, Window8 выполняют по 10 transfer scenarios:
0.6 и 0.7, по пять размеров карты (1 byte, окно минус 1 byte, ровно окно,
окно плюс 1 byte, два окна плюс 13 bytes). Размер chunk в harness — 31 byte.
Всего 30 сценариев, включая неполный последний chunk/window.

Тесты компилируют извлечённые production statements, а не копию условия.
Используются настоящие CMsgPacker, CPacker, CUnpacker, RepackMsg и файловый IO.
SendMsg перехватывается до UDP transport; сохраняются полные repacked messages.
Проверяются system bit, flags, connection, ID 23 без payload для 0.7,
отсутствие ID 19/ENTERGAME, ID 19 с последовательными chunk indices для 0.6,
число запросов после каждого входящего chunk, счётчики, завершение,
итоговые bytes и SHA-256 файла. Число запросов включает начальный запрос:
0.7 — ceil(chunks/window), 0.6 — chunks.

Контроль регрессии: harness собран с исходным обработчиком из HEAD d2f0867f0.
Window1 прошёл; Window2 и Window8 упали на лишних исходящих сообщениях.
После восстановления исправленного обработчика все три теста прошли.
Production-файл для этого эксперимента не откатывался.

map_download_06 и map_download_07 используют существующий integration runner,
отдельные save directories и серверную копию Tutorial под новым именем.
Карта отсутствует у клиента. Server window=8. Проверяются завершение скачивания,
вход в игру с ожидаемым протоколом, bytes и SHA-256 скачанного файла относительно
соответствующей серверной maps/maps7 версии. Локальный HTTPS endpoint
127.0.0.1:1 и connect timeout 50 ms позволяют проверить HTTP failure → UDP fallback
для 0.6 без внешнего map service. В 0.7 проверяется штатная UDP-загрузка.
В новых тестах нет sleep. Обычный клиент может делать фоновые запросы DDNet;
успех map transfer от их результата не зависит.

## Результаты проверки

- git fetch origin master: master и origin/master указывали на d2f0867f0.
  Работа начата от origin/master; по уточнению пользователя рабочая ветка
  переименована из fix/map-download-07 в dev/lucent-core без потери изменений.
- cmake -S . -B out/build/x64-Release -D CMAKE_BUILD_TYPE=Release: успешно.
- cmake --build out/build/x64-Release --target game-client testrunner -j 8:
  успешно, MSVC Hostx64/x64, Release. Это именно Release, а не исходный
  RelWithDebInfo в этом каталоге. DDNet.exe и testrunner.exe собраны.
- game-server также собран успешно для локальной проверки.
- testrunner.exe --gtest_filter=MapDownload.* --gtest_brief=1:
  3/3 passed, все 30 сценариев.
- Существующие suites Net, NetAddr, NetAddrDeathTest, Network, ChunkHeader,
  Packer, CVariableInt, Huffman, Snapshot, Uuid: 51 тест, 50 passed, 1 failed.
  Net.Ipv4AndIpv6Work: IPv6 receive не получил пакет; net_socket_read_wait
  вернул 0 после 10 s, net_udp_recv вернул -1 вместо 3 (net_test.cpp:48–49).
  Сбой повторился в Release; production network/base code не изменён.
  Полный network run не считается успешным.
- python scripts/integration_test.py out/build/x64-Release client_can_connect:
  client_can_connect_7 passed; client_can_connect (localhost) timeout;
  websocket test skipped, поддержка не включена.
- python scripts/integration_test.py out/build/x64-Release map_download:
  map_download_06 и map_download_07 passed, 2/2; реальные download/load/join,
  bytes/SHA-256 совпали. IPv4 literal использован для обоих протоколов.
- git diff --check: успешно.

## Ограничения и оставшиеся риски

C++ harness изолирует FinishMapDownload и transport, поэтому сам по себе
не доказывает загрузку валидной карты или полный handshake. Эти этапы отдельно
проверены локальными integration tests с реальными процессами.
Byte-level capture проверяет поток обработчика до UDP, не pcap полного handshake.
Integration transfer использует window=8; окна 1/2 и точные пограничные размеры
проверены в детерминированном harness, а не отдельными запусками сервера.

Не проверялись packet loss/reordering, внешние серверы, большой latency,
все upstream server versions, успешный HTTP download и повреждённый HTTP body.
Повторный UDP download после сетевой HTTP-ошибки проверен для 0.6;
отдельный случай HTTP body с неверным hash не проверен.
IPv6 и localhost failures остаются вне WP2 и не исправлялись.
Извлечение statements зависит от маркеров исходного файла; при рефакторинге
обработчика может потребоваться обновить extractor. Production API ради тестов
не расширялся. CORE AUDIT и master не изменены; push/merge/PR/tags не выполнялись.
