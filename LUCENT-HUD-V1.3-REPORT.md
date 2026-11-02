# Lucent HUD v1.3 — Timer, Status & Information Layout

Дата: 2026-11-02. Ветка: **`feat/game-ui`**. PR не создавался.

## Реализовано

Использованы существующие `CHud` render methods, HUD textures/quad containers, текстовый кэш и `LucentHud::CLayout`. Новых настроек, HUD Editor, Media Island, media player или внешних зависимостей нет. ESC, scoreboard, physics, networking, prediction и weapon behavior на этом этапе не изменялись. В chat header добавлен только read-only запрос состояния; обработка ввода, отправка сообщений и рендер чата не менялись.

### Общий контейнер часов и таймера

- Локальные часы и игровое время находятся в одном компактном контейнере сверху по центру. Центрируется **полная ширина контейнера**, включая часы, divider, интервалы и игровой блок, а не только крупные цифры.
- Часы показывают прежнее локальное время `%H:%M`, размером **5 HUD units**. Основное игровое время остаётся размером **14**, checkpoint/finish — **6**.
- Когда одновременно есть часы и игровой блок, между ними рисуется вертикальный разделитель шириной **0.5**, с зазорами **4** с каждой стороны. Внешний inset — **2**, gap между основным и дополнительным игровым временем — **3**. Все размеры масштабируются существующим `lucent_hud_scale`.
- Часы вертикально центрированы в контейнере. Основное игровое время и дополнительная строка выровнены в правой колонке. При отсутствии часов игровой блок снова центрируется самостоятельно; при отсутствии игрового времени остаётся компактный контейнер часов без divider.
- `cl_show_local_time_always` управляет часами во время обычного gameplay. Если он выключен, часы не добавляются принудительно. `cl_showhud_timer` по-прежнему управляет основным временем; checkpoint/finish сохраняют свои существующие условия показа и fade.
- Старые отдельные часы при обычном Lucent HUD больше не рисуются, дублирования нет. При открытом scoreboard сохранён прежний путь локальных часов; layout самого scoreboard не менялся. Legacy HUD при `lucent_hud 0` сохранён.
- При sudden death clock-only контейнер, если он нужен по настройке, располагается ниже существующей надписи Sudden Death.

Источник основного времени не менялся: `RenderGameTimer` вычисляет elapsed round time, остаток time limit либо race time из существующих game ticks. Дополнительная строка по-прежнему показывает checkpoint difference или finish time/difference из текущего HUD state. Сохранены шестисекундный срок показа, fade после четырёх секунд, цвета разницы и last-minute/blink состояния.

### Условия отображения AUTO/REC/RACE/REPLAY

Плашка рисуется, только когда одновременно выполняются условия:

1. Lucent HUD и существующий master HUD разрешены текущим режимом отображения.
2. Включён `lucent_hud_recording`, opacity больше нуля, scoreboard не открыт.
3. Чат **сейчас развёрнут**, по тому же выражению, которое использует `CChat::OnPrepareLines`:
   - удерживается `+show_chat`; либо
   - открыт ввод чата и `cl_showchat == 1`; либо
   - `cl_showchat == 2`, то есть большая область чата включена постоянно.
4. Отображение чата разрешено существующими normal/video settings.
5. Хотя бы один из существующих manual/auto/race/replay demo recorders действительно записывает.

При обычном gameplay с `cl_showchat 1`, без ввода и без `+show_chat`, плашка скрыта, даже если запись продолжается. При `cl_showchat 2` она может оставаться видимой: чат в этом режиме по определению постоянно развёрнут. Если чат скрыт настройками, одного режима ввода или запроса раскрытия недостаточно.

Для проверки используется `CChat::IsExpanded()` с текущими input/history/config состояниями. Не используется `m_PrevShowChat`, который обновляется позже HUD render pass и мог бы дать задержку на кадр. Chat renderer, bindings и команды не переписывались. Demo recorder APIs только читаются; start/stop и запись данных не менялись.

### Coordinates

- Постоянная плотная подложка удалена. Остались светлый текст, тонкая чёрная обводка и небольшая тень существующего text helper.
- Font size: **6 → 5 HUD units**; горизонтальный padding: **4 → 2**; промежуток между Vx/Vy: **8 → 4**. В строке X/Y/angle оставлен минимальный пробельный интервал.
- Высота резервируемой области одной/двух строк: **13/22 → 8/15**. Основная строка начинается с Y inset **1.5**, строка скорости — с **8.5**.
- X/Y сохраняют две цифры после запятой, угол — одну. Данные по-прежнему приходят из `GetMovementInformation`; freeview/spec-character не получает выдуманных angle/speed значений.
- `lucent_hud_coordinates`, `cl_showhud_player_speed` и существующие master HUD settings сохранены. Сигнальные цвета скорости не менялись.
- Если movement block пересекает статусную группу по ширине, он поднимается выше её верхней границы с полным зазором **4 scaled units**. Текущие bounds используются для резерва места spectator/dummy indicators.

### Прыжки и активные модификаторы над hotbar

- Jump indicators и capability/status icons больше не рисуются в верхней левой utility zone при включённом Lucent HUD. Там сохранены health/armor. Legacy player-state rendering остаётся для выключенного Lucent HUD.
- Новый компактный ряд использует существующие HUD textures: jumps, endless jump/hook, jetpack, teleport capabilities, solo/collision/hit restrictions, lock/practice/team0 modes, deep/live freeze. Только активные состояния и прежние условия доступности данных; таблицы numeric tuning нет.
- Сохранены `cl_showhud_jumps_indicator`, `cl_showhud_ddrace`, server HUD capabilities, extended-data и spectator/multiview ограничения.
- Размер статусной иконки — **8 HUD units**, gap — **2**, padding строки — **2**, высота строки — **12**, зазор между строками — **2**.

