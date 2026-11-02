# Lucent HUD Border Cleanup / Panel Quality Pass

Дата: 2026-11-02. Ветка: `feat/game-ui`. Коммит и PR не создавались.

## Причины в существующем paint path

Через `rg` проверены `hud/visuals.h`, все HUD call sites, `CLayout`, native rounded rectangles в `graphics_threaded.cpp`, text container alignment в `text.cpp`, а также Lucent checkbox/selection coverage helpers.

Предыдущая заливка использовала native `DrawRect4`, контур — отдельную жёсткую mesh-полосу с фиксированными восемью сегментами на угол. У контура не было прозрачных coverage-краёв; fill и stroke не использовали одну геометрию. Bounds/radius оставались дробными, а stroke шириной `Hairline × MenuUnit × Scale` часто занимал меньше одного физического пикселя. Selected contour имел яркий Accent с alpha 0.40 до пользовательской opacity. Вместе это делало дискретность растеризации заметнее, особенно на углах и при движении alignment rows.

Native text renderer уже умеет округлять translated containers. Однако non-timer `LucentText` мог наследовать `TEXT_RENDER_FLAG_NO_PIXEL_ALIGNMENT` из внешнего render state. Это возможность по исходникам; наличие такого флага в конкретном пользовательском кадре не измерялось.

## Что изменено

### Общая геометрия и pixel grid

Добавлен `src/game/client/lucent/hud/panel_geometry.h` с paint-only `CPixelGrid` и `CPanelGeometry`. Все scoped panels используют эту геометрию через прежний общий `Panel` helper: timer, coordinates, hotbar, независимые jumps/modifiers capsules, alignment; recording также получает тот же общий paint path.

- Размер физического пикселя берётся из текущего `Graphics()->GetScreen()` и `ScreenSize()`, как у существующих Lucent UI helpers. Нет привязки к фиксированному разрешению.
- Каждая сторона paint bounds округляется к ближайшей границе device pixel. Максимальный сдвиг отдельной стороны — половина пикселя; изменение paint width/height ограничено одним пикселем. Исходный logical rectangle не изменяется и не записывается обратно в layout/collision state.
- Nominal radius остаётся прежним: ограничение половиной меньшей стороны и cap 7 HUD units с local scale. Paint radius округляется на device grid, затем ограничивается snapped geometry; у нечётной капсулы допускается half-pixel radius для симметрии.
- Border width по-прежнему производна от `Style::Hairline × MenuUnit × localScale`, но квантуется с шагом половины physical pixel. Минимальная геометрическая ширина — половина пикселя с пониженной peak coverage, а не принудительный непрозрачный толстый stroke. Tiny geometry дополнительно ограничивает ширину по radius.
- Углы используют адаптивную tessellation по physical radius, до 32 segments на четверть окружности. В проверенном диапазоне chord error не превышает 0.126 physical pixel.
- Отдельные arc vertices не округляются: их округление обратно создало бы ломаную. Snapping применяется к bounds/radius/stroke placement; curve samples остаются непрерывными.

### Coverage вместо жёстких краёв

`visuals.h` больше не смешивает native hard-edged fill с отдельным hard-edged annulus. Fill и border получают один `CPanelGeometry` и совпадающие наружные boundaries.

Заливка состоит из неперекрывающегося core fan и прозрачной fringe. Border имеет прозрачный внутренний край, peak/plateau и прозрачный наружный край. Подход повторяет coverage mesh, уже используемый Lucent checkbox rings и selection backgrounds; цвета интерполируются стандартным vertex-color path. Core и fringe используют согласованный порядок вершин/цветов из `LucentUi::CheckboxMesh`.

У обычных HUD-размеров переход coverage занимает один physical pixel вокруг соответствующей границы. Наружная fringe заканчивается на половине пикселя за snapped contour. Для экстремально маленьких shapes feather ограничивается четвертью radius, чтобы внутренние контуры не инвертировались. Максимальный paint/AA вылет относительно исходной logical side — примерно один physical pixel, включая snapping; это не изменение layout.

Нет blur, render targets, textures для панелей, тяжёлых теней, новых shader programs или новых анимаций. Mesh storage фиксирован на стеке, до 132 boundary points. Дополнительная geometry относительно старого hard edge есть; runtime GPU/CPU profiling не выполнялся, ускорение не заявляется.

### Lucent tokens и selected slot

| Свойство | Итоговое поведение |
| --- | --- |
| Основная поверхность | Прежний `PanelSurface`: Blend существующих Surface/ServerSelection, прежняя plum gradient и overlay opacity 0.94 × HUD alpha |
| Idle border color | Blend существующих DetailText/Accent с весом Selected; мягче прежнего полного Accent |
| Idle border alpha | `Style::SurfaceGlow` = 0.07 вместо 0.12, затем HUD alpha и subpixel coverage |
| Selected fill | Прежний rose Accent × `Style::Selected` = 0.13, с сохранённой selection animation |
| Selected border alpha | `SurfaceGlow + BorderIdle` = 0.19 вместо 0.40; тот же coverage helper, без жёсткого зубчатого annulus |
| Border thickness | Общая Hairline-derived policy у idle/selected; selected не утолщается ради акцента |

Независимых RGB-цветов, конфигов или второй дизайн-системы не добавлено. `style/tokens.h` и `style/gradient.h` не редактировались. Контрастный фон и цвета текста сохранены; существующий numerical contrast test продолжает проходить.

