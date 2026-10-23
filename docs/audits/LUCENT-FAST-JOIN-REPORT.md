# Lucent — Fast Join Prototype: измерения и вердикт

Дата: 23 октября 2026. Ветка: dev/lucent-core. Исследованный production HEAD: 2f3473107bcd1ecc0280b03f6a3daaa35541da9c. Исходный аудит использован как входные данные; повторный полный аудит не выполнялся.

**Вердикт: этот прототип не оставлять в production.** Ускорение warm/reconnect измерено, но мягкий бюджет 2 мс не ограничивает отдельный decode/upload: main thread задерживает обслуживание сети до 32,51 мс. Map switch между разными картами не ускорился. Поэтому экспериментальные изменения удалены из src, сохранены воспроизводимый patch, стенд, сырые результаты и ограничения проверки. Обычный Release x64 клиент пересобран после удаления probes и прототипа.

## Архитектура проверенного прототипа

Critical stage сохраняет штатную проверку карты, CLayers, collision, teleports, switches, physics, настройки карты, gameplay-компоненты, sounds, все tile layers и используемые ими изображения. Все tile layers оставлены обязательными консервативно: формат карты не даёт общего доказательства, что произвольная background/foreground tile geometry безопасно отсутствует в игровом кадре. Прототип разделяет quads в обоих render passes и изображения, используемые только quads; он не реализует полное отложенное построение всех визуальных tile layers.

CMapImages ставит quad-only images в очередь. CMapRenderer сохраняет исходный порядок групп/слоёв и откладывает OnInit/Init quads; pending layers пропускаются renderer. Один общий deadline 2 мс в CGameClient::OnRender обслуживает сначала images, затем background quads, затем foreground quads, в том числе в LOADING во время snapshot bootstrap. Неподготовленные texture handles явно invalidated; невалидные quads остаются исключёнными из отрисовки. Все resource/GPU command API вызываются на main thread через существующий graphics command buffer, workers не добавлены.

OnMapUnload отменяет callbacks/очереди перед каждым engine Map::Unload, включая повторную загрузку в том же состоянии LOADING. State changes тоже отменяют main-map работу. Mutable gameplay world и snapshots не кэшируются; штатная очистка snapshots не меняется. Нет wire-format, snapshot ACK, tick или physics изменений. Демо загружаются штатно. Custom/menu background не переводится на progressive path; фон текущей карты синхронно дожидается своих images.

STARTINFO не отправлялся из более раннего engine state: он остаётся после critical OnConnected/component reset/map settings. Его продвижение во времени получается за счёт исключения deferred visuals из critical stage. ENTERGAME продолжает отправляться штатным обработчиком READYTOENTER, после обязательной готовности; две реальные snapshots до ONLINE сохранены. Серверная синхронизация перекрывается с декоративной подготовкой, но синхронный ресурсный шаг всё ещё откладывает следующий PumpNetwork.

Флаг cl_staged_map_load (default 0, не сохраняется) выбирал эксперимент только для новых карт; 0 проверял штатный путь, 1 — staged path. Кэш подготовленных ресурсов не добавлен: reconnect заново готовит ресурсы; вводить hash/device-generation cache поверх уже отвергнутого pipeline неоправданно.

## Стенд и определения

Windows 10.0.26100.9711, Ryzen 7 7700X, Radeon RX 7600, driver 32.0.31041.1004; MSVC 14.51.36231, Release x64. OpenGL 3.3 подтверждён stdout каждого клиентского процесса. Loopback 127.0.0.1; DDNet server 0.6 и DDNet sixup/0.7; sv_register 0, dummy off, VSync off, gfx_refresh_rate 144, cl_refresh_rate и inactive 1000. Это не сертификация всех исторических/native 0.7 серверов.

Итоговая выборка: 2 variants × 2 protocols × 4 cases × 10 repeats = 160 переходов. До каждого подключения и команды смены карты добавлена uniform(0, 200 мс) задержка, seed 20261023. Финальные серии выполнялись последовательно, без параллельной сборки. Pilot-результаты исключены: там map switch использовал копии Tutorial, а часть baseline пересеклась со сборкой.

