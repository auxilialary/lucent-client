# Lucent HUD Design Correction & Integrity Check

Дата: 2026-11-02. Ветка: `feat/game-ui`, без переключения, коммита и PR.

## Границы проверки

Работа выполнена через чтение исходников, историю локальных задач, CLI-сборку и автоматические тесты. Computer Use, запуск игры, screenshots и GUI-проверка не выполнялись: это запрещено `AGENTS.md`. Внешний вид на реальных картах **не объявляется проверенным**.

В начале сохранены SHA-256 всех 2004 существовавших неигнорируемых файлов и побайтовые копии пяти файлов HUD в существующем `out/build/x64-Release`. Итоговая сверка обнаружила изменения только в:

- `src/game/client/lucent/hud/layout.h` — форма скруглений;
- `src/game/client/lucent/hud/visuals.h` — общие поверхности, контур, вторичный текст;
- `src/test/lucent_hud_test.cpp` — геометрия скруглений и расчёт контраста.

Этот новый отчёт добавлен отдельно. Остальные существовавшие файлы совпали с начальным снимком. Никакие незакоммиченные изменения не откатывались и не восстанавливались поверх текущего дерева. `hud.cpp`, оба файла alignment, integration/reset в `components/hud.*`, конфиги, gameplay, networking, prediction, scoreboard и ESC в этой коррекции не редактировались.

## Целостность после прежней ошибки кодировки

Проверка охватывает весь `hud/hud.cpp`, все три функции `tee_alignment.cpp`, связанные layout/state/text helpers и вызовы из `components/hud.*`.

Источники сопоставления:

1. `out/build/x64-Release/hud-recovered-before-polish.cpp`: полная версия до предыдущего прохода интеграции, 406 строк. Она использована как reference, а не скопирована обратно в исходники.
2. Сохранённые CLI-выводы исходников из локальных задач `01a19938-d77a-7e72-a126-f74783bff000` и `01a199b4-6713-7f91-b3df-b961230b9bf2`. История только читалась; исторические команды не исполнялись.
3. `lucent-hud-v1.6-preserved.json` с полными контрольными суммами функций status/hotbar/recording/coordinates. Их область хеширования воспроизведена точно: после сигнатуры до следующей функции; для hotbar исключено позднее добавленное присваивание `m_LucentHotbarBounds = Bar`.
4. `LUCENT-HUD-V1.6-REPORT.md`, `LUCENT-TEE-ALIGNMENT-V1.1-REPORT.md`, предыдущие HUD-отчёты и `LUCENT-HUD-DESIGN-AUDIT.md` — для проверки ожидаемых размеров, форматов времени, поведения и истории визуальных изменений.

**Результат:** 403 из 406 строк reference найдены непрерывными блоками в независимо сохранённых выводах исходников. Остальные строки 223, 229 и 392 — соответственно ninja progress, ammo text и вызов Panel для coordinates — входят в функции, чьи полные SHA-256 совпали с архивом v1.6. Совпали все четыре архивных хеша. Таким образом, reference проверен полностью совокупностью source reads и function hashes, включая конец файла. Для text helper и timer покрытие сохранёнными source reads полное.

Для alignment извлечён отдельный полный source read до прошлого изменения палитры, затем проверены все три функции. Сравнение учитывало форматирование; строковые литералы сохранялись при сравнении токенов. Текущий код **не должен** побайтово совпадать со старой версией: между ними были утверждённые изменения интеграции, перечисленные ниже.

