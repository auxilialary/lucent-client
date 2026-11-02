# Lucent Tee Alignment v1.1 — Visual Redesign

Ветка `feat/game-ui`. PR не создавался. Предыдущие незакоммиченные изменения проекта сохранены.

## Визуальное представление

- Настоящие tee отрисовываются через `RenderTools()->RenderIdleTeeAtCenter()`, как в scoreboard. Копируется `m_aClients[ClientId].m_RenderInfo`, включая skin textures, custom colors и sixup render info; штатный renderer выбирает 0.6/0.7. Размер tee — 13 HUD units, круглая подложка — 14, capsule height — 18. Clip/scissor вокруг tee не устанавливается, прямоугольных рамок нет.
- Цвет capsule — **#292638**, alpha умножается на общую прозрачность Lucent HUD и анимацию. Полное pill-скругление: radius = height / 2.
- Круг под tee — **#FFFFFF0A** поверх capsule. Текст — **#F2EDF9**. Snowflake — **#9ADFFF**.
- Слева padding 2 HUD units, текст начинается на 19; справа зарезервированы 11 units для отступа и snowflake. Место под accent резервируется постоянно: смена freeze не меняет ширину или положение подписи.
- Ширина определяется измеренным текстом + 30 units служебного пространства; границы — 40–112 HUD units с дополнительным ограничением шириной экрана минус 12 units. Фиксированные широкие контейнеры убраны.
- Dummy-режим подписан `Dummy` либо `Main`; Players использует настоящий `m_aName`. `Player ID` удалён. Длинные подписи измеряются с текущим renderer/font size и сокращаются с `…` по границе UTF-8 codepoint. Кириллица, CJK, emoji и combining sequences проверены на валидность UTF-8; алгоритм не обещает сохранение целого Unicode grapheme cluster при усечении.
- Порядок вертикальный: Dummy/Main сверху, Players ниже. Между строками 2.5 HUD units. Каждая capsule центрирована по экрану; одна строка располагается непосредственно над нижней HUD-группой без пустого placeholder.
- В crossfade исходящий слой сохраняет прежние имя и `CTeeRenderInfo`, входящий использует новую цель. Нет масштабной анимации tee или текста.

## Анимации и размещение

- Существующие reveal/fade и freeze accent сохранены: 200 мс cubic ease-out, slide до 5 физических пикселей, fade-out 165 мс, отдельный freeze accent с 80 мс сглаживанием. Frame delta берётся из `RenderFrameTime`.
- Перемещение соседней строки — конечная 200 мс ease-out интерполяция. При добавлении нижней строки сначала освобождается место движением существующей верхней, затем начинается reveal нижней: нет наложения двух капсул во время расширения stack. После fade-out исчезнувшей строки оставшаяся перемещается к одиночному anchor.
- Позиция учитывает фактические сохранённые bounds hotbar, jumps/modifiers и coordinates, включая полное пространство для slide. При росте внешнего препятствия вверх действует немедленное ограничение безопасной позиции; безопасность имеет приоритет над плавным движением.
- Chat не предоставляет публичных bounds реально нарисованных строк. Чтобы не изменять chat component, overlay резервирует **консервативную область** по исходным пределам чата: верхняя граница 200 либо 50 при expanded, ширина `5 + cl_chat_width`, вся ширина при активном input. Это может оставлять больше свободного места, чем реально заняли сообщения, но предотвращает пересечения. Существующие chat/HUD элементы не перемещаются.
- Если expanded/wide chat и верхний timer не оставляют безопасной области, соответствующая capsule не рисуется; обнаружение продолжает работать. Верхний timer также проверяется перед отрисовкой. Это ограничение presentation, не изменение X-alignment.
- Геометрия проверена для aspect ratios 1:1, 4:3, 16:9, 21:9 и HUD scale 50/100/150%. При слишком тесной компоновке применяется описанное подавление отображения.

## Качество текста: исследование исходников

