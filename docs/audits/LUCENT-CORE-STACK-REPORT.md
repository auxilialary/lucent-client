# Lucent Core — Stack Report

Дата: 23 октября 2026. Ветка: dev/lucent-core. База: 30f799698 (WP2).

## Изменения

| Задача | Коммит | Результат |
| --- | --- | --- |
| N1 | 2a7f460a3 | Повтор token request 0.7 каждые 500 мс в WANT_TOKEN; исходный timeout не продлевается, token и wire bytes сохраняются. |
| N3 | 4d4de2315 | Проверки open, короткой write, io_error и close; отказ до следующего запроса/завершения карты; существующий Disconnect очищает временный файл. |
| N4 | 7843cc193 | UDP 0.6 ограничен объявленным размером до записи; вычитание предотвращает переполнение; premature Last отклоняется. |
| B4 | 8f0f1a6c6 | Sort сбрасывает m_NeedResort во всех путях, включая HTTP refresh и смену списка. |
| B3 | 3951c1000 | Оба GET списка серверов, включая выбор master, ограничены 64 MiB. Бюджет соответствует 16K серверов по 4 KiB; это запас по объёму, а не ограничение числа записей. OnData ограничивает также декодированные данные без Content-Length. |
| N5 | ab5fccce9 | Connect ставит CHostLookup в существующий job pool; главный поток опрашивает готовность и применяет результаты в исходном порядке. Disconnect/новый Connect удаляет текущую коллекцию jobs. Старые jobs не имеют client pointer/callback; владение остаётся у pool до завершения Run. Результаты читаются только в STATE_DONE. |

Для N5 идентичность текущей коллекции jobs заменяет generation counter: старые jobs после отмены недоступны пути применения результата. Сохранены парсинг URL, выбор первого результата resolver для каждого адреса, default port, 0.7 flags и выбор пароля. Во время DNS используется существующее состояние CONNECTING; offline transport не прерывает ожидающую DNS попытку. Новый UI не добавлялся.

## Проверки

После каждой задачи выполнялись релевантная сборка и тесты:

- N1: Network + ChunkHeader, 13/13. IPv4 UDP regression отбрасывает первый запрос и следующий ответ, проверяет одинаковые bytes повторов, отсутствие немедленного повтора, wrong peer/token, переход в CONNECT и Disconnect. Использует короткие реальные интервалы 550 мс; полного серверного handshake и управляемого clock нет.
- N3: MapDownload, 5/5. Open failure, короткая write, io_error и close failure через существующий fixture WP2; проверены отсутствие Finish/следующего запроса и удаление временного файла. Ошибки write/close инжектируются, физическое заполнение диска не проводилось.
- N4: MapDownload, 6/6. Перерасход, premature Last, исчерпанный бюджет; существующие окна 1/2/8 обоих протоколов и проверка содержимого сохранены.
- B4: testrunner пересобран; ServerBrowser.PingCache 1/1. Все три вызова Sort проверены чтением кода. Отдельного измерения количества сортировок или benchmark нет.
- B3: ServerBrowser 2/2. OnData принимает ровно 64 MiB и отклоняет следующий байт без HTTP-сервера/Content-Length. Результат не дублируется в памяти теста; применяется существующий production callback. Проверка установки лимита на обоих GET — чтением кода.
- N5: client.cpp скомпилирован; Jobs 9/9. Semaphore-controlled задержка CHostLookup, отмена работающего resolver, удержание lifetime pool, независимое завершение replacement и освобождение после Shutdown. Это проверка механизма jobs, а не полный CClient/UI integration test.

Одна финальная полная сборка: cmake --build out/build/x64-Release -j 8 из MSVC x64 developer environment, exit 0. Конфигурация CMAKE_BUILD_TYPE=Release. Собраны DDNet.exe, DDNet-Server.exe и инструменты.

Один общий прогон testrunner: 389/389 passed, 61 suite, около 3.45 с; 3 DISABLED теста проекта. Исключён известный Net.Ipv4AndIpv6Work. Фильтр также содержал Net.HostLookup и Net.HostLookupWebsocket, но таких тестов нет; реальные Jobs.LookupHost/LookupHostWebsocket выполнены успешно. Логи: out/build/x64-Release/lucent-core-stack-tests.log и lucent-core-stack-tests.xml.

Git diff --check пройден. Новые зависимости не устанавливались. Gameplay, wire format и UI-код не менялись. CORE AUDIT оставлен вне коммитов. Push/merge/PR/rebase не выполнялись.

## Ограничения и оставшиеся проверки

- N5: системный getaddrinfo нельзя прервать через Abort. Отмена безопасно отбрасывает результат и не блокирует игровой поток, однако ShutdownJobs по-прежнему ждёт завершения работающего resolver. Ограниченное время shutdown не гарантировано; эта часть WP3 остаётся открытой.
- Не измерялись p99 обработки событий и отзывчивость реального окна при искусственной задержке DNS. Connect A → B/cancel/shutdown проверены чтением клиентского lifecycle и тестом jobs, без полноценного GUI integration test.
- Реальные подключения и полные загрузки карты с локальными 0.6/0.7 серверами не запускались. Token regression подтверждает продвижение до CONNECT, но не полный обмен до ONLINE и не отдельный deadline test.
- Известный IPv6/localhost сетевой тест не исправлялся и не перезапускался. Другие включённые NetAddr/Jobs тесты прошли.
- Лимит HTTP — явный ресурсный бюджет, а не результат замера актуального production master. JSON DOM может занимать больше памяти, чем исходные 64 MiB.