Cold — нет map file в изолированном save directory, но page cache Windows не сбрасывался. Недоступный HTTPS endpoint 127.0.0.1:1/timeout 50 мс затем UDP fallback сохранён из существующего стенда. Это не успешный HTTPS и не чистый UDP-only benchmark. Warm — новый клиентский процесс с уже скачанной картой; reconnect — тот же процесс после disconnect. Реальный map switch: Tutorial (join-a) → Sunny Side Up (join-b), разные hashes, geometry и ресурсы; обе representations/hashes сохранены в fast-join-metadata.json.

T0 — вход в CClient::Connect. Для map switch T0 — обработка MAP_CHANGE клиентом, без серверной подготовки до отправки пакета. Double-click → Connect **не измерено**: этот command-driven стенд не воспроизводит UI double-click; это не нулевая задержка. ONLINE ниже — timestamp второй проверенной reconstructed snapshot, непосредственно перед штатным переходом ONLINE; небольшая работа между probe и SetState не выделена.

Playable submission — local character + ONLINE + неактивные menus/chat, следующий возврат Graphics::Swap. Это proxy доступного input path и CPU command submission, а не гарантия unfrozen character на каждой карте, серверного применения самого первого input или показа кадра монитором. Full submission дополнительно требует пустых image/background/foreground очередей. GPU completion, compositor present и monitor scanout не измерены. Полная сцена в этих 160 переходах успевала готовиться к первому playable proxy; разница timestamps двух submission probes — только последовательные вызовы логирования, менее 0,01 мс.

Все таблицы: миллисекунды, n=10 в каждой ячейке, p50=median, p95=nearest-rank, поэтому при n=10 p95 совпадает с max. Это малая локальная выборка; небольшие различия map switch не следует интерпретировать как статистически доказанный эффект. Таймер time_get_nanoseconds/steady_clock, сравнение только внутри одного процесса/epoch. Временные stdout probes имеют неизмеренный overhead; blank-probe A/B не выполнялся.

## Connect → ONLINE proxy

| Протокол / сценарий | Baseline p50 / p95 / max, мс | Prototype p50 / p95 / max, мс |
|---|---:|---:|
| 0.6/cold_fallback_udp | 631.22 / 760.58 / 760.58 | 601.31 / 636.63 / 636.63 |
| 0.6/reconnect | 570.54 / 670.13 / 670.13 | 458.83 / 532.65 / 532.65 |
| 0.6/warm_new_process | 593.71 / 645.80 / 645.80 | 423.23 / 583.62 / 583.62 |
| 0.6/map_switch_cold | 982.99 / 985.83 / 985.83 | 983.97 / 986.59 / 986.59 |
| 0.7/cold_fallback_udp | 932.19 / 1025.58 / 1025.58 | 873.84 / 952.44 / 952.44 |
| 0.7/reconnect | 560.82 / 635.02 / 635.02 | 437.27 / 551.11 / 551.11 |
| 0.7/warm_new_process | 547.55 / 656.05 / 656.05 | 458.59 / 560.63 / 560.63 |
| 0.7/map_switch_cold | 1184.88 / 1383.10 / 1383.10 | 1184.32 / 1185.81 / 1185.81 |

## Connect → first playable CPU frame submission

| Протокол / сценарий | Baseline p50 / p95 / max, мс | Prototype p50 / p95 / max, мс |
|---|---:|---:|
| 0.6/cold_fallback_udp | 637.55 / 769.74 / 769.74 | 609.60 / 642.90 / 642.90 |
| 0.6/reconnect | 572.06 / 672.11 / 672.11 | 463.25 / 537.76 / 537.76 |
| 0.6/warm_new_process | 603.04 / 652.59 / 652.59 | 431.08 / 593.34 / 593.34 |
| 0.6/map_switch_cold | 986.02 / 989.29 / 989.29 | 986.97 / 989.13 / 989.13 |
| 0.7/cold_fallback_udp | 940.87 / 1035.40 / 1035.40 | 881.84 / 959.21 / 959.21 |
| 0.7/reconnect | 563.24 / 637.21 / 637.21 | 442.69 / 554.31 / 554.31 |
| 0.7/warm_new_process | 556.97 / 664.42 / 664.42 | 466.63 / 568.46 / 568.46 |
| 0.7/map_switch_cold | 1187.40 / 1384.40 / 1384.40 | 1188.05 / 1191.03 / 1191.03 |

