---
title: Regenerate Golden Run
status: informative
version: 1.0
updated: 2026-08-20
depends_on:
  - README.md
  - ../Architecture/HeadlessSimulationContract.md
---

# Обновить golden run

> **Задача:** осознанно обновить manifest/digest после изменения наблюдаемого replay или script set.
> **Предмет:** `repository_content_hash`, `script_set_hash`, run result и CMake configure cache.
> **Нормативно:** [Headless Simulation § Run manifest](../Architecture/HeadlessSimulationContract.md#run-manifest-и-digest), [Build and Tooling](../Architecture/BuildAndTooling.md).

Повторяемость подтверждают fixtures `Tests/Fixtures/GoldenRuns/*.json5`, replay test в `Headless/CMakeLists.txt` и UE parity consumer `Source/GV2/Private/Tests/GV2RuntimeCoreCrossHostDigestTests.cpp`.

## Сначала определить причину

- Изменились `Scripts/`: ожидаемо меняется `script_set_hash` manifest и digest.
- Изменился frozen corpus `Tests/Fixtures/PortableContentCore/valid/core`: меняется `repository_content_hash`; это отдельное осознанное изменение fixture.
- Изменились команды/seed: это другой scenario, не regeneration существующего golden.
- Изменился только `GameData/`: golden меняться не должен, потому что он пинится к frozen corpus.

## Процедура

1. Проверьте script tree на том же corpus и возьмите `script_set_hash` из JSON:

   ```bash
   ./cmake-build-ci/Headless/gv2-headless \
     --check-scripts \
     --content-root=Tests/Fixtures/PortableContentCore/valid/core
   ```

2. Если hash изменился, обновите только `script_set_hash` в `golden_headless_10_seed_42.manifest.json5`.
3. Воспроизведите записанный manifest, не создавая новый scenario:

   ```bash
   ./cmake-build-ci/Headless/gv2-headless \
     --manifest=Tests/Fixtures/GoldenRuns/golden_headless_10_seed_42.manifest.json5 \
     --content-root=Tests/Fixtures/PortableContentCore/valid/core \
     --output-digest=Tests/Fixtures/GoldenRuns/golden_headless_10_seed_42.digest.json5
   ```

4. Проверьте diff всех полей manifest/digest и объясните каждое изменение.
5. Повторно сконфигурируйте CMake: `Headless/CMakeLists.txt` читает `digest_hash` во время configure, поэтому обычная пересборка может сохранить старое значение.

   ```bash
   cmake -S . -B cmake-build-ci
   ```

6. Запустите golden replay и cross-host parity checks, используемые текущей конфигурацией проекта.

Нельзя копировать hash из нового `--commands/--seed` прогона или менять fixture только ради зелёного теста.
