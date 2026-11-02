# Lucent Weapon Hotbar — Animated Selection Scale

2026-11-02, `feat/game-ui`. Коммит и PR не создавались.

Добавлен отдельный presentation state `CWeaponScale`: активное оружие стремится к множителю **1.0**, неактивное — **0.75**, длительность **200 мс**, cubic ease-out. Использован существующий `LucentStyle::Motion::CTransition` — так в репозитории называется указанный в задаче Motion helper. Предыдущее и новое оружие обновляются одним timestamp; быстрое перенаправление начинается с текущего видимого значения перехода.

Множитель применяется к прежнему `WeaponIconScale × BarScale` одинаково по обеим осям. Исходные weapon quads создаются относительно `(-Width/2, -Height/2)`; позиция рендера остаётся центром slot. Поэтому геометрический центр спрайта не перемещается. Сохранены наклон `pi * 7 / 4`, игровые текстуры, прежняя прозрачность, отдельная анимация выделения слотов, ammo и ninja progress. Layout, размеры/позиции hotbar и slots, конфиги и gameplay не менялись.

Состояние хранится по weapon ID, а не по slot index. Исчезнувшее оружие очищает переход; новое оружие в существующем inventory появляется в размере 0.75 и при выборе растёт. Первый показ, смена наблюдаемого игрока/dummy и возврат из пустого inventory инициализируются сразу в конечных размерах. `CHud::OnReset` очищает состояние при смене сессии; существующий `CGameClient::OnStateChange` вызывает reset до online. Неизвестный inventory продолжает использовать только известное активное оружие. Legacy `lucent_hud 0` path не изменён.

Проверки:

- Штатная сборка `./scripts/build-lucent-vs.ps1 -Targets game-client,testrunner` — **PASS**, сохранённый VS `x64-Release`, существующий MSVC x64 tree, CMake type `RelWithDebInfo`.
- `LucentHud.*:LucentTeeAlignment.*:LucentGameView.*` — **29/29 PASS**. Проверены одновременное увеличение/уменьшение, midpoint cubic ease-out, reversal без скачков, завершение и отсутствие overshoot на 30/60/144/240 FPS, все 64 inventory masks, ninja removal/reacquisition, owner/session reset и snapshot-only inventory.
- Геометрический тест расширен на масштабы 0.75, 0.8125, 0.9375 и 1.0 для всех шести weapon quads, 1–6 slots и HUD scale 50–150%. Проверены горизонтальные границы каждого slot и вертикальные границы hotbar после поворота. Поскольку анимация ограничена [0.75, 1.0] и сохраняет центр, промежуточные спрайты не выходят за проверенный максимальный размер и не достигают соседних slots.
- `git diff --check` — **PASS**.

Логи: `out/build/x64-Release/lucent-weapon-scale-build.log`, `lucent-weapon-scale-tests.log`.

GUI, запуск игры и screenshots не выполнялись согласно `AGENTS.md`. Отсутствие clipping/наложений подтверждено расчётом quad geometry и тестами; реальная отрисовка на картах и кастомных skins вручную не проверена.
