# Lucent HUD v1.5 — Visual Consistency & Timer Quality

Дата: 2026-11-02. Ветка: **`feat/game-ui`**. PR не создавался.

Использованы текущие HUD methods, layout helpers, text cache, HUD textures и quad containers. Новых config variables, HUD Editor, внешних библиотек или media-зависимостей нет. Gameplay, чат, ESC и scoreboard не перерабатывались.

## Jump indicators и modifiers: два контейнера

- Прыжки и active modifiers/capabilities получили отдельные фоны. Существующие условия доступности, textures и порядок состояний сохранены.
- В обычном наборе контейнеры располагаются горизонтально над hotbar с зазором **3 scaled HUD units**. Центрируется **вся пара**, включая зазор, а не каждый фон относительно середины экрана.
- Пустой контейнер не рисуется и не резервирует зазор. Если остались только прыжки или только modifiers, видимый контейнер центрируется самостоятельно.
- Сохранены иконки для небольшого количества прыжков и numeric `available/total (used)` для большого; при неизвестном остатке — `?/total`. Подсчёт прыжков не менялся.
- Modifiers переносятся внутри своего контейнера. Слишком узкая колонка не используется: если места для разумного горизонтального набора недостаточно, контейнеры переносятся в две строки с тем же зазором 3. При таком переносе они выровнены в общей группе, а не получают независимые screen-center anchors.
- Длинные наборы остаются в ограниченной области экрана с прежним chat clearance и информационной опорой. Coordinates продолжают учитывать общий status bounds. Hotbar не сдвигается.
- Размер иконки **8**, внутренний gap **2**, padding **2**, высота строки **12 HUD units** сохранены. Используются фиксированные stack arrays, без growing containers в render loop.

Для запрошенного примера при HUD scale 100 и обычной ширине экрана:

| Набор | Jump container | Modifier container | Зазор | Общая ширина |
| --- | --- | --- | --- | --- |
| 2 прыжка + 4 modifiers | 22 | 42 | 3 | **67** |
| 2 прыжка + 5 modifiers | 22 | 52 | 3 | **77** |
| 2 прыжка + 6 modifiers | 22 | 62 | 3 | **87** |

Это результаты проверки геометрии, не оценки реального кадра.

## Скругления и общая визуальная система

- Общий номинальный radius: **2 → 5.5 HUD units**.
- Реальный radius ограничен `min(5.5 × scale, height/2, width/2)`. Поэтому низкие/узкие панели не получают некорректной геометрии и не увеличиваются ради скругления.
- Примеры при scale 100: jump/modifier row высотой 12 — radius **5.5**; coordinates высотой 8 — **4**; двухстрочные coordinates — **5.5**; hotbar с его сохранённым множителем 0.85 — **4.675**.
- Active-slot fill и его тонкий контур используют один и тот же ограниченный radius. Толщина контура и размер оружия не увеличены.
- Все плашки используют общий фон RGB **(0.045, 0.035, 0.065)** и alpha **`0.58 × lucent_hud_opacity/100`**. У coordinates удалён дополнительный множитель 0.70, чтобы согласовать фон с остальными модулями.
- Основной текст **(0.96, 0.96, 0.99)**, вторичный **(0.80, 0.78, 0.85)**, акцент **(0.70, 0.58, 0.92)** сохранены. Сигнальные gameplay RGB-цвета не менялись.
- У surfaces нет новых массивных теней или толстых рамок. Размеры hotbar, его слотов и спрайтов, наклон `pi * 7 / 4`, позиция и inventory behavior сохранены.

Общий radius применяется и к recording-плашке через существующий helper. Её поведение, состояния, условия развёрнутого чата и форматы не менялись.

## Timer: качество текста

Исследован существующий `src/engine/client/text.cpp`:

- `AppendTextContainer` выбирает glyph size из `font size × ScreenSize/virtual screen size` и переводит его в целые пиксели.
- Glyph atlas получает FreeType glyph для этого размера; renderer поддерживает диапазон **6–128 px**.
- `RenderTextContainer` выполняет pixel alignment, если в контейнер не попал `NO_PIXEL_ALIGNMENT`.
- Outline atlas имеет размерозависимую толщину; сильная outline opacity на крупном тексте усиливает визуальную тяжесть края.

Доработан только локальный HUD text path, engine text renderer не переписывался:

