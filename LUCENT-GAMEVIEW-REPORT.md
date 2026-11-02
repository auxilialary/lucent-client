# Lucent GameView — отчёт

Дата: 2026-11-02. Ветка: `feat/game-ui`, создана от открытой `feat/settings-ui` по уточнению пользователя. Изначальные незакоммиченные изменения сохранены. Ветка `dev/lucent-core` не переключалась. PR не создавался.

## Реализовано

- ESC-меню онлайн: вертикальная навигация, полупрозрачная панель содержимого и навигации, единые отступы. Кнопки используют существующие Lucent hover/active состояния и адаптацию размера текста. Высота навигационных строк ограничивается доступной высотой экрана.
- Сохранены переходы Game, Players, Server info, Browser, Ghost (для race), Call vote, Demos, Settings; действия Editor и Quit с прежними проверками подтверждения. Добавлены GameView и Back. Внутренние реализации существующих страниц сохранены; Settings продолжает использовать собственную полноэкранную оболочку.
- GameView: Cursor, Weapons, Camera, Tuning; отдельная прокрутка для каждой категории, штатные UI coordinates/clipping/scaling.
- Cursor: `lucent_cursor_size`, 5–500%, default 100%, Reset и предпросмотр. Масштаб применяется в `CHud::RenderCursor` через множитель существующего quad container, немедленно, включая доступный spectator cursor.
- Weapons: независимые `lucent_weapon_{hammer,gun,shotgun,grenade,laser,ninja}_{x,y}`; диапазон −100…100 world units, default 0. Предпросмотр с обозначением исходной точки, Reset выбранного оружия. Смещение по мировым X/Y применяется к изображению локального оружия, прикреплённым рукам и muzzle artwork. Положение персонажа, эффекты, реальные снаряды, хитбоксы и физика не меняются. Изображения оружия других игроков не сдвигаются.
- Camera: существующие конфиги zoom/smooth zoom, dynamic camera, обычные и dynamic deadzone/follow factor/min/max distance, dynamic sensitivity/smoothness/stabilizing, smooth spectating, spectator camera sync и ограничения zoom. Новых конфигов камеры нет. Изменение default zoom вызывает штатный `SetZoom`; переключатель dynamic camera согласован с поведением General.
- Tuning: активный `m_aTuning` выбранного main/dummy, локальная tune zone, сообщения о недоступной зоне и ожидании tuning. Только чтение. Отображается таблица эффективных клиентских значений, включая protocol defaults для полей, не переданных сервером; это явно указано в UI. Удалённый из 0.7 `laser_damage` скрыт на sixup. Обработка tuning/network messages не менялась.
- Все 13 новых переменных имеют `CFGFLAG_CLIENT | CFGFLAG_SAVE` и сохраняются штатным config manager в `settings_ddnet.cfg` при включённом сохранении настроек.
- Для дальнейших страниц вынесены `LucentUi::PanelRow` и `LucentUi::IngamePanel`; повторно используются существующие Lucent buttons/scrollbars. Внешних UI-библиотек нет.
- В дополнениях к горячим путям HUD/player rendering используются скаляры и фиксированные stack arrays; динамических контейнеров и новых heap allocations не добавлено. Проверено по исходникам, без runtime allocation profiling.

## Файлы этой задачи

| Файл | Изменение |
| --- | --- |
| `CMakeLists.txt` | Регистрация новых headers и интеграционного теста |
| `src/game/client/components/menus.h` | GameView page ID и методы рендера |
| `src/game/client/components/menus.cpp` | Вертикальный онлайн-каркас и dispatch GameView |
| `src/game/client/components/menus_ingame.cpp` | Навигация и четыре категории GameView |
| `src/game/client/components/hud.cpp` | Множитель размера игрового прицела |
| `src/game/client/components/players.cpp` | Исключительно визуальные смещения локального оружия |
| `src/game/client/lucent/config_variables.h` | Подключение настроек GameView |
| `src/game/client/lucent/gameview/config_variables.h` | 13 сохраняемых настроек с префиксом `lucent_` |
| `src/game/client/lucent/gameview/presentation.h` | Общие функции масштаба и выбора смещения |
| `src/game/client/lucent/ui/panel.h` | Переиспользуемые panel/row helpers |
| `src/game/client/lucent/tests/menu_interaction_test.cc` | Границы масштаба, mapping шести оружий, defaults/save flags/prefix |
| `src/test/lucent_gameview_test.cpp` | Реальные console/config save/load/reset между свежими сессиями |
| `LUCENT-GAMEVIEW-REPORT.md` | Этот отчёт |

