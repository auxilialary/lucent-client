# Lucent Tee Alignment Overlay

Ветка: `feat/game-ui`. PR не создавался. Существующие незакоммиченные изменения других задач сохранены.

## Реализация

- Изолированный HUD-модуль `lucent/hud/tee_alignment.{h,cpp}`. Существующие HUD-панели не перемещаются и не меняют оформление.
- Независимые сохраняемые client config variables, по умолчанию выключены:
  - `lucent_tee_alignment_dummy 0|1`;
  - `lucent_tee_alignment_players 0|1`.
- Для включения обоих режимов: `lucent_tee_alignment_dummy 1` и `lucent_tee_alignment_players 1`. Overlay подчиняется общему включению, масштабу, прозрачности и видимости Lucent HUD. При scoreboard, spectator, demo playback, отсутствии локального живого персонажа или несовпадении выбранного connection/local ID он скрывается.
- Dummy-режим сравнивает управляемый tee с другим локальным tee. При смене управления состояние сбрасывается; обозначение цели — `Main` либо `Dummy`. Отключённый dummy не используется как цель.
- Players исключает управляемого tee и другой подключённый локальный tee. Выбирается ближайший по положительной разнице мировой Y. При равной Y выбор детерминирован: меньший client ID. Короткая подпись — `Player ID`, без потенциально длинного имени.
- При одновременном показе порядок слева направо: Dummy/Main, Players. Одиночная капсула центрируется, две центрируются как группа.

## Данные, X tolerance и freeze

- Вход в alignment: `abs(targetX - selfX) <= 2.0` мировых единиц. Для текущей цели удержание до `3.0` включительно. Выше 3.0 совпадение прекращается. Y цели должна быть строго больше Y управляемого tee.
- Используется одна пара `m_Snap.m_aCharacters[].m_Prev/m_Cur` и один `IntraGameTick` выбранного соединения для всех сравниваемых персонажей. Намеренно не смешиваются predicted local `RenderPos` и непредсказанные remote позиции. Возможна естественная задержка snapshot-интерполяции относительно предсказанного локального рендера.
- `m_Active` character устанавливается клиентом только при наличии current и previous character snapshot; дополнительно проверяется активность клиента. Новые/исчезнувшие персонажи без достоверной пары не выбираются. Используется общий переводимый snapshot-слой клиента для 0.6/0.7, без новых протокольных сообщений.
- Нет проверки скорости, столкновений, карты, camera bounds или видимости на экране. Вне камеры обнаружение возможно только для персонажей, о которых сервер действительно прислал необходимые данные.
- Снежинка подтверждается исключительно extended server data: обычный freeze — `FreezeEnd > соответствующий GameTick`; deep freeze — `FreezeEnd == -1`; live freeze — `CHARACTERFLAG_MOVEMENTS_DISABLED`. Подтверждение требуется в обоих концах интерполируемого интервала; переходы показываются консервативно. При отсутствии current/previous extended data freeze считается неизвестным и не выводится как подтверждённый. Неподвижность, skin, оружие и неизвестные значения не используются для вывода о freeze.
- После потери цели только прежний визуальный слой затухает; он не участвует в новом обнаружении как достоверный объект. Исчезновение snowflake accent тоже плавное.

## Визуал и анимация

- Капсула 48 × 12 HUD units, зазор 3, текст 5, существующая палитра/радиус/прозрачность Lucent. Компактная шестилучевая снежинка рисуется фиксированным массивом линий.
- Нижний anchor рассчитывается по фактическим bounds hotbar, общей группы jumps/modifiers и пересекающейся по X coordinates-панели. Зарезервирован полный slide, существующие HUD-блоки не сдвигаются.
- Появление 200 мс: cubic ease-out opacity и вертикальное смещение от 5 физических пикселей к нулю. Fade-out полного слоя — 165 мс. Смена цели — перекрывающиеся входящий/исходящий слои. Изменение freeze меняет только accent с экспоненциальным сглаживанием 80 мс.
- Используется `RenderFrameTime`, без бесконечной пульсации и масштабирования текста/готовых растровых текстур. Два фиксированных visual layers на режим; массив кандидатов на стеке. Нет новых регулярных heap allocations в обходе кандидатов, selection, geometry или animation. Текстовые контейнеры используют существующий cache; создание/изменение подписи может задействовать штатные выделения renderer при появлении/смене цели.

## Файлы этой задачи

- `CMakeLists.txt` — регистрация модуля и профильных тестов.
- `src/game/client/components/hud.h` — состояние, text cache slots, hotbar bounds и метод overlay.
- `src/game/client/components/hud.cpp` — reset и вызов overlay после расчёта нижних HUD bounds.
- `src/game/client/lucent/hud/hud.cpp` — сохранение фактического hotbar bounds, без изменения отрисовки оружия.
- `src/game/client/lucent/hud/config_variables.h` — две сохраняемые настройки.
- `src/game/client/lucent/hud/tee_alignment.h` — selection/hysteresis, fixed animation state и размещение.
- `src/game/client/lucent/hud/tee_alignment.cpp` — read-only snapshot adapter и отрисовка капсул.
- `src/test/lucent_tee_alignment_test.cpp` — пять профильных тестов.
- `LUCENT-TEE-ALIGNMENT-REPORT.md` — этот отчёт.

Physics, networking, prediction, player rendering, weapon behavior, chat, ESC и scoreboard implementation не изменялись этой задачей. Нового HUD Editor нет.

## Проверки

- CLI build через `./scripts/build-lucent-vs.ps1 -Targets game-client,testrunner,lucent-browser-test,lucent-menu-test,lucent-radial-transition-test,lucent-settings-test,lucent-menu-effects-test` — PASS.
- Использован сохранённый Visual Studio selection `x64-Release`, существующий `out/build/x64-Release`, MSVC x64. Реальный CMake build type этого selection — `RelWithDebInfo`; альтернативные build trees не создавались.
- Финальная сборка клиента после уточнения подписи `Main` — PASS. Логи: `out/build/x64-Release/lucent-tee-alignment-build.log` и `lucent-tee-alignment-build-final.log`.
- `testrunner --gtest_filter=LucentTeeAlignment.*:LucentHud.*:LucentGameView.*` — **22/22 PASS**. XML: `out/build/x64-Release/lucent-tee-alignment-tests.xml`.
- Покрыты смена управляемого tee, независимые режимы, исключения, невалидные цели, nearest-Y, world-X tolerance/hysteresis, отсутствие camera clipping, известный/неизвестный freeze, crossfade/fade и freeze-only accent при 30/60/144 FPS, clearance с учётом slide на 1080p/1440p и масштабе 50/100/150%.
- Существующий `LucentHud.ConfigSurvivesFreshSessionsAndReset` автоматически перебирает весь HUD config header: обе новые настройки установлены через console, сохранены, загружены в свежей config session и сброшены — PASS.
- `lucent-browser-test`, `lucent-menu-test`, `lucent-radial-transition-test`, `lucent-settings-test .`, `lucent-menu-effects-test` — PASS.
- `git diff --check` — PASS.

GUI/ручная визуальная проверка **не выполнялась**: Computer Use запрещён AGENTS.md. Не проверялись реальный кадр на яркой карте, сетевые игровые сессии 0.6/0.7, live dummy reconnect и runtime heap profiling. Совместимость оценена по существующим source contracts и CLI tests; эти проверки не заменяют ручную игровую проверку.
