# Lucent development rules

**Computer Use is prohibited for Lucent development.**

- Never invoke Computer Use.
- Never automate desktop interactions, mouse movements, keyboard input, GUI clicking or screenshot capture.
- Never open the game through GUI automation.
- Use CLI, source inspection, builds and automated tests only.
- If visual verification requires human interaction, mark it as not performed and leave it to the user.
- Do not request Computer Use permissions.

## CLion build workflow (default)

Use `./scripts/build-lucent-clion.ps1` for Lucent CLI builds. It follows CLion's
selected enabled CMake profile in `.idea/workspace.xml` and validates its existing
CMake cache, File API, configured toolchain, compiler, SDK and environment.
It uses the CMake/Ninja recorded by CLion and never creates a build tree or
switches compilers. Do not substitute Visual Studio settings or guess a profile.
If CLion state is missing, ambiguous or inconsistent, report the mismatch; do not
automatically fall back to Visual Studio, reconfigure caches or create directories.

The default builds `all`, `game-client`, `testrunner` and every existing
`lucent-*-test` target discovered from the selected CMake codemodel.
Use `-Targets game-client,testrunner` for a narrower build, `-Profile Debug` only
when explicitly needed, and `-Inspect` to validate and print the resolved workflow
without building. Use `-Test` for the complete C++ suite, all Lucent standalone
test executables and the existing Rust test target. Test failures remain failures;
do not silently exclude the known IPv6 loopback failure. An explicit diagnostic
rerun may use `-TestFilter '-Net.Ipv4AndIpv6Work'` and must be reported separately.

Do not alter CLion themes, Winterglass, plugins, editor settings or Project
Explorer for build tasks. Keep existing `.idea` preferences intact.

## Visual Studio build workflow (explicit fallback only)

Use `./scripts/build-lucent-vs.ps1` only when the user explicitly requests the
legacy Visual Studio workflow. It reads Visual Studio's saved
`.vs/ProjectSettings.json` selection and validates the matching existing CMake cache
before using its CMake and MSVC x64 toolchain. It never creates a build tree.
Pass additional targets with `-Targets game-client,lucent-browser-test,lucent-menu-test,lucent-radial-transition-test`.
If the saved selection is missing or ambiguous, report it; do not guess a preset.
Do not use `out/build/lucent-hover-release` or create alternate build directories.

## Shared Lucent design language

When creating or adapting designs, use the existing shared Lucent design-language
files: `src/game/client/lucent/style/tokens.h`, `style/gradient.h`, `ui/` and
`motion/` helpers. Reuse their colors, surfaces, typography conventions and states;
adapt coordinate density as needed instead of creating an independent design system.
