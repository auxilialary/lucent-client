# Lucent HUD — Weapon Sprite Rendering Quality

Дата: 2026-11-02. Ветка: `feat/game-ui`, без переключения, коммита и PR.

## Вывод и достоверность

Найдены и исправлены два конкретных источника недофильтрации в существующем пути desktop OpenGL: отрицательный world LOD bias, наследуемый маленькими HUD-спрайтами, и ограничение mip-цепочки больших текстур уровнем 5, которое может мешать очень крупным custom game skins. Исправление включается только для отдельных копий шести pickup textures hotbar. Прежний рендер мира, vanilla HUD и прочих текстур сохраняет прежнюю политику.

Не обнаружены последовательные уменьшения bitmap, nearest sampling или отключённые mipmaps у обычных pickup textures. Коэффициенты масштаба объединяются в один GPU transform. Исходные изображения не были заменены, увеличены, размыты или предварительно уменьшены.

Это подтверждённые механизмы ухудшения качества по исходникам и численному анализу, **не доказательство единственной причины конкретного скриншота**. Файл этого скриншота и runtime backend/settings в задаче недоступны. Если у пользователя bias уже равен нулю и skin обычного размера, первый механизм не объясняет его кадр; при исходной пиксельной графике или очень малом конечном размере фильтрация не восстановит отсутствующие детали.

Computer Use, запуск игры, screenshots и GUI-проверка не выполнялись согласно `AGENTS.md`. Реальное GPU-изображение после исправления не объявляется проверенным.

## Полный rendering path, исследованный через rg

1. `datasrc/content.py`: `game` — atlas grid 32×16. Pickup и weapon body для каждого оружия имеют одинаковые пропорции: hammer 4×3, gun 4×2, shotgun/ninja 8×2, grenade 7×2, laser 7×3.
2. `src/game/client/assets.cpp`, `CGameClient::LoadAssetFromPath` / `LoadGameSkin`: загрузка выбранного game skin и optional default fallback. Tee texture-quality limiter в этот путь не входит. PNG `data/game.png` и `data/assets/game/game_06.png` имеют размер 1024×512, то есть 32 px на ячейку.
3. `src/engine/client/graphics_threaded.cpp`, `LoadSpriteTexture`: точное копирование прямоугольника в отдельную texture; fallback при полностью прозрачной области. Полные UV [0,1] относятся к отдельному изображению, не к соседним ячейкам atlas. У нового opt-in flags сохранены и при рекурсивном fallback.
4. `LoadTextureRawMove` и texture-create command: исходный размер передаётся backend без HUD downsample. Backend может уменьшить изображение только при превышении аппаратного maximum texture size. Для стандартных pickup crops этот случай не нужен.
5. `src/game/client/components/hud.cpp`, `PreparePlayerStateQuads`: quad = `VisualSize × 0.25 × (W,H)/hypot(W,H)`. `QuadContainerAddSprite` в `graphics_threaded_sprites.cpp` создаёт quad относительно `(-Width/2,-Height/2)`.
6. `lucent/hud/hud.cpp`: scale = `0.92 × 0.85 × HudScale × AnimatedScale`, где AnimatedScale находится в [0.75,1.0]. При HudScale 100 это **0.782 / 0.5865** относительно исходного HUD quad. Это один численный множитель, а не три bitmap resizes.
7. `RenderQuadContainerAsSprite` → `RenderQuadContainerEx`: buffered path преобразует screen mapping, fallback преобразует float vertices. Оба сохраняют дробные значения; целочисленного округления scale/position здесь нет. Поворот `pi * 7 / 4` выполняется вокруг центра quad, затем используется неизменный центр slot. WrapClamp предотвращает повторение края texture.
8. Desktop OpenGL/OpenGL3: MAG = GL_LINEAR, MIN = GL_LINEAR_MIPMAP_LINEAR при наличии mipmaps; mipmaps генерируются. На legacy hardware без mipmaps используется GL_LINEAR. Vulkan: linear min/mag и linear mip selection, генерация цепочки при поддержке image blitting.
9. Shader sampling использует texture derivatives. Поворот и fractional coordinates сами по себе не требуют nearest sampling, pixel snapping или декоративного blur. Snapping при текущей анимации добавил бы ступени движения; он не вводился.