| Функция | Проверенный результат сравнения со старой версией |
| --- | --- |
| `LucentText` | Весь helper: сохранены создание/recreate/delete контейнеров, строковый кэш, центрирование и возврат ширины. Отличия — target raster size для всех слотов, invalidation при смене высоты экрана, удалённые shadow/outline submissions. |
| `RenderLucentStatus` | Полное совпадение функции. Сохранены здоровье/ammo utility path, jump accounting, все modifier conditions, отдельные группы, wrapping и graphics state restore. |
| `RenderLucentHotbar` | Все inventory/protocol guards, порядок оружия, поворот/размер artwork, ammo и обе ветки ninja progress сохранены. Отличия — presentation-only transition для шести выделений и интерполяция яркости иконок. |
| `RenderLucentTimer` | Вся функция, включая последний warning path: сохранены условия main/clock/CP/finish, форматы, six-second lifetime, fade, измерение шаблонных ячеек, sudden death offset и центрирование. Отличия — token colors, общий Panel, плавная warning opacity вместо дискретного blink. |
| `RenderLucentRecording` | Сохранены guards, четыре recorder labels/states, elapsed times, FPS/pred offsets и избегание таймера. Отличия — target-size измерение и цвет recording dot из токена. |
| `RenderLucentCoordinates` | Сохранены controlled/spectator/freeview guards, получение position/speed/angle, формат строк и избегание status/score. Отличия — target font size, резервирование числовых ячеек, положение второй оси и сигнальные token colors. |
| `RenderLucentAlignment` | Вся функция: те же snapshot interpolation, eligibility, frozen confirmation, target selection/hysteresis, outgoing skin/name cache, два слоя, вертикальный stack, chat/HUD avoidance, реальные tee и snowflake. Отличия — Panel, avatar-well tint и frozen color из общих helpers; остальное — форматирование. |
| `PrepareAlignmentText` | Токены функции совпали полностью; различается форматирование. Сохранены kerning flags, advance measurement, target-size cache, font-height invalidation и восстановление text state. |
| `DrawAlignmentText` | Единственное содержательное отличие — общий цвет Text вместо прежнего RGB. Сохранены pixel-aligned container rendering, центрирование по bounding box, отсутствие shadow/outline/texture scaling. |

Дополнительно проверены `Jumps`, `Weapons`, все методы `CLayout`, `TimerFormatTemplate`, `TargetFontSize`, `BelowIfOverlapping`, alignment `FitLabel`, `Select`, `Frozen`, `BottomAbove`, `CMode::Update`, `CRowPosition::Update`, объявления caches и reset. Текущие отличия состояния alignment от v1.1 — ранее внесённые общие durations/cubic-out helper; в этой коррекции они не менялись. Все layout methods остались неизменны; в `layout.h` изменены только комментарий и значение radius cap.

Строгое декодирование UTF-8 файлов HUD и `components/hud.*` успешно; NUL и U+FFFD не обнаружены. Новые правки внесены адресно, без повторной реконструкции C++-реализации. Потери функций, необъяснённых замен условий или повреждения строк не найдено. Это проверка по доступным локальным свидетельствам, а не утверждение о наличии независимого Git-коммита для прежде untracked файлов.

Машиночитаемые свидетельства в `out/build/x64-Release`:

- `lucent-hud-correction-before.json` — SHA-256 начального дерева;
- `lucent-hud-correction-integrity-evidence.json` — reference hash, полное покрытие source reads по строкам и provenance;
- `lucent-hud-correction-functions.json` — инвентарь всех девяти функций, полные SHA-256 до/после прошлой интеграции, число строк и token comparison;
- `lucent-hud-correction-alignment-before-polish.cpp` — извлечённый независимый alignment source read;
- `lucent-hud-correction-workspace-check.json` — проверенные файлы, фактический scope изменений и численный расчёт контраста.

## Исправление формы и глубины

Предыдущий radius `Style::Radius × 0.5 = 1.5 HUD units` механически переносил форму небольшого menu input на игровые панели. Исправленная форма использует меньшую сторону панели: `min(width/2, height/2, 7 × localScale)`, с защитой от отрицательной геометрии. Cap 7 получен из существующей высоты control `Style::ControlSize × MenuUnit`; он ограничивает большие панели, а короткие сохраняют капсульную форму. Все anchors, extents, text sizes, padding, icon sizes, hotbar factor и collision budgets сохранены.

| Панель при HUD scale 100 | Высота, HUD units | Radius, HUD units |
| --- | --- | --- |
| Coordinates, одна строка | 8 | 4 |
| Jumps/modifiers или recording, одна строка | 12 | 6 |
| Timer main-only / main + clock / три строки | 15 / 22 / 31 | 7 |
| Alignment | 18 | 7 |
| Hotbar (внешняя панель, factor 0.85) | 20.4 | 5.95 |
| Hotbar slot (factor 0.85) | 17 | 5.95 |

Радиусы дополнительно ограничены шириной, поэтому узкие/нулевые прямоугольники безопасны. На 1080p радиус обычной крупной панели теперь 25.2 px вместо 5.4 px; у hotbar 21.42 px вместо 4.59 px. Это вычисления геометрии, не измерения screenshots.

