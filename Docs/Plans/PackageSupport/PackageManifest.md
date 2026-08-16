---
title: Package Manifest Tasks
status: draft
version: 1.0
updated: 2026-08-16
depends_on:
  - README.md
  - ../../Architecture/Modding.md
  - ../../Architecture/StableIDSpecification.md
---

# M1 — Package Manifest

> **Материализует:** [Modding § Package contents](../../Architecture/Modding.md).
> **Задачи:** PKG-01…04.
> **Результат:** пакет объявляет свою identity, версию и совместимость документом, а не именем каталога.

## Результат этапа

`package.json5` обязателен и является единственным источником identity пакета. Отсутствующий, невалидный или несовместимый манифест отвергается типизированной диагностикой до чтения definitions.

## Задачи

- [ ] **PKG-01 — Сделать манифест обязательным и владеющим identity**
  - Сейчас `package_id` и namespace выводятся из имени каталога (`PackageDiscovery.cpp`), а `package.json5` необязателен и несёт только `redirects`/`tombstones`. Identity пакета не может зависеть от того, как его распаковали.
  - Манифест объявляет `package_id`, `namespace`, `version`; `redirects` и `tombstones` остаются на месте.
  - Done: отсутствие `package.json5` — диагностика `core:diagnostic.package.manifest.missing`; `package_id` вне грамматики segment, несовпадение с ожидаемым namespace и дубликат ключа — раздельные диагностики; вывод identity из имени каталога удалён, а не оставлен как fallback; `GameData/core` и все фикстуры под `Tests/Fixtures/PortableContentCore/` получают манифест.
  - Evidence: <!-- tests/commit/PR -->

- [ ] **PKG-02 — Ввести диапазоны совместимости**
  - Зависимости: PKG-01.
  - Манифест объявляет поддерживаемые диапазоны game/API/schema, как требует [Modding](../../Architecture/Modding.md).
  - Done: несовместимый диапазон отвергает пакет с диагностикой, называющей и требуемый, и фактический диапазон; проверка выполняется до чтения definitions; отсутствие диапазона у `core` не является ошибкой; negative case на каждый вид несовместимости.
  - Evidence: <!-- tests/commit/PR -->

- [ ] **PKG-03 — Ввести объявленные зависимости пакета**
  - Зависимости: PKG-01.
  - Манифест объявляет требуемые пакеты; проверка выполняется на наборе, а не на одиночном пакете, поэтому здесь только парсинг и валидация формы.
  - Done: отсутствующая зависимость и синтаксически невалидная запись — раздельные диагностики; `load_after` парсится, но на порядок не влияет (подсказка редактору, PKG-06); поле не обязательно.
  - Evidence: <!-- tests/commit/PR -->

- [ ] **PKG-04 — Синхронизировать contract и tooling**
  - Зависимости: PKG-01–PKG-03.
  - Done: [Modding](../../Architecture/Modding.md) описывает фактический состав манифеста вместо перечисления намерений; `gv2-content validate` проверяет манифест и печатает его диагностики; `gv2-content new` создаёт манифест для нового пакета; [Implementation Status](../../Status/ImplementationStatus.md) обновлён.
  - Evidence: <!-- tests/commit/PR -->

## Проверка milestone

- [ ] Пакет без манифеста отвергается, а не собирается по имени каталога.
- [ ] `package_id` из манифеста используется в диагностиках и provenance.
- [ ] Несовместимый диапазон отвергает пакет до чтения definitions.
- [ ] `GameData/core` и все фикстуры имеют манифест, тесты проходят без изменения pinned-значений.