### Реальные исходные и конечные размеры

Размеры измерены из PNG metadata и рассчитаны по реальному quad path при 1080 px height, HUD scale 100. Конечные размеры — стороны спрайта **до поворота**, не bounding box после поворота.

| Оружие | Pickup crop, px | Активное, px | Неактивное, px | Нейтральный LOD active / inactive |
| --- | --- | --- | --- | --- |
| Hammer | 128×96 | 54.05×40.54 | 40.54×30.40 | 1.24 / 1.66 |
| Gun | 128×64 | 40.29×20.14 | 30.22×15.11 | 1.67 / 2.08 |
| Shotgun | 256×64 | 65.55×16.39 | 49.16×12.29 | 1.97 / 2.38 |
| Grenade | 224×64 | 64.97×18.56 | 48.72×13.92 | 1.79 / 2.20 |
| Laser | 224×96 | 59.51×25.51 | 44.64×19.13 | 1.91 / 2.33 |
| Ninja | 256×64 | 65.55×16.39 | 49.16×12.29 | 1.97 / 2.38 |

Стандартный PNG дополнительно декодирован без GUI: во всех шести crops есть частично прозрачные texels (178–474 на sprite), то есть исходные края уже имеют alpha antialiasing. Это не доказательство качества каждого custom skin. Данные сохранены в `out/build/x64-Release/lucent-sprite-quality-analysis.json`.

Итоговые неактивные gun/shotgun действительно малы по высоте: примерно 15/12 px. Часть исходных деталей неизбежно пропадает. Менять утверждённые размеры или убирать 0.75 scale для сокрытия этого ограничения было бы изменением задачи, поэтому геометрия сохранена.

## Подтверждённые проблемы и исправление

### Наследование sharpening bias

`gfx_gl_texture_lod_bias` по умолчанию равен −500, то есть **−0.5 mip level**. Desktop GL задаёт его в texture/sampler state, Vulkan — в shared sampler. Отрицательное смещение выбирает более детальную mip-выборку, чем нейтральная оценка footprint. Это сохраняет высокие частоты, которые маленький экранный sprite уже не может стабильно представить. Уменьшение до 0.75 дополнительно повышает требуемый LOD на `log2(1/0.75) ≈ 0.415`; прежний −0.5 bias не исчезает. Например, inactive shotgun требует около 2.38, а bias сдвигает выборку к 1.88.

Добавлен opt-in `IGraphics::TEXLOAD_UI_SPRITE` → `TextureFlag::UI_SPRITE`. Его policy возвращает нейтральный bias **0**, не выключая mipmaps. Все прежние callers продолжают использовать Flags=0 и сохраняют исходный bias. Глобальная настройка не меняется, shaders мира не редактируются.

### Ограничение mip-цепочки HD textures

OpenGL3 ограничивает 2D textures с обеими сторонами ≥1024 уровнем 5. Например, hammer crop 2048×1536 от большого custom atlas при прежнем HUD размере требует около LOD 5.24 / 5.66. Ограниченная цепочка не может предоставить такую выборку. `UI_SPRITE` освобождает только выделенные UI textures от этого world workaround; для Flags=0 исходное условие и лимит сохранены. В Vulkan эта GL-специфичная отсечка отсутствует.

### Локализация изменения

