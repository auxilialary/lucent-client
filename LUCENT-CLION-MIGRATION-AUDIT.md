# Lucent Development Workflow — Visual Studio → CLion

Дата: 2026-11-02. Ветка: `feat/game-ui`, без переключения, коммита и PR.

## Результат

CLion workflow стал основным в `AGENTS.md`. Добавлен `scripts/build-lucent-clion.ps1`, который использует фактический выбранный профиль CLion и его существующий cache/File API. Старый `scripts/build-lucent-vs.ps1` сохранён побайтово и разрешён только как явно запрошенный пользователем fallback; автоматически он не вызывается.

Compiler/toolchain не менялись. Пересоздания CMake caches, нового CMake build tree, ручной конфигурации или переключения IDE profiles не было. Не потребовалось изменять `.idea`: CLion уже имел рабочие Debug/Release profiles, выбранный Release и нужный MSVC toolchain.

## Источники конфигурации

Исследование начато через `rg` по `AGENTS.md`, CMake files, scripts, `.idea/workspace.xml`, `.idea/misc.xml`, существующим caches, toolchain XML и логам CLion. Темы, Winterglass, plugins, editor и Project Explorer не перенастраивались.

Корень проекта: `C:\Users\loren\Desktop\dev\Lucent-Client-DDNet`.

Авторитетные источники:

- `.idea/workspace.xml`, `CMakeSettings`: enabled profiles `Debug` и `Release`, оба `TOOLCHAIN_NAME="Visual Studio"`; custom environment/generation options в этих profiles отсутствуют.
- `ExecutionTargetManager`: `CMakeBuildProfile:Release`. RunManager выбран `game-client`. Сохранённая отдельная Google Test run configuration имеет CONFIG_NAME Debug; она не изменялась. CLI следует выбранному CMake profile, а не переопределяет эту run configuration.
- `cmake-build-release/CMakeCache.txt` и `cmake-build-debug/CMakeCache.txt`: реальные compiler/generator/build type/tool paths.
- `.cmake/api/v1/reply/index-*.json`, codemodel и toolchains replies внутри каждого build directory: configuration name, source/build paths, compiler identity/version, target inventory и executable artifacts.
- `CMakeFiles/clion-environment.txt`: MSVC installation, Ninja 1.13.2 и его путь; custom options пусты, кроме заданного CLion `CMAKE_MAKE_PROGRAM`.
- `C:\Users\loren\AppData\Roaming\JetBrains\CLion2026.2\options\windows\toolchains.xml`: named toolchain Visual Studio, kind MSVC, installation `C:\Program Files\Microsoft Visual Studio\18\Community`. MinGW также существует, но profiles его не используют.
- `C:\Users\loren\AppData\Local\JetBrains\CLion2026.2\log\idea.log`: фактические configure/build/environment commands. Наблюдавшаяся IDE-сборка: bundled CMake `--build ...\cmake-build-release --target game-client -j 14`. Инициализация — `vcvarsall.bat amd64`, также зафиксирован вызов с `-vcvars_ver=14.51.36231`.

Имя installation directory **CLion 2026.1.3 не соответствует текущей версии продукта**. Его `product-info.json` сообщает CLion **2026.2.4.1**, build `262.11879.106`, data directory `CLion2026.2`. Реальный bundled CMake сообщает **4.3.1**, что совпадает с File API. Версии не угадывались по имени папки.

## Конфигурация до и после

| Параметр | Прежний default Codex workflow | Новый default CLion workflow |
| --- | --- | --- |
| Entry point | `scripts/build-lucent-vs.ps1` | `scripts/build-lucent-clion.ps1` |
| Selection source | `.vs/ProjectSettings.json`, `x64-Release` | `.idea/workspace.xml`, выбранный `Release` |
| Build directory | `out/build/x64-Release` | `cmake-build-release` |
| CMake build type | RelWithDebInfo | **Release**, как в CLion |
| Generator | Ninja | Ninja |
| CMake executable | VS `Common7/IDE/CommonExtensions/Microsoft/CMake/CMake/bin/cmake.exe` | CLion `bin/cmake/win/x64/bin/cmake.exe`, 4.3.1 |
| Ninja executable | Bundled Visual Studio Ninja из старого cache | CLion `bin/ninja/win/x64/ninja.exe`, 1.13.2 |
| Compiler | MSVC host-x64/target-x64 | Тот же compiler; не заменён на MinGW/Clang |
| SDK | 10.0.28000.0 x64 | 10.0.28000.0 x64 |
| Parallel build | VS script default 6 | 14, как в наблюдавшейся IDE-команде; параметр `-Parallel` доступен |
| Default targets | game-client | all + game-client + testrunner + все шесть Lucent standalone tests |