#### Большое количество прыжков

- При достоверном счётчике до **6** показываются полные/пустые иконки доступных и использованных прыжков.
- Более **6** отображаются одним компактным элементом вида **`240/255 (15 used)`**: доступные / всего и точное число использованных. Число не обрезается до прежних десяти.
- Расчёт сохраняет существующие ground/air jump, spent-jump-bit, endless-jump и recovery правила. Никаких записей в character core или physics нет.
- Если snapshot сообщает общее количество, но не содержит достаточной extended display information для доступного остатка, показывается **`?/255`**, а не ложное утверждение, что все прыжки доступны.
- Нулевой jump count не создаёт пустую группу; активный endless-jump modifier при этом может отображаться своей иконкой.

#### Центрирование и длинные наборы

- Каждая непустая строка центрируется целиком относительно середины экрана. Короткий набор не растягивается на фиксированную широкую панель.
- Элементы упаковываются в строки с ограниченной шириной: максимум 44% экрана, 180 scaled HUD units и доступная ширина с margins. При наличии места дополнительно резервируется ширина существующего чата симметрично относительно центра.
- При нехватке горизонтального места элементы переносятся вверх в дополнительные центрированные строки. Numeric jump label при необходимости подгоняется по выделенной ширине, иконки не пропадают.
- На узких экранах или при очень широком чате, когда центрированной полосы недостаточно, статусная группа поднимается выше обычной области последних сообщений, не сдвигая hotbar. Произвольные expanded-chat overlays требуют ручной оценки: геометрические тесты не являются проверкой фактического кадра чата.
- Массив ограничен **32** элементами; текущий набор укладывается в него: до 6 jump icons и 19 modifiers, либо один numeric jump element и modifiers. Используются фиксированные stack arrays, без per-frame growing containers.
- **Hotbar не сдвигается:** его размеры, Y-anchor, слоты, sprite scale 0.92, наклон `pi * 7 / 4`, alpha, weapon textures и ammo/ninja behavior сохранены. Статусная группа привязана над ним, её появление/исчезновение не влияет на его геометрию.

## Файлы этого этапа

| Файл | Изменение |
| --- | --- |
| `src/game/client/components/chat.h` | Только read-only expanded-state query и его pure predicate |
| `src/game/client/components/hud.h` | Clock/jump text-cache slots и bounds информационных групп |
| `src/game/client/components/hud.cpp` | Устранение отдельного gameplay clock, сброс bounds, согласование movement/spectator layout |
| `src/game/client/lucent/hud/hud.cpp` | Combined timer, recording gate, компактные coordinates и перенос active statuses |
| `src/game/client/lucent/hud/layout.h` | Clock-aware timer bounds, status packing, jump counts и clearance helpers |
| `src/test/lucent_hud_test.cpp` | Новые состояния и геометрические regression checks |
| `LUCENT-HUD-V1.3-REPORT.md` | Этот отчёт |

Остальные рабочие изменения предыдущих этапов сохранены. Новых config variables нет. `chat.cpp`, scoreboard и gameplay/network/prediction code не редактировались.

## Сборка и проверки

```powershell
./scripts/build-lucent-vs.ps1 -Targets game-client,testrunner,lucent-browser-test,lucent-menu-test,lucent-radial-transition-test,lucent-settings-test,lucent-menu-effects-test
```

**PASS**, exit 0, MSVC x64, существующий `out/build/x64-Release/Lucent Client.exe`. Сохранённый Visual Studio профиль называется **x64-Release**, фактический build type — **RelWithDebInfo**, как и на предыдущих этапах. Другой build tree или чистый Release preset не создавался.

| Проверка | Результат |
| --- | --- |
| `testrunner.exe --gtest_filter=LucentHud.*:LucentGameView.* --gtest_color=no` | **PASS, 10/10** |
| Expanded-chat predicate | **PASS:** ordinary, input, held history, always-large, hidden-chat setting combinations |
| Combined clock/timer centering | **PASS:** main/secondary on/off и clock on/off; общий центр и divider spacing |
| Jump counts | **PASS:** ground-only, spent-jump recovery, 255 jumps, endless state, unknown data и integer boundary safety |
| Status packing / hotbar stability | **PASS:** 0–32 элемента, mixed numeric/icon widths, несколько aspect ratios, scale 50/75/100/125/150%, chat widths 0/200/400 |
| Movement/status clearance | **PASS:** координаты со скоростью/без неё, score HUD on/off, полный зазор над status bounds |
| Существующие layout/weapon/config regression checks | **PASS**, включая 1920×1080 и 2560×1440 |
| `lucent-menu-test.exe` | **PASS** |
| `lucent-browser-test.exe` | **PASS** |
| `lucent-menu-effects-test.exe` | **PASS** |
| `lucent-radial-transition-test.exe` | **PASS** |
| `lucent-settings-test.exe .` | **PASS** |
| `git diff --check` | **PASS** |

Build log: `out/build/x64-Release/lucent-hud-v1.3-build.log`. Профильный XML: `out/build/x64-Release/lucent-hud-v1.3-tests.xml`. Полный общий набор тестов не запускался.

## Что не проверено вручную

**GUI-проверка не выполнялась.** Computer Use, screenshot capture и GUI automation не использовались согласно AGENTS.md; игра для просмотра оформления не запускалась. Пользователю остаётся оценить типографику и читаемость общего таймера/coordinates, custom HUD/game skins, реальные jump/modifier transitions, раскрытие чата и video-capture режимы. Геометрия и условия проверены автоматически; отсутствие пересечений при произвольных пользовательских overlay-настройках не заявляется как визуально подтверждённое.