Alignment avatar well теперь получает ту же soft fill coverage вместо отдельного hard-edged `DrawRect`. Размер/позиция настоящего tee и его rendering path не менялись.

### Text anchors

`LucentText` и `DrawAlignmentText` используют ту же device grid для конечного anchor. У non-timer containers явно снимается только inherited `NO_PIXEL_ALIGNMENT`; остальные прежние render flags сохраняются. Native pixel alignment остаётся включённым, повторное округление уже snapped anchor идемпотентно.

Не менялись font family, target glyph size, текстовые строки/форматы, код измерения/кэширования ширин или time-cell измерения. Не добавлены outline, shadow copy или масштабирование готовых glyph textures.

## Сохранность layout и логики

В начале сохранён SHA-256 snapshot 2010 существовавших неигнорируемых файлов. Из них изменены только:

- `hud/visuals.h` — общий paint path;
- `hud/hud.cpp` — render flags и конечные text anchors в `LucentText`;
- `hud/tee_alignment.cpp` — soft avatar backing и text anchor;
- `src/test/lucent_hud_test.cpp` — три geometry/coverage tests.

Добавлены `hud/panel_geometry.h` и этот аудит. Все остальные существовавшие source/assets/scripts сохранены. В частности, `layout.h`, `weapon_scale.h`, pickup textures, game/HUD skins, texture filtering, shared style tokens, engine, gameplay, networking, prediction и scoreboard не менялись.

Функции `RenderLucentStatus`, `RenderLucentHotbar`, `RenderLucentTimer`, `RenderLucentRecording`, `RenderLucentCoordinates` совпали с начальным snapshot полностью. `PrepareAlignmentText` также не менялась. В `RenderLucentAlignment` изменён только paint avatar well; target selection, snapshot conditions, hysteresis, reflow и настоящие tee сохранены. Масштаб оружия 0.75↔1.0, наклон, alpha и slot selection timings остались прежними. Ветки `lucent_hud 0` не редактировались.

Свидетельства: `cmake-build-release/lucent-hud-border-before.json`, побайтовые `lucent-hud-border-before-*` copies и `lucent-hud-border-integrity.json`.

## Build и тесты

Использован основной CLion workflow: сохранённый профиль **Release**, `cmake-build-release`, Ninja, MSVC x64/SDK из его cache. VS fallback не запускался, новые CMake build trees не создавались.

```powershell
./scripts/build-lucent-clion.ps1 -Targets game-client,testrunner
./cmake-build-release/testrunner.exe '--gtest_filter=LucentHud.*:LucentTeeAlignment.*:LucentGameView.*' --gtest_color=no
./scripts/build-lucent-clion.ps1 -Test
# Отдельный явный диагностический повтор после известного сбоя:
./cmake-build-release/testrunner.exe '--gtest_filter=-Net.Ipv4AndIpv6Work' --gtest_color=no --gtest_brief=1
git diff --check
```

| Проверка | Результат |
| --- | --- |
| Клиент и testrunner | **Build PASS** |
| Полный default build: all/client/testrunner/все Lucent standalone targets | **Build PASS**, без clean/reconfigure |
| Профильные HUD/alignment/gameview tests | **33/33 PASS** |
| Полный C++ suite | 495 tests: **490 passed, 4 skipped, 1 failed**; 3 disabled не запускались |
| Все Lucent standalone executables | **6/6 PASS** |
| Rust target | **18 doctests PASS**; остальные groups — 0 tests |
| Диагностический C++ rerun без IPv6 test | 494 tests: **490 passed, 4 skipped**, exit 0 |
| `git diff --check` | **PASS** |

Три новых tests проверяют все пять scoped component shapes, resolutions 360–4320 px height, aspect ratios 1:1/4:3/16:9/21:9 и HUD scale 50–150%. Проверены nearest-grid bounds/anchors, идемпотентность snapping, согласованная phase radius/width, nested closed contours, finite points, orientation/area, transparency-band offsets, subpixel chord error и tiny/invalid inputs. Прежние layout/weapon/animation/text/contrast tests также проходят.

Единственный полный FAIL — прежний `Net.Ipv4AndIpv6Work`: timeout IPv6 loopback receive, wait=0 и recv=−1 вместо 3 в `src/test/net_test.cpp:48–49`. Он не исправлялся и не исключался из полного прогона; full workflow завершился failure после выполнения остальных suites. Полный suite не объявляется зелёным. Четыре relay integration tests пропущены по существующим fixture/environment requirements.

Логи в `cmake-build-release`: `lucent-hud-border-build.log`, `lucent-hud-border-focused.log`, `lucent-hud-border-full-tests.log`, `lucent-hud-border-filtered-tests.log`.

## Визуальные ограничения

Computer Use, запуск игры, screenshots и GUI verification не выполнялись согласно `AGENTS.md`. Установлены source-level причины, добавлено coverage сглаживание и проверена geometry; внешний вид на реальных ярких/тёмных картах, при движении alignment rows, разных DPI/MSAA/backends и с кастомными skins вручную не проверен. Mesh AA не является обещанием полного отсутствия любых ступеней в любом кадре. Очень низкая HUD opacity по-прежнему снижает различимость границ по выбору пользователя.