- `CGameSkin::m_aLucentHudWeapons[6]`: отдельные handles с теми же original pickup pixels, тем же selected skin и тем же default fallback. Старые `m_aSpritePickupWeapons` не заменены.
- `LoadGameSkin`: создаёт шесть UI copies и выгружает предыдущие при reload. Для стандартного skin дополнительная память — примерно 0.45 MiB с mipmaps; у HD skins больше. Нет загрузки или создания texture на каждом кадре.
- Только `RenderLucentHotbar` выбирает UI handle; при invalid handle использует прежний pickup handle. Подписанные slots, alpha, наклон, центр, scale transition, ammo/ninja bar и вся Lucent Design System остались прежними.
- Backend changes — дополнительная opt-in sampling policy, не смена глобальной фильтрации DDNet. Desktop GL использует существующий per-texture sampler. Vulkan получает две отдельные shared neutral samplers для repeat/clamp; прежние samplers продолжают обслуживать остальные textures.
- Не добавлены конфиги, постобработка, blur, upscale, sharpener, render target, pixel snapping или изменение длительности анимации.

## Jump/modifier icons

`data/hud.png` имеет 512×512 при grid 16×16. Jump и основные modifier icons занимают 2×2 cells, то есть 64×64 px. В `RenderLucentStatus` quad 12×12 масштабируется до 8 HUD units: при 1080p это около 28.8×28.8 px. Поворот равен 0; size animation отсутствует. Исходные UV/crop, loader с Flags=0 и mipmap path штатные.

Глобальный negative bias теоретически может влиять и на такие icons, но отдельного повреждения texture/filter state или подтверждения дефекта на реальном кадре не найдено. Их handles, sampler policy и rendering **не менялись**. Полная функция `RenderLucentStatus` совпадает с ранее сохранённым reference. Порог GL для больших textures стандартные 64×64 crops не затрагивает.

## Проверки и ограничения

- `./scripts/build-lucent-vs.ps1 -Targets game-client,testrunner` — **PASS**. Использованы сохранённый `x64-Release`, существующий `out/build/x64-Release`, MSVC x64 и CMake type `RelWithDebInfo`; альтернативные build trees не создавались.
- `LucentHud.*:LucentTeeAlignment.*:LucentGameView.*:LucentSkinManager.*` — **46/46 PASS**. Сохранены scale/reversal/inventory/ninja/session, rotated geometry и layout tests. Новый test проверяет neutral bias, сохранение world policy, включённые mipmaps и изолированное снятие HD mip cap.
- `git diff --check` — **PASS**. Git дополнительно сообщает о своём LF→CRLF conversion policy для двух файлов; whitespace errors отсутствуют.
- Начальный SHA-256 snapshot неигнорируемого дерева: `lucent-sprite-quality-before.json`. Проверены неизменность оригинальных PNG, `datasrc/content.py`, layout, design helpers и `weapon_scale.h`. Фактический scope: 10 исходных файлов и новый аудит; другие незакоммиченные изменения сохранены. Scope evidence: `lucent-sprite-quality-scope.json`.
- Build/test logs: `lucent-sprite-quality-build.log`, `lucent-sprite-quality-tests.log` в существующем build tree. Полный testrunner в этой задаче не запускался; предыдущие результаты не выдаются за новые.

**Backend coverage:** текущий cache имеет `VULKAN=OFF`, поэтому desktop OpenGL implementation скомпилирован, а Vulkan adaptation проверен только чтением кода, без компиляции и runtime. Настройка VS ради другой сборки не менялась. В OpenGL ES глобальный bias внедряется shader compiler в вызов texture; этот GLES path не нейтрализован новым sampler flag и не проверен текущей Windows-сборкой. На legacy hardware без mipmap support качество ограничено bilinear sampling. Эти пути не объявляются исправленными или визуально проверенными.

GPU output, live skin reload и сравнение конкретного пользовательского скриншота требуют ручной проверки. Исправление устраняет установленные desktop sampling restrictions; оно не создаёт новые детали в исходном bitmap и не гарантирует отсутствие любых ступенчатых краёв при произвольно малом HUD scale.