1. Основное время **13 → 12.5 HUD units**. Race/game time остаётся сверху, local time — снизу; checkpoint/finish остаётся между ними при наличии события. Все строки центрированы.
2. Для timer/main/secondary/clock вычисляется **целевой целочисленный пиксельный размер** с учётом разрешения и HUD scale. Размер передаётся в `CTextCursor` для создания glyph geometry, а не применяется масштабированием готовой картинки времени.
3. Запрос ограничен существующим диапазоном atlas 6–128 px, чтобы не растягивать glyph выше поддерживаемого размера. При 1920×1080 и 2560×1440 весь заданный диапазон HUD scale помещается в него.
4. Измерение и создание timer text используют одинаковые явные flags: **ONE_TIME_USE | KERNING**, без наследования `NO_PIXEL_ALIGNMENT` и UI-specific bearing/oversize flags. Внешние flags восстанавливаются.
5. Кэш инвалидируется при смене размера или pixel height. Неподвижный текст не пересоздаётся каждый кадр; изменившиеся значения используют существующее soft recreation.
6. Timer outline alpha: **0.30 → 0.10**. Shadow alpha: **0.36 → 0.24**. Смещение тени теперь **ровно один физический пиксель по X/Y**, вместо логического offset, который рос до нескольких пикселей при большом HUD scale.
7. Внешний timer inset **2 → 3 HUD units**, чтобы текст согласовывался с заметным radius. Width увеличивается только на дополнительные 2 units padding; main-only/main+clock/full height — **18.5 / 25.5 / 34.5** units. Другие контейнеры ради скруглений не увеличены.

Основной шрифт при scale 100 запрашивается как **45 px на 1080p** и **60 px на 1440p**. Это корректное создание текста в целевом размере. Новых bitmap-панелей, supersampling-композитов или масштабирования готовой растровой текстуры таймера нет.

Time formats, checkpoint/finish conditions, fade timings, last-minute/blink и сигнальные цвета сохранены. Отсутствие видимой пикселизации не заявляется: результат зависит от выбранного шрифта, backend и реального отображения и требует просмотра кадра.

## Сохранение поведения

Сверены контрольные хеши до/после: **`RenderLucentHotbar`**, **`RenderLucentRecording`**, `chat.h`, основной `components/hud.cpp` и HUD config declarations не изменились. Новые размерные/цветовые параметры локальны presentation layer. Существующие anchors и автоматическое clearance продолжают применяться; дополнительные строки статусов растут вверх, не передвигая hotbar.

## Файлы этого этапа

- `src/game/client/lucent/hud/layout.h` — два status groups, wrap/group centering, radius clamp и target-pixel font helper.
- `src/game/client/lucent/hud/visuals.h` — общие заметные скругления, включая active-slot contour.
- `src/game/client/lucent/hud/hud.cpp` — отдельные status backgrounds, timer text quality и согласование coordinates surface.
- `src/game/client/components/hud.h` — pixel-height key существующего text cache.
- `src/test/lucent_hud_test.cpp` — проверки нового grouped layout, округлений и target-size текста.
- `LUCENT-HUD-V1.5-REPORT.md` — этот отчёт.

Предыдущие рабочие изменения сохранены.

## Сборка и проверки

```powershell
./scripts/build-lucent-vs.ps1 -Targets game-client,testrunner,lucent-browser-test,lucent-menu-test,lucent-radial-transition-test,lucent-settings-test,lucent-menu-effects-test
```

**PASS**, exit 0, MSVC x64. Использован существующий Visual Studio профиль **x64-Release** с фактическим build type **RelWithDebInfo**. Другой build tree/preset не создавался.

| Проверка | Результат |
| --- | --- |
| `testrunner.exe --gtest_filter=LucentHud.*:LucentGameView.* --gtest_color=no` | **PASS, 15/15** |
| 2 jump icons + 4–6 modifiers | **PASS:** отдельные bounds, gap 3, общий центр, пустые группы отсутствуют |
| Grouped status wrap | **PASS:** 1–32 элемента, numeric jump group, разные aspect ratios/chat widths/scales; containment и coordinates clearance |
| Rounded panel geometry | **PASS:** radius не превышает половины width/height, включая короткие и пустые bounds |
| Target-pixel timer text | **PASS:** 1080/1440/2160 px height, HUD scale 50/75/100/125/150%, integer glyph size и межстрочное пространство |
| Existing HUD/weapon/config regressions | **PASS**, включая неизменённую геометрию hotbar |
| `lucent-menu-test.exe` | **PASS** |
| `lucent-browser-test.exe` | **PASS** |
| `lucent-menu-effects-test.exe` | **PASS** |
| `lucent-radial-transition-test.exe` | **PASS** |
| `lucent-settings-test.exe .` | **PASS** |
| `git diff --check` | **PASS** |

Артефакты: `out/build/x64-Release/Lucent Client.exe`, `lucent-hud-v1.5-build.log`, `lucent-hud-v1.5-tests.xml`, `lucent-hud-v1.5-preserved.json`. Полный общий набор тестов не запускался.

## Визуальная проверка

**GUI-проверка недоступна в рамках правил проекта:** AGENTS.md запрещает Computer Use, GUI automation и screenshot capture. Эти инструменты не использовались, игра для просмотра не запускалась.

Реальный кадр таймера на светлой карте, восприятие antialiasing/outline/shadow и группа с 2 прыжками и 4–6 modifiers **визуально не проверялись**. Для этой группы проверены только расчётные bounds и центрирование. Улучшение render path выполнено по исходникам и автоматическим проверкам; окончательная оценка изображения остаётся пользователю.