Другие файлы, уже изменённые до начала задачи, не являются результатом GameView. Scoreboard, networking, prediction и input-архитектура этой задачей не изменялись.

## Сборка

Использован только разрешённый workflow:

```powershell
./scripts/build-lucent-vs.ps1 -Targets game-client,testrunner,lucent-browser-test,lucent-menu-test,lucent-radial-transition-test,lucent-settings-test,lucent-menu-effects-test
```

Результат: exit 0, MSVC x64, существующий `out/build/x64-Release`, создан `out/build/x64-Release/Lucent Client.exe`.

Важное уточнение: сохранённый Visual Studio профиль называется **x64-Release**, но в `CMakeSettings.json` и существующем cache он имеет build type **RelWithDebInfo**. Собран именно этот оптимизированный профиль. Отдельная чистая конфигурация `Release` не создавалась и не подменялась, согласно AGENTS.md.

Лог: `out/build/x64-Release/lucent-gameview-build.log`.

## Проверки

| Проверка | Результат |
| --- | --- |
| `lucent-menu-test.exe` | PASS; включает новые GameView проверки |
| `lucent-browser-test.exe` | PASS |
| `lucent-menu-effects-test.exe` | PASS |
| `lucent-radial-transition-test.exe` | PASS |
| `lucent-settings-test.exe .` | PASS |
| `testrunner.exe --gtest_filter=LucentGameView.* --gtest_color=no` | PASS, 1 тест |
| Полный `testrunner.exe` | 463 выполнено: 458 PASS, 4 SKIP, 1 FAIL; ещё 3 теста disabled |
| `git diff --check` | PASS |
| `python scripts/check_config_variables.py` | FAIL: существующая неиспользуемая `cl_show_start_menu_images` |

Новый интеграционный тест устанавливает все 13 переменных через зарегистрированные console commands, сохраняет реальный config в изолированном test storage, создаёт новый config manager, загружает значения, проверяет clamp для cursor/weapon offsets, выполняет Reset и ещё раз сохраняет/загружает defaults. Пользовательский config не используется.

Единственный FAIL полного набора: `Net.Ipv4AndIpv6Work`, `src/test/net_test.cpp:48–49`: ожидание IPv6 loopback UDP-пакета закончилось timeout 10s (`read_wait=0`, `recv=-1`). Причина не устанавливалась; сетевой код и тест не изменялись. Пропущены четыре opt-in теста LucentPingRelay. Полный набор не считается полностью зелёным.

Линтер сообщает только `cl_show_start_menu_images`. Через `git grep ... HEAD` подтверждено, что переменная уже была объявлена без использования до этой задачи. Несвязанный конфиг не удалялся.

Подробные результаты полного набора: `out/build/x64-Release/lucent-gameview-tests.log` и `lucent-gameview-tests.xml`.

## Не выполнено вручную

Computer Use и GUI automation не использовались. Игра для визуальной проверки не запускалась. Проверка внешнего вида, hover/active, прокрутки и крайних значений в живой игре на разных разрешениях/DPI, а также подключения к серверам 0.6/0.7 остаётся пользователю. Совместимость основана на общей существующей rendering/config инфраструктуре и учёте отличия tuning 0.7; live interoperability не заявляется как проверенная. Сохранение после перезапуска проверено свежими config-сессиями, без GUI-перезапуска игры.