- Старый overlay использовал общий `LucentText`: font size 5, inherited render flags, outline alpha 0.30. Дополнительного shadow pass именно у alignment не было. Нативный `RenderTextContainer(..., X, Y)` уже округлял перенос по physical pixels: дробные HUD coordinates сами по себе не доказывают причину размытия.
- Text renderer использует FreeType grayscale glyph rasterization с `FT_LOAD_RENDER | FT_LOAD_NO_BITMAP`, atlas по целевому pixel size. Размер cursor переводится в physical pixels через текущую screen mapping; native code использует целочисленный `ActualSize`. Признаков принудительного уменьшения atlas для overlay не найдено. На небольших glyph outline способен заметно утяжелять края; изменение одной alpha не решает недостаточный размер подписи.
- Overlay получил отдельные prepare/draw методы. Font size увеличен до **6.5 HUD units**, целевой размер приводится через `TargetFontSize` к целому physical glyph size: при 100% примерно 23 px на 1080p и 31 px на 1440p вместо прежних 18/24.
- Явно включены native pixel alignment и kerning, задан положительный cursor line width. Outline = 0, shadow pass отсутствует, текст выводится одним fill-only проходом на тёмной capsule. Цвет не запекается в cache.
- Text containers пересоздаются при изменении строки, font size или physical screen height. Они не масштабируются для fade/slide/reflow. Существующий resize/reset удаляет эти же cached containers.
- Исходные скриншоты к текущему сообщению не приложены, GUI запрещён AGENTS.md. Поэтому это проверенное по исходникам улучшение text path, **не подтверждение устранения артефактов на реальном кадре**. FreeType/native atlas не заменялся и не уменьшался.

## Сохранённая логика и область изменений

- Snapshot adapter, условия spectator/disconnect, единый интерполированный временной слой и консервативная freeze detection не изменены. До/после сопоставлен SHA-256 блока adapter: совпадение подтверждено (`out/build/x64-Release/tee-alignment-v1.1-detection-hash.json`).
- `Select`, `Frozen`, вход X <= 2 world units / удержание <= 3 и обе независимые config variables сохранены. Новых настроек нет.
- Изменены только:
  - `src/game/client/lucent/hud/tee_alignment.cpp` — visual renderer, имена, snapshot tee info, текст и stack;
  - `src/game/client/lucent/hud/tee_alignment.h` — UTF-8 fit helper и состояние reflow;
  - `src/game/client/components/hud.h` — фиксированные visual caches и объявления специальных text методов;
  - `src/test/lucent_tee_alignment_test.cpp` — три дополнительных теста;
  - `LUCENT-TEE-ALIGNMENT-V1.1-REPORT.md` — этот отчёт.
- Нет новых контейнеров с динамическим ростом в per-frame layout/animation, строки и visual layers имеют фиксированную ёмкость. Glyph/text cache может выделять память при первом появлении символов, смене подписи/разрешения по штатному renderer path. Runtime heap profiling не выполнялся.
- Chat, scoreboard, другие HUD render methods, ESC, gameplay, physics, prediction, networking и config headers этой задачей не менялись. HUD Editor отсутствует.

## Проверки

- `./scripts/build-lucent-vs.ps1 -Targets game-client,testrunner,lucent-browser-test,lucent-menu-test,lucent-radial-transition-test,lucent-settings-test,lucent-menu-effects-test` — **PASS**.
- Использован сохранённый Visual Studio selection `x64-Release`, существующий tree `out/build/x64-Release`, MSVC x64. Реальный CMake build type selection — `RelWithDebInfo`. Альтернативные деревья не создавались.
- Логи: `out/build/x64-Release/lucent-tee-alignment-v1.1-build.log`, `lucent-tee-alignment-v1.1-build-final.log`.
- `testrunner --gtest_filter=LucentTeeAlignment.*:LucentHud.*:LucentGameView.*` — **25/25 PASS**, включая 8 alignment tests. XML: `out/build/x64-Release/lucent-tee-alignment-v1.1-tests.xml`.
- Новые tests проверяют усечение UTF-8 с учётом ellipsis width, конечную плавную reflow анимацию на 30/60/144 FPS, ограничения нижних HUD/chat областей на разных aspect ratios/scales и больших наборах status items. Прежние selection/hysteresis/freeze/config tests проходят.
- `lucent-browser-test`, `lucent-menu-test`, `lucent-radial-transition-test`, `lucent-settings-test .`, `lucent-menu-effects-test` — **PASS**.
- `git diff --check` — **PASS**.
- GUI/screenshot/interaction visual verification не выполнялась: Computer Use запрещён. Не проверены вручную яркие карты, реальные кастомные skins, runtime skin reload во время crossfade и живые сессии серверов 0.6/0.7. Headless tests подтверждают source logic/geometry, а не фактическую чёткость изображения.
