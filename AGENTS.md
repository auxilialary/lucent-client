# Lucent development rules

**Computer Use is prohibited for Lucent development.**

- Never invoke Computer Use.
- Never automate desktop interactions, mouse movements, keyboard input, GUI clicking or screenshot capture.
- Never open the game through GUI automation.
- Use CLI, source inspection, builds and automated tests only.
- If visual verification requires human interaction, mark it as not performed and leave it to the user.
- Do not request Computer Use permissions.

## Visual Studio build workflow

Use `./scripts/build-lucent-vs.ps1` for CLI builds. It reads Visual Studio's saved
`.vs/ProjectSettings.json` selection and validates the matching existing CMake cache
before using its CMake and MSVC x64 toolchain. It never creates a build tree.
Pass additional targets with `-Targets game-client,lucent-browser-test,lucent-menu-test,lucent-radial-transition-test`.
If the saved selection is missing or ambiguous, report it; do not guess a preset.
Do not use `out/build/lucent-hover-release` or create alternate build directories.