| Протокол / сценарий | Изменение p50 playable |
|---|---:|
| 0.6/cold_fallback_udp | +4.38% |
| 0.6/reconnect | +19.02% |
| 0.6/warm_new_process | +28.52% |
| 0.6/map_switch_cold | -0.10% |
| 0.7/cold_fallback_udp | +6.27% |
| 0.7/reconnect | +21.40% |
| 0.7/warm_new_process | +16.22% |
| 0.7/map_switch_cold | -0.05% |

## Connect → full scene CPU submission

| Протокол / сценарий | Baseline p50 / p95 / max, мс | Prototype p50 / p95 / max, мс |
|---|---:|---:|
| 0.6/cold_fallback_udp | 637.55 / 769.74 / 769.74 | 609.61 / 642.90 / 642.90 |
| 0.6/reconnect | 572.06 / 672.11 / 672.11 | 463.26 / 537.76 / 537.76 |
| 0.6/warm_new_process | 603.04 / 652.59 / 652.59 | 431.08 / 593.34 / 593.34 |
| 0.6/map_switch_cold | 986.02 / 989.29 / 989.29 | 986.97 / 989.13 / 989.13 |
| 0.7/cold_fallback_udp | 940.87 / 1035.41 / 1035.41 | 881.85 / 959.22 / 959.22 |
| 0.7/reconnect | 563.24 / 637.22 / 637.22 | 442.69 / 554.31 / 554.31 |
| 0.7/warm_new_process | 556.98 / 664.42 / 664.42 | 466.63 / 568.46 / 568.46 |
| 0.7/map_switch_cold | 1187.40 / 1384.41 / 1384.41 | 1188.05 / 1191.03 / 1191.03 |

## Critical resource preparation

| Протокол / сценарий | Baseline p50 / p95 / max, мс | Prototype p50 / p95 / max, мс |
|---|---:|---:|
| 0.6/cold_fallback_udp | 283.40 / 303.81 / 303.81 | 152.69 / 157.26 / 157.26 |
| 0.6/reconnect | 253.49 / 264.85 / 264.85 | 133.70 / 140.76 / 140.76 |
| 0.6/warm_new_process | 280.58 / 283.18 / 283.18 | 150.04 / 153.69 / 153.69 |
| 0.6/map_switch_cold | 602.08 / 618.67 / 618.67 | 538.07 / 549.31 / 549.31 |
| 0.7/cold_fallback_udp | 251.05 / 265.65 / 265.65 | 153.29 / 170.09 / 170.09 |
| 0.7/reconnect | 220.55 / 224.39 / 224.39 | 126.81 / 148.13 / 148.13 |
| 0.7/warm_new_process | 247.36 / 252.52 / 252.52 | 149.39 / 183.67 / 183.67 |
| 0.7/map_switch_cold | 563.91 / 606.92 / 606.92 | 532.73 / 557.83 / 557.83 |

## Суммарная работа подготовки: OnConnected + deferred steps

| Протокол / сценарий | Baseline p50 / p95 / max, мс | Prototype p50 / p95 / max, мс |
|---|---:|---:|
| 0.6/cold_fallback_udp | 283.46 / 303.89 / 303.89 | 262.93 / 269.53 / 269.53 |
| 0.6/reconnect | 253.53 / 264.90 / 264.90 | 244.02 / 255.51 / 255.51 |
| 0.6/warm_new_process | 280.62 / 283.23 / 283.23 | 258.18 / 264.84 / 264.84 |
| 0.6/map_switch_cold | 602.12 / 618.72 / 618.72 | 599.07 / 611.64 / 611.64 |
| 0.7/cold_fallback_udp | 251.10 / 265.72 / 265.72 | 231.18 / 260.79 / 260.79 |
| 0.7/reconnect | 220.60 / 224.43 / 224.43 | 204.49 / 232.77 / 232.77 |
| 0.7/warm_new_process | 247.40 / 252.56 / 252.56 | 225.87 / 273.72 / 273.72 |
| 0.7/map_switch_cold | 563.96 / 606.97 / 606.97 | 562.64 / 588.70 / 588.70 |

