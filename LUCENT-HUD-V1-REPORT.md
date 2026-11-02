# Lucent HUD v1

Дата: 2026-11-02. Ветка: `feat/game-ui`.

## Реализовано

- **Weapon hotbar:** пассивная панель в нижнем центре, симметрично расширяющаяся от одного до шести слотов. Используются существующие pickup textures и подготовленные HUD quad containers. Активное оружие выделено заливкой и акцентной полосой, остальные иконки приглушены. Никаких click handlers, переключения оружия или записи в weapon state нет.
- **Доступность оружия:** при наличии DDNet extended snapshot используется уже известный клиенту инвентарь; без полного инвентаря показывается только подтверждённое активное оружие. Неизвестное оружие не предполагается доступным. Выделение соответствует `m_RenderCur.m_Weapon`, как прежний strip. Ammo отображается только для соответствующего snapshot weapon; ninja duration перенесена в компактный progress indicator.
- **Timer:** крупное основное время в компактной верхней центральной панели. Расчёт основного времени оставлен в существующем `RenderGameTimer`, включая time limit, race time и round time. Finish/checkpoint и разница времени собраны в одну компактную строку ниже. Сохранены шестисекундное отображение, fade после четырёх секунд, цвета положительной/отрицательной разницы, настройка checkpoint и предупреждение последней минуты/мигание последних десяти секунд.
- **Recording:** единая верхняя правая плашка с `REC`/`AUTO` и длительностью активных manual/auto demo recorders, красной точкой; активные race/replay recorder states дополнительно обозначаются короткими метками. Когда ничего не записывается, плашка скрыта. Состояние recorders только читается. Учтены FPS/prediction readouts; при пересечении с таймером плашка опускается ниже. При открытом scoreboard новый блок скрывается, штатный scoreboard остаётся без изменений.
- **Coordinates:** горизонтальная строка `X 17.50   Y 96.53   ∠ 358.2` справа снизу. Используется прежний `GetMovementInformation`: те же координаты в блоках, угол и источники snapshot/spectator data. Freeview/spec-character отображает только доступные X/Y. Существующий speed display сохранён дополнительной компактной строкой Vx/Vy с прежней индикацией изменения скорости. Зарезервировано место под существующий score HUD и hotbar.
- **Utility/status zone:** health/armor, jump indicators, capability/modifier/status icons остаются слева сверху в более компактном масштабе. Старый оружейный strip и ammo projectiles там не рисуются при включённом Lucent HUD. Данные статусов и условия их доступности сохранены.
- **Соседние виджеты:** существующие pause/warmup/connection notifications учитывают увеличенную высоту таймера; включённые локальные часы располагаются слева от него. Dummy/spectator-count блоки учитывают высоту новой строки движения.
- **Модульность:** отдельные методы `RenderLucentTimer`, `RenderLucentHotbar`, `RenderLucentRecording`, `RenderLucentCoordinates`, `RenderLucentStatus`; общие helpers для anchors, panel drawing, spacing, typography, opacity и выбора известных слотов.
- **Совместимость:** `lucent_hud 0` возвращает legacy rendering. Существующие master HUD/video-HUD настройки сохранены. Новый HUD использует общую rendering/config систему клиента; изменений physics, networking, prediction, scoreboard и selection logic нет. ESC-меню на этом этапе не изменялось.

Чат не изменялся: необязательная косметическая правка отложена, чтобы не затрагивать кэширование и layout строк chat system.

## Новые config variables

Все переменные имеют `CFGFLAG_CLIENT | CFGFLAG_SAVE`, сохраняются штатным config manager в `settings_ddnet.cfg` при включённом сохранении настроек. Доступны через консоль/config; новые элементы ESC-меню не добавлялись.

| Переменная | Default | Диапазон | Назначение |
| --- | --- | --- | --- |
| `lucent_hud` | 1 | 0–1 | Включить HUD v1 / вернуть legacy HUD |
| `lucent_hud_scale` | 100 | 50–150 | Масштаб новых элементов, % |
| `lucent_hud_opacity` | 85 | 0–100 | Прозрачность новых элементов и status icons, % |
| `lucent_hud_coordinates` | 1 | 0–1 | Показ X/Y/угла |
| `lucent_hud_recording` | 1 | 0–1 | Показ блока активной записи |

Масштаб и прозрачность относятся к переработанным блокам Lucent; существующие score/FPS/прочие legacy widgets сохраняют собственное оформление и настройки. Показ скорости продолжает управляться `cl_showhud_player_speed`.

## Изменённые файлы этого этапа

