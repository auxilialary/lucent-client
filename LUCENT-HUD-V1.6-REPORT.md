# Lucent HUD v1.6 — Timer Redesign

Дата: 2026-11-02. Ветка: **`feat/game-ui`**. PR не создавался.

Переработан только таймер. Остальные HUD-модули, настройки, gameplay, networking, physics, ESC и scoreboard не изменялись. HUD Editor и media-зависимостей нет.

## Новое оформление

- Основное время: **12.5 → 9 HUD units**. Локальные часы остаются компактными, **5 units**, вторичным цветом.
- Порядок строк сохранён: race/game time сверху, checkpoint/finish между строками при наличии события, local time снизу. Все видимые строки центрированы относительно одного screen-center anchor.
- Контейнер — небольшая тёмная полупрозрачная капсула. RGB **(0.045, 0.035, 0.065)**, alpha **`0.66 × lucent_hud_opacity/100`**; при default opacity 85% — **0.561**. Фон обеспечивает контраст без дополнительных чёрных копий цифр.
- Radius капсулы: **половина меньшей стороны**. Форма меняется вместе с числом строк и длиной формата, не превращаясь в широкую фиксированную панель.
- Padding по X **6**, по Y **3 HUD units**. Gap между game time и checkpoint/finish **3**, перед local time **2**.
- Высота main-only **15**, main + clock **22**, всех трёх строк **31 HUD units** при HUD scale 100. Ширина определяется форматом самого длинного видимого текста и padding.
- **У timer text нет отдельной тени и outline.** HUD отправляет одно заполнение glyph geometry. Устранены повторная смещённая копия текста и наложение outline mask на края. Это изменение compositing path, а не только уменьшение alpha.

Все time formats, checkpoint/finish conditions, шестисекундный срок показа и fade, last-minute warning, blink, show/hide configs и сигнальные RGB-цвета сохранены. Отображаемые строки времени не заменяются шаблонами и не получают искусственных нулей.

## Исследование причин грубых краёв и нестабильности

Исследованы `src/engine/client/text.cpp`, `src/engine/textrender.h`, OpenGL/Vulkan text shaders, shipped font index и `DejaVuSans.ttf`. Общий engine text renderer, shaders и font assets не редактировались.

### Повторное заполнение и outline

В v1.5 timer text подавался в renderer дважды: смещённое чёрное заполнение для shadow, затем основное заполнение с outline. На anti-aliased краях это создаёт несколько наложенных силуэтов. Pixel alignment сам по себе не устраняет такой compositing.

Outline atlas дополнительно растёт в зависимости от размера шрифта: существующий renderer увеличивает толщину outline для крупных glyph sizes, в том числе в четыре раза при размере больше 48 px. Поэтому даже небольшая outline alpha не равнозначна тонкой геометрической линии.

В v1.6 у main/secondary/clock убран дополнительный shadow submission, outline alpha равна нулю. Нативная fill coverage сохраняется. В buffered backend shader получает прозрачный outline; в fallback backend прозрачный outline pass не добавляет видимого контура. Не заявляется, что все backend draw calls стали одним GPU call: устранено именно повторное **видимое заполнение** из HUD.

### Font atlas и rasterization

- Renderer запрашивает FreeType glyph через **`FT_LOAD_RENDER | FT_LOAD_NO_BITMAP`**, проверяет **`FT_PIXEL_MODE_GRAY`** и хранит antialiased coverage в glyph atlas.
- Atlas кэшируется по face/character/pixel size. Его разрешение, filtering и font texture quality не уменьшались.
- Нужный размер по-прежнему вычисляется из HUD units и физического `ScreenHeight/300`, округляется до целого pixel size и передаётся в `CTextCursor`. Уже готовая картинка времени не масштабируется.
- При scale 100 основное время запрашивает **32 px на 1080p** и **43 px на 1440p**. Это нативная rasterization меньшего шрифта, а не downscale старой текстуры 12.5 units.
- Существующее ограничение glyph atlas **6–128 px** сохранено. Корректность размеров проверена для 1080/1440/2160 px height и HUD scale 50–150%.

По исходникам не обнаружено основания объявлять штатный atlas низкоразрешённым или сломанным. Подтверждены нежелательное повторное заполнение/outline и влияние ink-based centering; связь с конкретным реальным кадром не проверялась.

### Integer alignment и DPI

Pixel alignment сохранён. Он привязывает glyph geometry к пиксельной сетке, но не используется как самостоятельное «лечение» antialiasing.

Для timer text заданы стабильные flags без inherited `NO_PIXEL_ALIGNMENT`, kerning и bearing/oversize overrides. Измерение и создание используют одинаковые параметры. Кэш обновляется при смене целевого размера или pixel height; существующий resize reset также сохранён. Перевод cached container на экран выполняет translation, а не масштабирование готового bitmap времени.

### Почему строка могла подрагивать