Эта сумма считает elapsed scopes, включая CPU/IO/render waits, но не ожидание очереди между кадрами. Не суммируются вложенные map_images/map_layers scopes второй раз. Уменьшение critical duration само по себе не означает уменьшение общего объёма работы; последний в ряде сценариев увеличился.

## ConReady → завершение всех ресурсов, wall time

| Протокол / сценарий | Baseline p50 / p95 / max, мс | Prototype p50 / p95 / max, мс |
|---|---:|---:|
| 0.6/cold_fallback_udp | 286.14 / 306.70 / 306.70 | 308.04 / 313.09 / 313.09 |
| 0.6/reconnect | 253.65 / 265.01 / 265.01 | 286.64 / 298.11 / 298.11 |
| 0.6/warm_new_process | 280.88 / 284.16 / 284.16 | 299.47 / 305.68 / 305.68 |
| 0.6/map_switch_cold | 602.20 / 618.78 / 618.78 | 605.99 / 622.03 / 622.03 |
| 0.7/cold_fallback_udp | 251.33 / 266.03 / 266.03 | 271.39 / 299.97 / 299.97 |
| 0.7/reconnect | 220.70 / 224.54 / 224.54 | 245.72 / 274.11 / 274.11 |
| 0.7/warm_new_process | 248.07 / 252.80 / 252.80 | 265.48 / 312.08 / 312.08 |
| 0.7/map_switch_cold | 564.05 / 607.03 / 607.03 | 568.53 / 593.59 / 593.59 |

## Максимальный интервал между PumpNetwork до full submission

| Протокол / сценарий | Baseline p50 / p95 / max, мс | Prototype p50 / p95 / max, мс |
|---|---:|---:|
| 0.6/cold_fallback_udp | 285.99 / 306.72 / 306.72 | 165.88 / 184.49 / 184.49 |
| 0.6/reconnect | 253.59 / 264.96 / 264.96 | 133.80 / 140.87 / 140.87 |
| 0.6/warm_new_process | 280.69 / 283.90 / 283.90 | 151.45 / 181.84 / 181.84 |
| 0.6/map_switch_cold | 602.19 / 618.80 / 618.80 | 557.06 / 573.93 / 573.93 |
| 0.7/cold_fallback_udp | 251.46 / 266.07 / 266.07 | 165.72 / 185.31 / 185.31 |
| 0.7/reconnect | 220.65 / 224.51 / 224.51 | 129.78 / 148.32 / 148.32 |
| 0.7/warm_new_process | 247.93 / 252.62 / 252.62 | 150.76 / 183.79 / 183.79 |
| 0.7/map_switch_cold | 564.04 / 607.06 / 607.06 | 540.39 / 570.52 / 570.52 |

Это wall-clock service gap, включая scheduling/sleep/IO/graphics waits; он не называется чистым CPU time. Отдельный scope PrepareNext непосредственно показывает непрерываемую main-thread работу.

## Deferred stalls прототипа

| Протокол / сценарий | Самый долгий image step p50 / p95 / max, мс | Самый большой deferred network gap p50 / p95 / max, мс |
|---|---:|---:|
| 0.6/cold_fallback_udp | 27.40 / 28.72 / 28.72 | 27.56 / 28.94 / 28.94 |
| 0.6/reconnect | 27.41 / 32.39 / 32.39 | 27.53 / 32.51 / 32.51 |
| 0.6/warm_new_process | 26.52 / 30.63 / 30.63 | 26.61 / 30.93 / 30.93 |
| 0.6/map_switch_cold | 26.51 / 29.00 / 29.00 | 26.66 / 28.59 / 28.59 |
| 0.7/cold_fallback_udp | 12.19 / 14.71 / 14.71 | 11.04 / 13.87 / 13.87 |
| 0.7/reconnect | 11.95 / 13.98 / 13.98 | 12.21 / 14.26 / 14.26 |
| 0.7/warm_new_process | 11.82 / 14.43 / 14.43 | 12.27 / 14.86 / 14.86 |
| 0.7/map_switch_cold | 11.87 / 14.60 / 14.60 | 10.43 / 13.13 / 13.13 |