| Файл | Изменение |
| --- | --- |
| `CMakeLists.txt` | Регистрация HUD sources/headers и нового теста |
| `src/game/client/components/hud.h` | Методы модулей, фиксированный текстовый кэш и bounds таймера |
| `src/game/client/components/hud.cpp` | Подключение новых модулей, legacy fallback, освобождение кэша, согласование соседних anchors |
| `src/game/client/lucent/config_variables.h` | Подключение HUD configs |
| `src/game/client/lucent/hud/config_variables.h` | Пять сохраняемых настроек |
| `src/game/client/lucent/hud/hud.cpp` | Timer/hotbar/recording/coordinates/status rendering |
| `src/game/client/lucent/hud/layout.h` | Независимые geometry/inventory helpers |
| `src/game/client/lucent/hud/visuals.h` | Общие panel/color/typography helpers |
| `src/test/lucent_hud_test.cpp` | Проверки inventory, layout и настоящего config save/load/reset |
| `LUCENT-HUD-V1-REPORT.md` | Этот отчёт |

Рабочая копия уже содержала изменения предыдущего этапа GameView и другие незакоммиченные изменения. Они сохранены; перечисление выше относится именно к HUD v1. PR не создавался.

## Производительность

- Inventory/слоты используют фиксированный `std::array` и битовую маску, форматирование — stack buffers.
- Иконки используют уже загруженные textures/quad containers, без загрузки ресурсов в рендере.
- Новые текстовые блоки имеют фиксированный кэш: неизменившийся текст не пересоздаётся; для изменившегося значения используется `RecreateTextContainerSoft` с повторным использованием character storage. Resize/смена font size корректно инвалидируют кэш.
- В новом коде нет per-frame `new`, `malloc`, динамических строк или растущих inventory containers. Стандартный text/graphics backend может выделять память при первом использовании, увеличении capacity или смене масштаба; runtime allocation profiling не выполнялся. Не заявляется полное отсутствие аллокаций внутри существующего renderer.

## Сборка и тесты

Сборка выполнена разрешённым скриптом:

```powershell
./scripts/build-lucent-vs.ps1 -Targets game-client,testrunner,lucent-browser-test,lucent-menu-test,lucent-radial-transition-test,lucent-settings-test,lucent-menu-effects-test
```

Результат: **PASS**, exit 0, MSVC x64, существующий `out/build/x64-Release/Lucent Client.exe`. Итоговый лог: `out/build/x64-Release/lucent-hud-build.log`, без compiler errors/warnings.

Сохранённая Visual Studio конфигурация называется `x64-Release`, но её build type в settings/cache — **RelWithDebInfo**. Использован именно этот профиль согласно AGENTS.md; отдельная конфигурация `Release` не создавалась и не подменялась.

| Проверка | Результат |
| --- | --- |
| `testrunner.exe --gtest_filter=LucentHud.*:LucentGameView.* --gtest_color=no` | PASS, 4/4 теста |
| `LucentHud.ConfigSurvivesFreshSessionsAndReset` | PASS: все пять настроек установлены console commands, сохранены и загружены свежим manager; проверены clamp и сохранение Reset |
| `LucentHud.InventoryUsesOnlyKnownOwnershipAndKeepsProtocolOrder` | PASS: все 64 inventory masks, валидные/невалидные active IDs, fallback без известного inventory |
| `LucentHud.AnchorsRemainSymmetricAndSeparateAtSupportedResolutions` | PASS: 1920×1080, 2560×1440, дополнительно 1280×960; scale 50/75/100/125/150%; 1–6 slots, symmetry/bounds/gaps, coordinates vs score/hotbar, recording vs timer |
| `LucentGameView.ConfigSurvivesFreshSessionsAndReset` | PASS, регрессия предыдущего этапа |
| `lucent-menu-test.exe` | PASS |
| `lucent-browser-test.exe` | PASS |
| `lucent-menu-effects-test.exe` | PASS |
| `lucent-radial-transition-test.exe` | PASS |
| `lucent-settings-test.exe .` | PASS |
| `git diff --check` | PASS |

Профильный XML: `out/build/x64-Release/lucent-hud-tests.xml`. Config tests используют изолированное test storage и не перезаписывают пользовательский config. Полный общий `testrunner` на этом этапе не запускался.

## Не проверено визуально вручную

Computer Use, screenshot capture и GUI automation не использовались. Визуальная проверка в игре не выполнена. Пользователю остаётся проверить реальное восприятие отступов/типографики на 1920×1080 и 2560×1440, загрузку иконок/глифа угла из выбранных skins/fonts, динамику переключения оружия и ninja duration, live checkpoint/finish, manual/auto recording, spectator/freeview и сочетания пользовательских HUD settings. Геометрические тесты не заменяют проверку фактического кадра.

Live-подключения к серверам 0.6/0.7, video capture и runtime heap profiling не выполнялись. Совместимость опирается на существующие snapshot/rendering/config APIs и консервативный fallback при неизвестном inventory.