Точные пути CLion workflow:

- Release: `C:\Users\loren\Desktop\dev\Lucent-Client-DDNet\cmake-build-release`.
- Debug: `C:\Users\loren\Desktop\dev\Lucent-Client-DDNet\cmake-build-debug`, type Debug; проверен через `-Profile Debug -Inspect`, не перестраивался.
- CMake: `C:\Program Files\JetBrains\CLion 2026.1.3\bin\cmake\win\x64\bin\cmake.exe`.
- Ninja: `C:\Program Files\JetBrains\CLion 2026.1.3\bin\ninja\win\x64\ninja.exe`.
- C/C++ compiler: `C:\Program Files\Microsoft Visual Studio\18\Community\VC\Tools\MSVC\14.51.36231\bin\Hostx64\x64\cl.exe`.
- Compiler ID/version: **MSVC 19.51.36260.0**; binary FileVersion совпадает с File API. Номер toolset directory **14.51.36231** — отдельное значение, не подмена версии compiler binary.
- Resource compiler: `C:\Program Files (x86)\Windows Kits\10\bin\10.0.28000.0\x64\rc.exe`.
- Environment initializer: `C:\Program Files\Microsoft Visual Studio\18\Community\VC\Auxiliary\Build\vcvarsall.bat`.
- Rust tools из существующего cache: `C:\Users\loren\.cargo\bin\rustc.exe` и `cargo.exe`, оба **1.96.0**. Их версия и пути не менялись ради миграции.

CLion toolchain называется Visual Studio потому, что использует установленный MSVC. Это не запуск прежнего VS workflow. Новый entry point не читает VS selection или CMakeSettings.json для выбора сборки.

### Совпадение IDE и CLI

Подтверждены совпадение выбранного profile/build type, существующего build directory, source directory, Ninja generator, CMake executable/version, Ninja executable/version, C/C++ compiler path/ID/version, target architecture и Windows SDK. CLI использует тот же generated Ninja build graph, что CLion. `compile_commands.json` для Lucent HUD указывает этот build directory и Release flags `/O2 /Ob2 /DNDEBUG`.

Инициализация CLI environment воспроизводит CLion MSVC recipe и дополнительно фиксирует SDK из его cache:

```text
vcvarsall.bat amd64 10.0.28000.0 -vcvars_ver=14.51.36231
```

После инициализации скрипт проверяет фактически разрешённый `cl.exe`, binary version, VCToolsVersion, WindowsSDKVersion и x64 target. INCLUDE/LIB/LIBPATH/PATH задаются Microsoft initializer, затем все изменённые переменные и рабочая директория восстанавливаются. Отдельно проверены отсутствовавшие INCLUDE/LIB: после вызова они снова отсутствуют, а не остаются пустыми. Сырые значения всего пользовательского environment не выводятся и не сохраняются в отчёт. Произвольные inherited environment variables GUI-процесса побайтово не сравнивались; совпадение подтверждено для build toolchain и его initializer, а не для всего окружения IDE.

## Поведение entry point

- Без `-Profile` читает selected enabled profile. Если generation directory не задан, находит единственный существующий CLion tree с marker и соответствующим cached build type, затем проверяет File API paths. Создавать directory/cache ему не разрешено.
- Проверяет profile/cache/File API/toolchain/SDK/version consistency. Неоднозначность, missing state, неподдерживаемые custom overrides или другая toolchain приводят к ошибке, а не к молчаливому выбору VS, MinGW или другого preset.
- CMake/Ninja берутся из CLion cache и сверяются с File API/environment marker. Global toolchain XML выбирается по реальному `product-info.json`, а не по guessed installation version.
- Targets и пути test executables читаются из выбранного codemodel, а не из старого VS build tree. После возможной штатной Ninja regeneration File API для test artifacts читается заново.
- `-Test` собирает требуемые test targets даже при более узком `-Targets`, запускает полный testrunner, все Lucent standalone tests и существующий `run_rust_tests`. Ошибка одного набора не препятствует запуску остальных, но итоговый вызов возвращает failure.
- Поддерживается фактическая текущая комбинация local Windows/MSVC x64/Ninja. Другие generators/toolchains/custom environment overrides не эмулируются автоматически.

Все существующие Lucent standalone targets:

```text
lucent-browser-test
lucent-menu-effects-test
lucent-menu-test
lucent-ping-test
lucent-radial-transition-test
lucent-settings-test
```