Deadline проверяется только между заданиями. Decode/upload одного изображения нельзя прервать: именно здесь нарушается budget. В этой выборке всё deferred завершилось до ONLINE, поэтому фриз уже управляемого персонажа не наблюдался. Это не доказывает отсутствие такого фриза при другой фазе, более быстрой bootstrap-синхронизации или более тяжёлой карте. Считать скрытые snapshot-ожиданием 12–33 мс паузы полноценной неблокирующей подготовкой нельзя.

## Пиковый working set процесса

| Протокол / сценарий | Baseline p50 / p95 / max, MiB | Prototype p50 / p95 / max, MiB |
|---|---:|---:|
| 0.6/cold_fallback_udp | 339.93 / 357.80 / 357.80 | 290.28 / 292.20 / 292.20 |
| 0.6/reconnect | 407.61 / 491.38 / 491.38 | 350.38 / 393.47 / 393.47 |
| 0.6/warm_new_process | 342.57 / 344.04 / 344.04 | 292.38 / 295.57 / 295.57 |
| 0.6/map_switch_cold | 447.13 / 448.34 / 448.34 | 410.72 / 411.90 / 411.90 |
| 0.7/cold_fallback_udp | 345.09 / 391.37 / 391.37 | 294.77 / 389.57 / 389.57 |
| 0.7/reconnect | 407.28 / 490.16 / 490.16 | 391.45 / 418.20 / 418.20 |
| 0.7/warm_new_process | 341.86 / 344.63 / 344.63 | 291.32 / 295.60 / 295.60 |
| 0.7/map_switch_cold | 446.05 / 446.95 / 446.95 | 410.19 / 413.42 / 413.42 |

GetProcessMemoryInfo.PeakWorkingSetSize — lifetime peak процесса на момент sample. У reconnect он включает предыдущее cold подключение; у map switch — предыдущую карту. Это не размер cache и не incremental allocation конкретной сцены. VRAM peak недоступен в используемой телеметрии; значения не оценивались по размеру texture files.

## Остаточные задержки и риски

Snapshot bootstrap остаётся основным сетевым нижним пределом: штатные две snapshots и около 200 мс между ними. Также остаются фаза первой snapshot, UDP transfer/fallback cold и обязательные tile images/layers. На Sunny Side Up основная работа относится к оставшимся critical ресурсам, поэтому map switch p50 практически не изменился. Ускорение Tutorial warm/reconnect нельзя переносить на произвольные карты.

Риски: одиночные непредсказуемые decode/upload stalls; неполная декоративная сцена при более раннем ONLINE; quad artwork может быть важен для читаемости карты; визуальный blending/order нужно проверять на широком map suite; custom background и смена backend/context требуют отдельной матрицы; budget не жёсткий, очередь не ограничивает размер одного изображения. Cache/device reset/eviction не реализованы и не проверялись. Безопасный следующий эксперимент — отделить CPU decode от upload с ownership/cancel generation и ограничивать размер GPU upload, до расширения deferred tile layers или добавления cache.

## Validation

- Prototype и восстановленный штатный клиент успешно собраны Release x64; production src в конце совпадает с исходным HEAD.
- Итоговые baseline 80/80 и prototype 80/80 переходов получили character/full-scene submission: cold, warm, reconnect и разные карты в обоих протоколах.
- 10 disconnect во время незавершённой staged подготовки на протокол, затем успешный полный вход: всего 20 отмен. Server connlimit поднят до 100 только в cancellation fixture; packet format не менялся. После финального входа joinprobe_right удерживает обычный CControls input; полученная snapshot character X увеличивается более чем на 16 units в обоих протоколах. Это подтверждает движение после входа, но не измеряет момент первого server input application и не является детерминированным сравнением physics replay.
- Выбраны Network, Net, NetAddr, ChunkHeader, Huffman, MapDownload, MapBugs, Mapitems, Datafile, Snapshot, Packer, GameWorld, Tunings: **57/58 passed**. Net.Ipv4AndIpv6Work не получил IPv6 UDP loopback packet (строки 48–49); отдельный повтор дал тот же отказ. Testrunner и его network code прототипом не изменялись.
- Existing map_download_06/map_download_07: **2/2 passed**, включая сравнение downloaded bytes/SHA с сервером. Existing client_can_connect_7: passed; websockets skipped (не собран). client_can_connect через localhost: timeout, воспроизведён сохранённым baseline executable. Числовой IPv4 loopback в основной матрице работал на обоих протоколах. Причина hostname/IPv6 отказов не исправлялась в рамках этой задачи; все тесты зелёными не объявляются.
- Collision/gameplay loading functions и очистка snapshots не менялись; смена карты с другой geometry проверена положительным входом. Побайтовый differential collision replay, freeze/teleport/switch gameplay suite, sanitizer/UAF instrumentation и исторические/native servers не проверены. Cancellation smoke не заменяет доказательство отсутствия всех lifetime bugs.
- Штатный screenshot был запрошен через console. Захват не выполнен: ScreenshotDirect пропускает его при неактивном окне. Автоматического pixel comparison/визуальной приёмки нет. Поэтому полноценная визуальная эквивалентность не заявляется; full submission означает готовность очередей плюс renderer pass.