Общий `Panel` теперь содержит статическую мягкую вертикальную градацию сливового цвета. Оба края производны от `Style::Surface` и `Style::ServerSelection` через существующий `Gradient::Blend`; веса — `SceneBase` и `Selected`. Независимых RGB-палитр не добавлено. Прозрачность берётся из существующего overlay token `Console::PopoverSurface.a = 0.94`, умноженного на пользовательскую HUD opacity. При default 85% непрозрачность основы — 0.799.

Контур рисуется отдельным периметром с `Style::Accent`, `BorderIdle` и преобразованным `Hairline`. Под всей внутренней областью больше нет полноразмерного rose fill, который смешивался бы с тёмной поверхностью. Active hotbar сохраняет мягкий rose fill (`Selected`) и более заметный контур (`BorderActive`), использует тот же radius/contour helper и прежнюю transition animation. Для увеличенных углов число сегментов дуги повышено с 4 до 8 на четверть; память фиксирована на стеке. Тяжёлых теней, blur, движущихся бликов и новых анимаций нет.

Все целевые компоненты, включая recording и alignment, получают оформление через общий helper. Внутренний кружок с настоящим tee сохранён. Freeze/error/positive semantics сохранены. Таймер и остальные подписи по-прежнему используют нативные target-size glyph containers, без повторных теневых копий и масштабирования готовой текстуры.

Вторичный текст слегка осветлён существующим Blend между `DetailText` и `Text`. Добавлен тест альфа-композитинга на белой/чёрной карте для обоих концов градации при default opacity. Расчётный contrast ratio primary/secondary составляет **4.91–10.91**, все восемь сочетаний проходят порог 4.5. Это модель sRGB alpha compositing; она не доказывает реальную coverage/filtering/back-end output и не распространяется на произвольно низкую пользовательскую opacity или переходные fading layers.

Этот отчёт уточняет прежний `LUCENT-HUD-DESIGN-AUDIT.md`: описанные там radius 1.5 и surface opacity 0.82 больше не относятся к итоговому оформлению. Старый отчёт не перезаписан.

## Проверки

Сборка: `./scripts/build-lucent-vs.ps1 -Targets game-client,testrunner,lucent-browser-test,lucent-menu-test,lucent-settings-test,lucent-menu-effects-test,lucent-radial-transition-test` — **PASS**. Использованы сохранённый VS selection `x64-Release`, его существующий `out/build/x64-Release`, MSVC x64 и CMake type `RelWithDebInfo`. Новый build tree не создавался; `Lucent Client.exe` успешно слинкован.

| Проверка | Результат |
| --- | --- |
| `LucentHud.*:LucentTeeAlignment.*:LucentGameView.*` | **27/27 PASS** |
| Browser, menu, settings, menu effects, radial standalone tests | **Все пять PASS** |
| Полный testrunner | 489 tests: **484 passed, 4 skipped, 1 failed**; 3 disabled не запускались |
| Регрессия без `Net.Ipv4AndIpv6Work` | 488 tests: **484 passed, 4 skipped**; 3 disabled не запускались |
| `git diff --check` | **PASS** |
| Сверка начальных файлов по SHA-256 | Только три целевых файла изменены; остальные исходные файлы сохранены |

Единственный сбой полного набора — прежний `Net.Ipv4AndIpv6Work`: IPv6 loopback receive timeout в `src/test/net_test.cpp:48–49`, wait возвращает 0, recv −1 вместо 3. IPv4 assertions завершились до этого. Такой же сбой зафиксирован предыдущим аудитом и повторился в текущем полном прогоне. Сетевой код не менялся; причина не диагностировалась в этой задаче. Полная регрессия **не объявляется зелёной**. Четыре relay integration tests пропущены из-за их существующих требований к fixtures/environment.

Логи: `lucent-hud-correction-build.log`, `lucent-hud-correction-focused.log`, `lucent-hud-correction-regression.log`, `lucent-hud-correction-regression-without-network.log` в `out/build/x64-Release`.

## Оставшиеся визуальные ограничения

Нужна ручная проверка композиции и контраста на ярких/тёмных картах, сглаживания увеличенных углов, small-scale текста, кастомных skins и crossfade в реальном renderer. Она здесь не выполнялась. Низкая HUD opacity снижает контраст по определению; пользовательское значение не переопределяется. Существующие пределы glyph atlas 6–128 px, conservative chat envelope и подавление alignment при отсутствии безопасного места сохранены. `lucent_hud 0` dispatch не изменён; его внешний вид также не проверялся через GUI.