`CreateTextContainer` автоматически добавляет `NO_FIRST_CHARACTER_X_BEARING` и `NO_LAST_CHARACTER_ADVANCE`, если line width не положительна. Это делает ширину/центрирование зависимыми от нарисованного контура первой/последней цифры, а не только от её advance.

Аудит реального shipped `DejaVuSans.ttf` подтвердил:

- у всех цифр **0–9 одинаковый advance: 1303 design units**;
- их ink bounds различаются: например, `1` имеет X **225…1114**, `4` — **100…1188**.

В timer cursor теперь задаётся положительная line width. Автоматическое ink trimming не включается; сохраняются полные advances первой/последней цифры. Timer kerning отключён. В рамках одного формата координаты ячеек и ширина строки не зависят от того, заканчивается она на `1`, `4` или другую цифру.

Для измерения контейнера используется **шаблон формата** с заменой цифр на `0`. Он не отображается. При смене минут/секунд ширина не пересчитывается по разным ink bounds; при переходе в часы/дни меняется сама длина формата, а контейнер симметрично расширяется вокруг прежнего центра. Существующие форматы `MM:SS`, `HH:MM:SS`, дни и centiseconds сохранены.

### Text container recreation

Проверено, что `RecreateTextContainerSoft` очищает предыдущие character quads и использует новый cursor: glyph geometry не накапливается поверх старого значения. Этот механизм сохранён. Timer cache имеет корректные размер/pixel-height keys; независимые значения других HUD-модулей не затронуты.

## Границы изменений

Контрольные хеши до/после совпали для:

- `RenderLucentStatus`, `RenderLucentHotbar`, `RenderLucentRecording`, `RenderLucentCoordinates`;
- `components/hud.cpp`, `chat.h`, HUD config declarations и общих `visuals.h` helpers.

Shared text helper меняет rendering только для main/secondary/local timer slots; остальные ветки сохраняют прежние outline/shadow и flags. Other HUD geometry/appearance не перенастраивалась. Существующий clearance использует новые timer bounds для предотвращения пересечения соседних элементов.

## Изменённые файлы

- `src/game/client/lucent/hud/hud.cpp` — fill-only timer text, full-advance measurement/rendering, compact capsule.
- `src/game/client/lucent/hud/layout.h` — основной размер 9, timer X padding и format-template helper.
- `src/test/lucent_hud_test.cpp` — format-transition regression и проверка tabular advances штатного font file.
- `LUCENT-HUD-V1.6-REPORT.md` — этот отчёт.

Новых настроек и production dependencies нет. Предыдущие рабочие изменения сохранены.

## Сборка и тесты

```powershell
./scripts/build-lucent-vs.ps1 -Targets game-client,testrunner,lucent-browser-test,lucent-menu-test,lucent-radial-transition-test,lucent-settings-test,lucent-menu-effects-test
```

**PASS**, exit 0, MSVC x64. Собран существующий профиль **x64-Release**, его фактический build type — **RelWithDebInfo**. Другой build tree или preset не создавался.

| Проверка | Результат |
| --- | --- |
| `testrunner.exe --gtest_filter=LucentHud.*:LucentGameView.* --gtest_color=no` | **PASS, 17/17** |
| Seconds/minutes/hours/days transitions | **PASS:** сохранение форматов, стабильные format cells и общий центр |
| Shipped default font | **PASS:** cmap/hmtx проверены из реального `DejaVuSans.ttf`; все digit advances равны |
| Timer target pixel sizes / DPI geometry | **PASS:** 1080/1440/2160 px height, scale 50–150%, межстрочное пространство |
| Existing HUD/status/hotbar/config/clearance checks | **PASS** |
| `lucent-menu-test.exe` | **PASS** |
| `lucent-browser-test.exe` | **PASS** |
| `lucent-menu-effects-test.exe` | **PASS** |
| `lucent-radial-transition-test.exe` | **PASS** |
| `lucent-settings-test.exe .` | **PASS** |
| `git diff --check` | **PASS** |

Артефакты в `out/build/x64-Release`: `Lucent Client.exe`, `lucent-hud-v1.6-build.log`, `lucent-hud-v1.6-tests.xml`, `lucent-hud-v1.6-font-audit.json`, `lucent-hud-v1.6-preserved.json`. Полный общий набор тестов не запускался.

## Визуальная проверка

**GUI-проверка не выполнялась:** AGENTS.md запрещает Computer Use, GUI automation и screenshot capture. Игра для просмотра не запускалась. В текущем сообщении изображения не приложены, поэтому непосредственное сравнение с упомянутыми скриншотами не выполнялось.

Удаление повторного silhouette/outline и исправление metrics path подтверждены кодом и тестами. Окончательная оценка визуальной чистоты, резкости и контраста капсулы на светлой карте остаётся пользователю; отсутствие всех видимых неровностей не заявляется как визуально проверенное.