Логи отказов и успешных проверок сохранены рядом с JSON. Pilot failure cancellation fixture из-за штатного connlimit устранён настройкой локального fixture; connection-rate protection клиента/сервера не изменялась.

## Воспроизводимость и состояние Git

Архив: [staged-map-prototype.patch](instant-join/staged-map-prototype.patch), [raw baseline](instant-join/fast-join-baseline.json), [raw prototype](instant-join/fast-join-prototype.json), [summary](instant-join/fast-join-summary.json), [metadata](instant-join/fast-join-metadata.json), [cancellation](instant-join/fast-join-cancellation.json). Metadata содержит hashes карт, stdout и patch, actual graphics context; полные process logs остаются по work paths в out.

В Developer PowerShell из корня, с исходниками без probes:

1. Baseline: `python -B docs/audits/instant-join/instrument.py apply`; `cmake --build out/build/x64-Release --target game-client -j 6`; установить `$env:FAST_JOIN_VARIANT="baseline"`, удалить FAST_JOIN_OPTIONS, FAST_JOIN_VALIDATE, FAST_JOIN_SCREENSHOT_ONLY; `python -B docs/audits/instant-join/run_local.py out/build/x64-Release 10 OpenGL3`. Сохранить out/build/x64-Release/fast-join-baseline.json.
2. `python -B docs/audits/instant-join/instrument.py remove`; `git apply docs/audits/instant-join/staged-map-prototype.patch`; затем снова apply probes и собрать game-client. Не менять источники при установленных probes: removal намеренно проверяет hashes.
3. Установить `$env:FAST_JOIN_VARIANT="prototype"`, `$env:FAST_JOIN_OPTIONS="cl_staged_map_load 1"`; тот же runner с 10 повторами. Сохранить fast-join-prototype.json. Серию не совмещать со сборкой или другими нагрузками.
4. `python -B docs/audits/instant-join/cancel_staged.py out/build/x64-Release` выполняет 20 отмен и два movement checks. Проверяет только локальный сервер.
5. `python -B docs/audits/instant-join/summarize_fast.py docs/audits/instant-join/fast-join-baseline.json docs/audits/instant-join/fast-join-prototype.json` выводит все статистики. Output redirection не должен перезаписывать входные JSON.
6. В finally: снять probes через instrument.py remove, выполнить `git apply -R docs/audits/instant-join/staged-map-prototype.patch` и пересобрать game-client. Проверить отсутствие src diff/probes. Не применять reset/checkout к пользовательским файлам.

Коммит с запрошенным заголовком `perf(client): introduce staged map initialization`: **7c91de08ed84d9388e216de4c2dba2f0cd6b776c**. Он фиксирует стенд и probes. Rejected prototype patch и результаты фиксируются вместе с этим отчётом отдельным docs-коммитом; заголовок perf-коммита не означает, что staged path включён в production. Точный hash проверенного patch указан в metadata. Production base SHA приведён в начале отчёта.

Работа велась только в dev/lucent-core. Master, предшествующие коммиты и исходные аудитные материалы не переписывались; push, merge и PR не выполнялись. Экспериментальные src изменения и probes сняты. Сохранять данный coarse-grained pipeline в production не рекомендуется, несмотря на измеренный выигрыш на Tutorial.