Остальные Lucent gtests входят в `testrunner`. Default `all` дополнительно строит обычные enabled project targets, включая server/tools по существующему CMake graph. Более широкий набор CLI targets намеренно удовлетворяет задаче; выбранный IDE run target game-client при этом не изменён.

## Команды и результаты

Из корня проекта:

```powershell
# Проверка выбранной конфигурации без сборки
./scripts/build-lucent-clion.ps1 -Inspect
./scripts/build-lucent-clion.ps1 -Profile Debug -Inspect

# Полная default сборка и все стандартные тесты
./scripts/build-lucent-clion.ps1 -Test

# Явный диагностический повтор с исключением известного сбоя
./scripts/build-lucent-clion.ps1 -Test -TestFilter '-Net.Ipv4AndIpv6Work'

# Обычная узкая сборка при последующей разработке
./scripts/build-lucent-clion.ps1 -Targets game-client,testrunner
git diff --check
```

| Проверка | Результат |
| --- | --- |
| Full Release build: all/client/testrunner/шесть Lucent executables | **PASS**, 398 Ninja steps; клиент и тестовые executable artifacts созданы в CLion tree |
| Полный C++ testrunner | 492 tests: **487 passed, 4 skipped, 1 failed**; 3 disabled не запускались |
| Все шесть Lucent standalone test executables | **6/6 PASS** |
| Rust `run_rust_tests` | **PASS**: 18 doctests, 10 ddnet_base + 8 ddnet_engine; остальные test/doc groups — 0 tests |
| Диагностический C++ rerun без известного IPv6 test | 491 tests: **487 passed, 4 skipped**; 3 disabled не запускались |
| Standalone/Rust tests при диагностическом повторе | **PASS**, итоговый workflow exit 0 |
| Release и Debug `-Inspect` | **PASS**, профиль, tools и environment проверены |
| Invalid/missing profile, unknown target, shell metacharacters в target, Parallel=0 | **Все ожидаемо отклонены**, без сборки и fallback |
| PATH/INCLUDE/LIB и working directory после Inspect и test workflow | Восстановлены; проверка **PASS** |
| PowerShell parser и `git diff --check` | **PASS** |

**Известный failure:** `Net.Ipv4AndIpv6Work`, IPv6 phase `src/test/net_test.cpp:48–49`: UDP wait timeout 10 s, wait=0, recv=−1 вместо 3. Он не исправлялся и не исключается по умолчанию. Полный workflow закономерно завершился exit 1 после выполнения остальных tests; полный набор **не объявляется зелёным**. Исключение выполнено только отдельной явной командой и записано отдельно. Четыре relay integration tests имеют прежние fixture/environment requirements и остаются skipped.

Rust target использует существующий Cargo test profile и создаёт свои test artifacts под `cmake-build-release/debug`; это штатное поведение существующего target, не новый CMake build tree и не смена Release profile клиента.

Логи и evidence в `cmake-build-release`:

- `lucent-clion-migration-build-tests.log` — полный build, полный C++ прогон, шесть standalone и Rust;
- `lucent-clion-migration-filtered-tests.log` — отдельный повтор без IPv6 failure;
- `lucent-clion-migration-resolved.json`, `lucent-clion-migration-resolved-final.json` — фактически разрешённые параметры;
- `lucent-clion-migration-validation.log` — проверки profiles, восстановления окружения, отклонения неверных аргументов и PowerShell parser;
- `lucent-clion-migration-before.json` — исходные SHA-256 файлов проекта, settings и обоих caches;
- `lucent-clion-migration-integrity.json` — итоговая проверка scope/settings/cache preservation.

## Сохранность пользовательского состояния

Побайтовая SHA-256 проверка подтверждает: **оба CLion CMakeCache.txt неизменны**, все **122** проверенных файла `.idea` и CLion 2026.1/2026.2 `options` неизменны. Темы, Winterglass, plugins, editor и Project Explorer не изменялись. Старый VS script сохранён. Из исходно существовавших неигнорируемых файлов изменён только `AGENTS.md`; добавлены новый build entry point и этот аудит. Все накопленные незакоммиченные source/UI изменения сохранены.

В `AGENTS.md` также добавлено правило использования единого языка дизайна Lucent через существующие `style/tokens.h`, `style/gradient.h`, `ui/` и `motion/` helpers. Ни gameplay, ни Game UI этой миграцией не редактировались. Computer Use и GUI verification не использовались; параметры IDE установлены по сохранённой конфигурации, logs и File API.
