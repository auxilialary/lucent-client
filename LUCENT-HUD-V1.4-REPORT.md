# Lucent HUD v1.4 — точечная компоновка

Дата: 2026-11-02. Ветка: **`feat/game-ui`**. PR не создавался.

## Изменения

### Вертикальный timer

- Горизонтальные колонки и вертикальный divider удалены. Теперь строки расположены сверху вниз: **race/game time → checkpoint/finish, если активен → локальные часы**.
- Каждая видимая строка центрируется по горизонтали относительно середины экрана. Весь контейнер тоже центрирован.
- Основной шрифт: **14 → 13 HUD units**, локальные часы остаются **5** и используют вторичный цвет. Checkpoint/finish остаётся **6**.
- Ширина контейнера равна максимальной измеренной ширине видимого текста плюс **2 units inset с каждой стороны**. Ширины времени не суммируются, пустые колонки не резервируются.
- Высота зависит от видимых строк. Сохранён gap **3** между игровым временем и checkpoint/finish; перед локальными часами — **2**.
- Main only: **17** units высоты; clock only: **9**; main + clock: **24**; main + checkpoint/finish: **26**; все три строки: **33**. Значения приведены при `lucent_hud_scale 100`.
- Сохранены `cl_showhud_timer`, `cl_show_local_time_always`, независимый checkpoint/finish, fade, сигнальные цвета и предупреждения. Расчёты game ticks, форматы времени и сообщения не менялись. Media Island и media-зависимостей нет.

### Hotbar: уменьшение ровно на 15%

Введён локальный визуальный множитель **0.85** только для hotbar. Существующий пользовательский HUD scale продолжает применяться поверх него; диапазон 50–150% не ограничивает внутренний множитель.

| Параметр при HUD scale 100 | v1.3 | v1.4 |
| --- | --- | --- |
| Слот | 22 × 20 | **18.7 × 17** |
| Gap | 2.5 | **2.125** |
| Внешний padding панели | 2 | **1.7** |
| Высота панели | 24 | **20.4** |
| Ширина панели для 1 / 6 слотов | 26 / 148.5 | **22.1 / 126.225** |
| Множитель weapon quad | 0.92 | **0.782** (`0.92 × 0.85`) |
| Radius панели / active fill | 2 | **1.7** |
| Толщина active contour | 0.35 | **0.2975** |
| Размер ammo text | 4 | **3.4** |

Ninja progress, его padding и толщина также уменьшены пропорционально. Центрирование снизу и внешний отступ от нижнего края **6 scaled units** сохранены. Поворот **`pi * 7 / 4`**, reset вращения, active highlight, alpha, game skins, inventory и ammo/ninja logic сохранены. Проверено, что все шесть повёрнутых weapon quads помещаются в уменьшенную панель.

### Coordinates

Возвращён небольшой полупрозрачный фон через существующий panel helper:

- RGB **(0.045, 0.035, 0.065)**.
- Итоговая alpha фона **`0.58 × 0.70 × lucent_hud_opacity/100`**; при текущем default opacity 85% — примерно **0.345**.
- Radius **2 scaled HUD units**, без дополнительной тени панели или толстой рамки.
- Шрифт **5**, padding **2**, внешние размеры **8/15** для одной/двух строк и компактный горизонтальный формат не увеличены. Данные, настройки, сигнальные цвета скорости и clearance относительно статусов сохранены.

## Что сохранено без изменений

**Jump indicators и modifiers:** рендер, размеры, текстуры, оформление, счётчики, условия отображения и упаковка не перерабатывались. Их вертикальная опора отделена от уменьшенной геометрии hotbar и оставлена равной прежнему значению v1.3. Поэтому строки не сдвигаются вслед за уменьшением панели; зазор до hotbar стал немного больше. Позиция coordinates также использует прежнюю информационную опору, чтобы уменьшение hotbar не тянуло её вниз.

**Recording:** метод `RenderLucentRecording` не изменён. Условия развёрнутого чата, стили, размеры, recorder states и форматы времени сохранены. Существующий механизм clearance продолжает учитывать актуальные timer bounds.

До изменений сохранены хеши, после изменений проверено точное совпадение:

- `RenderLucentStatus` и `RenderLucentRecording`;
- `src/game/client/components/chat.h`;
- `src/game/client/components/hud.cpp`;
- HUD config declarations и общие `visuals.h` helpers.

Новых настроек, HUD Editor и других функций нет. ESC, scoreboard, chat, physics и networking не редактировались. Остальные рабочие изменения предыдущих этапов сохранены.

## Файлы этого этапа

- `src/game/client/lucent/hud/layout.h` — вертикальная геометрия timer, локальный hotbar scale и сохранённая информационная опора v1.3.
- `src/game/client/lucent/hud/hud.cpp` — размещение строк, пропорциональное масштабирование hotbar и фон coordinates.
- `src/test/lucent_hud_test.cpp` — актуализированные geometry checks и regression на неизменность status positions.
- `LUCENT-HUD-V1.4-REPORT.md` — этот отчёт.

## Сборка и проверки

```powershell
./scripts/build-lucent-vs.ps1 -Targets game-client,testrunner,lucent-browser-test,lucent-menu-test,lucent-radial-transition-test,lucent-settings-test,lucent-menu-effects-test
```

**PASS**, exit 0, MSVC x64. Собран существующий Visual Studio профиль **x64-Release**, его фактический build type — **RelWithDebInfo**. Другой build tree или preset не создавался.

| Проверка | Результат |
| --- | --- |
| `testrunner.exe --gtest_filter=LucentHud.*:LucentGameView.* --gtest_color=no` | **PASS, 11/11** |
| Vertical timer visibility/width/centering | **PASS:** main, checkpoint/finish и clock on/off; короткие и длинные строки; scale 50/75/100/125/150% |
| Точное уменьшение hotbar | **PASS:** контейнер, слоты, gap, padding, bottom anchor и центрирование |
| Повёрнутые weapon quads | **PASS:** все шесть типов, все позиции 1–6 slots, поддерживаемые масштабы |
| Неизменность status geometry v1.3 | **PASS:** 1–32 элемента, разные widths/aspects, chat widths и HUD scales |
| Existing HUD/config/inventory/clearance regressions | **PASS**, включая 1920×1080 и 2560×1440 |
| `lucent-menu-test.exe` | **PASS** |
| `lucent-browser-test.exe` | **PASS** |
| `lucent-menu-effects-test.exe` | **PASS** |
| `lucent-radial-transition-test.exe` | **PASS** |
| `lucent-settings-test.exe .` | **PASS** |
| `git diff --check` | **PASS** |

Артефакты: `out/build/x64-Release/Lucent Client.exe`, `lucent-hud-v1.4-build.log`, `lucent-hud-v1.4-tests.xml`; контрольные хеши — `lucent-hud-v1.4-preserved.json` в той же build directory. Полный общий набор тестов не запускался.

**GUI-проверка не выполнялась.** Игра для просмотра не запускалась, Computer Use и screenshots не использовались согласно AGENTS.md. Большие наборы jump/modifier states визуально не оценивались и оставлены для отдельной проверки пользователя. Геометрические тесты не заменяют просмотр реального кадра, оценку читаемости и проверку custom skins.
