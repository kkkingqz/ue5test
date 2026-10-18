---
title: RH Hunter Gameplay
status: draft
version: 0.3
updated: 2026-10-18
depends_on:
  - GameplayArchitecture.md
  - ../Status/EngineGaps.md
decisions:
  - ../ADR/0051-temporary-modes-and-presentation-routing.md
---

# Охотник: правила игры

> **Владеет:** правилами первой игры пакета `rh` «Охотник»: классами, полями охотника и дикарки, формулами, предметами, локациями, охотой, добычей, встречами, торговлей, танцем, стартом, концом игры и победой, ресурсами с номерами исходного проекта.
> **Не владеет:** общими механиками и модулями сцен — их задаёт [архитектура геймплея RH](GameplayArchitecture.md); здесь только данные и правила, которыми «Охотник» их наполняет.
> **Статус:** `draft` — проект игры до реализации.

Формулы и номера ресурсов перенесены из исходного проекта игры; имена переменных, классы, функции и данные спроектированы заново. Псевдокод записан в форме authoring Lua, как в [архитектуре](GameplayArchitecture.md); чистые функции получают бросок аргументом `rng(a, b)`.

## 1. Игра

Охотник приходит в Рим через Туманы, уходит в Дикий лес, ловит дикарок и продаёт их на рынке или в лавке. Усталость копится от действий и при превышении порога убивает; голод ускоряет усталость; время суток сдвигают сами действия, кнопки конца хода нет. Победа — накопить больше 999 золота; после неё можно продолжать.

Чем «Охотник» пользуется из общей архитектуры: время от действий и фазы времени, словари, конвейер взаимодействия без стоимости в выносливости, обработчик после команды, роль `catch`, типовой NPC (дикарка), модули диалога, боя и торговли, собственный режим танца, общий интерфейс. Не пользуется: выносливостью, командой конца хода и отчётом, сознанием, моделями NPC, процессами, квестами.

## 2. Модель данных

### 2.1. Охотник (actor игрока)

| Поле | Тип | Значения | Бывшее имя |
|---|---|---|---|
| `hunter_class` | ref `hunter_class` | 4 класса | `class` |
| `display_name` | string | ввод игрока в модальном окне | `name` |
| `gold` | int ≥ 0 | старт 50 | `money` |
| `stealth`, `attack`, `pursuit` | характеристики общего набора | по классу; `pursuit` 10 | `sneak`, `atk`, `chase` |
| `fatigue` | int ≥ 0 | 0 — свеж; > 7 — смерть | `energy` |
| `hunger` | int ≥ 0 | | `hunger` |
| `woods_depth` | int 0…3 | хранится: лагерь общий для всех глубин | `deep` |
| `fog_destination` | ref `location` | лес или площадь Рима | направление в Туманах |
| `fog_steps` | int 0…9 | | `fog_timer` |
| `ghost_horse_nearby` | bool | ржание услышано; не сбрасывается при выходе из Туманов | — |
| `ghost_horse_tamed` | bool | предмета «Конь-призрак» нет | `ghost` |
| `traps_set` | bool | | `trap` |
| `trade_skill_learned` | bool | после диалога обучения | `shop == 3` |
| `met_farid` | bool | Фарид уже знаком: приветствие по имени | `farid` |
| `endless_mode` | bool | | `finished` |
| `hound_hunger`, `retriever_hunger` | int ≥ 0 | | `dog_hung`, `ret_hung` |
| `catch` | ref instance `actor`, nullable | текущая добыча | «Добыча» |
| `inventory` | `item → count` | [GAP-008](../Status/EngineGaps.md#gap-008-поля-коллекции-и-целостность-ссылок-на-экземпляры) | `Items` + `Goods` |
| `stamina` | int ≥ 0 | механизм пакета, «Охотник» не использует | — |

Вычисляются и не хранятся: `is_female()` (из класса; бывший `sex`), `stat(name)`, `starving_penalty()`, `fatigue_gain()`, `hunger_state()`, зона (бывшие `rome`, `fog`).

### 2.2. Дикарка (экземпляр шаблона `rh:actor.beast.wild`)

| Поле | Значения | Бывшее имя |
|---|---|---|
| `looks` | 1…6 | внешность |
| `height` | 1…3 | рост |
| `hair` | `blond`, `brunette`, `red` | цвет 1, 2, 3 |
| `figure` | 1…7 | ноги |
| `agility` | 1…7 | ловкость |
| `wits` | 1…7 | ум |
| `maturity` | 1…4 | возраст; все значения — взрослые женщины разных лет |
| `alive` | bool | `isalive` / `beast == 2` |
| `tattoo` | bool | `tatoo` (2 → `true`, 1 → `false`) |
| `subdued` | bool | `dominated` |
| `dance_bonus_given` | bool | бонус за первый танец уже получен (§ 4.10) |
| `beast_look` | ref `beast_look` | облик, выбранный при создании (§ 3.6) |

Экземпляр создаётся при встрече: при входе в режим охоты или при срабатывании ловушки. Облик выбирается под уже сгенерированные значения и хранится, потому что при равных кандидатах выбор случайный. Если встреча кончилась без поимки, экземпляр удаляется ([GAP-018](../Status/EngineGaps.md#gap-018-имя-экземпляра-и-удаление-персонажа-из-authoring)).

### 2.3. Мир

| Поле | Значения | Бывшее имя |
|---|---|---|
| `world.time_of_day` | `morning`, `day`, `evening`, `night` | `time` 1…4 |

### 2.4. Объявление полей

Характеристики `stealth`, `attack`, `pursuit`, `looks`, `height`, `figure`, `agility`, `wits`, `maturity` объявлены общим набором ([архитектура, § 4.3](GameplayArchitecture.md#43-характеристики-и-модификаторы)). Остальные поля «Охотника»:

```lua
-- model/hunter.lua
Actor.hunter_class        = field.ref_definition("hunter_class", { nullable = true })
Actor.display_name        = field.string({ max_length = 40, nullable = true })
Actor.gold                = field.non_negative_integer()
Actor.stamina             = field.non_negative_integer()
Actor.fatigue             = field.non_negative_integer()
Actor.hunger              = field.non_negative_integer()
Actor.woods_depth         = field.integer({ min = 0, max = 3 })
Actor.fog_destination     = field.ref_definition("location", { nullable = true })
Actor.fog_steps           = field.non_negative_integer()
Actor.ghost_horse_nearby  = field.boolean({ default = false })
Actor.ghost_horse_tamed   = field.boolean({ default = false })
Actor.traps_set           = field.boolean({ default = false })
Actor.trade_skill_learned = field.boolean({ default = false })
Actor.met_farid           = field.boolean({ default = false })
Actor.endless_mode        = field.boolean({ default = false })
Actor.hound_hunger        = field.non_negative_integer()
Actor.retriever_hunger    = field.non_negative_integer()
Actor.catch               = field.ref_instance("actor", { nullable = true })

-- model/beast.lua
Actor.hair              = field.enum({ "blond", "brunette", "red" }, { nullable = true })
Actor.alive             = field.boolean({ default = true })
Actor.tattoo            = field.boolean({ default = true })
Actor.subdued           = field.boolean({ default = false })
Actor.dance_bonus_given = field.boolean({ default = false })
Actor.beast_look        = field.ref_definition("beast_look", { nullable = true })

function Actor:is_female()      return self.hunter_class.is_female end
function Actor:has_trait(trait) return contains(self.hunter_class.traits, trait) end
function Actor:has(item)        return self:count(item) > 0 end
```


### 2.5. Kinds игры

| Kind | Содержит |
|---|---|
| `hunter_class` | `stealth`, `attack`, `pursuit`, `is_female`, `traits`, тексты названия и сложности |
| `capture_method` | стадия охоты, требования, расход, проверка с параметрами, исход успеха и провала (§ 4.5) |
| `beast_look` | облик дикарки: цвет, зрелость, условия на черты, картинки (§ 3.6) |
| `beast_trait_scale` | шкала описания черты: полосы значений → текст (§ 3.8) |
| `hunt_scene` | сцена обнаружения дикарки: текст подкрадывания и картинка (§ 4.5) |

Остальные данные — общие kinds архитектуры: `interaction`, `location`, `item`, `dialogue`, `combat`, `shop`, `time_phase`, `config`, `scenario`.

### 2.6. Классы

| Класс | ID | stealth | attack | pursuit | is_female | traits |
|---|---|---|---|---|---|---|
| Ловчий | `rh:hunter_class.trapper` | 11 | 9 | 10 | false | `net` |
| Ассасин | `rh:hunter_class.assassin` | 12 | 8 | 10 | false | `throwing_knives` |
| Варвар | `rh:hunter_class.barbarian` | 8 | 12 | 10 | false | `bear_hug` |
| Госпожа | `rh:hunter_class.mistress` | 10 | 10 | 10 | true | `subdue`, `no_dance`, `catch_never_escapes`, `peaceful_owlbear`, `refuses_chainmail` |

`catch_never_escapes` действует только при ходьбе по лесу и отдыхе в лагере; на охоте дикарка замечает и убегает от Госпожи как от всех.

### 2.7. Конфигурация

```json5
{
  id: "rh:config.main",
  start_gold: 50,
  fatigue_death_above: 7,
  starving_above: 6,
  hound_hunger_limit: 7,        // > 7 — ищейка пропадает
  retriever_hunger_limit: 8,    // > 8 — ретривер съедает хозяина
  victory_gold_above: 999,
  butcher_base_price: [50, 25, 15, 15],   // по зрелости 1…4
  market_estimate_bonus: 24,    // оценка Фарида
  market_minimum_bonus: 0,      // оценка птицы-секретаря
  owlbear_head_price: 50,
  rob_meat_amount: 10,
  fog_horse_steps: 10,
  escape_threshold: { walk: 10, rest: 10, night_sleep: 8 },
  chase_lose_chance: [1, 2],    // 1 из 2: после неудачного шага погони дикарка потеряна
}
```

### 2.8. Предметы

| Предмет | ID | Вкладка | Цена | stackable | Действие в игре |
|---|---|---|---|---|---|
| Одеяло | `rh:item.tool.blanket` | tools | 5 | нет | ночной сон снимает всю усталость |
| Оковы | `rh:item.tool.shackles` | tools | 25 | нет | добыча не убегает |
| Ловушки | `rh:item.tool.traps` | tools | 50 | нет | ставятся на опушке; при установке исчезают навсегда |
| Нож | `rh:item.weapon.knife` | weapons | 5 | нет | захват `knife`; зарезать, обокрасть |
| Дубинка | `rh:item.weapon.club` | weapons | 10 | нет | захват `club` |
| Арбалет | `rh:item.weapon.crossbow` | weapons | 20 | нет | захват `crossbow` |
| Меч | `rh:item.weapon.sword` | weapons | 75 | нет | бой с совомедведем |
| Шокер | `rh:item.weapon.taser` | weapons | 100 | нет | захват `taser` |
| Мягкие ботинки | `rh:item.clothes.soft_boots` | clothes | 25 | нет | `modifiers: { stealth: 2 }` |
| Кольчуга | `rh:item.clothes.chainmail` | clothes | 50 | нет | бой с совомедведем всегда успешен; Госпожа отказывается |
| Эльфийский плащ | `rh:item.clothes.elven_cloak` | clothes | 200 | нет | `modifiers: { stealth: 5 }` |
| Птица-секретарь | `rh:item.pet.secretary_bird` | pets | 10 | нет | оценка добычи (§ 4.8) |
| Ищейка | `rh:item.pet.hound` | pets | 50 | нет | дикарка находится всегда; натравить на совомедведя; голодает |
| Ретривер | `rh:item.pet.retriever` | pets | 300 | нет | захват `retriever`; натравить на совомедведя; голодает |
| Припасы | `rh:item.food.supplies` | consumables | 1 | да | еда |
| Тоник | `rh:item.potion.tonic` | consumables | 10 | да | `fatigue = 0` |
| Дымовая шашка | `rh:item.potion.smoke_bomb` | consumables | 20 | да | уход от совомедведя |
| Приманка (бывшее «Зелье») | `rh:item.potion.lure` | consumables | 25 | да | выход на дикарку без поиска и проверки «заметила» |
| Усыпляющий газ | `rh:item.potion.sleeping_gas` | consumables | 40 | да | захват `sleeping_gas` |
| Мясо | `rh:item.food.meat` | — | — | да, портится | еда |
| Консервы | `rh:item.food.canned_meat` | — | — | да, портится | еда, снимает 1 усталости |
| Голова совомедведя | `rh:item.trophy.owlbear_head` | — | — | нет | Менхантер покупает за 50 |
| Перо совомедведя | `rh:item.trophy.owlbear_feather` | — | — | нет | совомедведь больше не появляется |
| Навигационный кристалл | `rh:item.misc.nav_crystal` | — | — | нет | вход в Туманы; выдаётся в конце вступления |
| Записки охотника | `rh:item.misc.hunter_notes` | — | — | нет | совет по зоне, где стоит охотник (§ 4.8); выдаются вместе с кристаллом |

Уникальный предмет второй раз не покупается. Менхантер покупает только голову совомедведя. Кошелька в игре нет: золото всегда видно на панели статуса.

Инвентарь показывается отдельной панелью общего интерфейса (§ 6.1). Из неё доступны тоник, птица-секретарь и записки; действия с добычей и еда — только в лагере.

## 3. Формулы (`rules/formulas/`)

Чистые функции. Броски делаются в порядке источника; бросок делается и тогда, когда его результат перекрывается (ищейка, кольчуга, единственный кандидат облика), чтобы последовательность потока не зависела от инвентаря и контента.

### 3.1. Генерация дикарки

```lua
-- rules/formulas/generation.lua
local HAIR = { "blond", "brunette", "red" }

function generate_beast(depth, rng)
    local looks    = rng(1, 6)
    local height   = rng(1, 3)
    local hair     = HAIR[rng(1, 3)]
    local figure   = math.min(7, rng(1, 7) + depth * 2)
    local agility  = math.min(7, rng(1, 7) + depth * 2)
    local wits     = math.min(7, rng(1, 7) + depth * 2)
    local maturity = math.min(4, rng(1, 4) + depth)
    return {
        looks = looks, height = height, hair = hair,
        figure = figure, agility = agility, wits = wits, maturity = maturity,
        alive = true, tattoo = true, subdued = false,
    }
end
```

| `woods_depth` | figure / agility / wits | maturity |
|---|---|---|
| 0 | 1–7 | 1–4 |
| 1 | 3–7 | 2–4 |
| 2 | 5–7 | 3–4 |
| 3 | 7 | 4 |

### 3.2. Голод и усталость

```lua
-- rules/formulas/survival.lua
function starving_penalty(hunger)          -- 0 или отрицательное число
    if hunger > 6 then return 6 - hunger end
    return 0
end

function fatigue_gain(hunger)               -- прирост усталости от действия
    return 1 - starving_penalty(hunger)     -- голод 7 → 2, 8 → 3, …
end

function hunger_state(hunger)
    if hunger < 2 then return "fed" end
    if hunger <= 3 then return "hungry" end
    if hunger <= 5 then return "very_hungry" end
    if hunger == 6 then return "famished" end      -- на грани истощения
    return "exhausted"
end

function dies_of_fatigue(fatigue)          return fatigue > 7 end
function hound_lost(hound_hunger)          return hound_hunger > 7 end
function retriever_turns(retriever_hunger) return retriever_hunger > 8 end
function reached_victory(gold)             return gold > 999 end
```

### 3.3. Охота

```lua
-- rules/formulas/hunt.lua
function find_roll(has_hound, rng)
    local roll = rng(1, 10)
    if has_hound then roll = 10 end
    return roll
end
function is_found(roll) return roll >= 5 end

-- и «заметила первой», и «подкрадывание»
function beast_notices(beast, stealth, rng)
    return rng(1, 10) + beast.wits > stealth
end

function beast_outruns(beast, pursuit, rng)
    return rng(1, 10) + beast.figure > pursuit
end

function beast_lost_in_chase(rng)          -- после неудачного шага погони
    return rng(1, 2) == 1
end

-- проверки захвата; параметры — из Definition способа (§ 4.5)
CAPTURE_CHECKS = {
    always = function(beast, hunter, p, rng) return true end,
    agility_vs_attack = function(beast, hunter, p, rng)     -- захват, дубинка, нож, руки
        return rng(1, p.die) + beast.agility <= hunter:stat("attack") + (p.attack_bonus or 0)
    end,
    roll_above_agility = function(beast, hunter, p, rng)    -- сеть
        return rng(0, 9) > beast.agility
    end,
    roll_equals = function(beast, hunter, p, rng)           -- камень
        return rng(1, 10) == 10
    end,
}

function escape_roll(beast, rng) return rng(1, 10) + beast.agility end
```

### 3.4. Лес, Туманы, ловушки, совомедведь

```lua
-- rules/formulas/wilds.lua
function owlbear_appears(depth, rng)   return rng(1, 10) + depth * 2 > 10 end
function owlbear_flee_kills(rng)       return rng(1, 2) == 2 end
function owlbear_fight_won(has_chainmail, rng)
    local roll = rng(1, 10)
    if has_chainmail then roll = 10 end
    return roll >= 5
end
function fog_exit_found(rng)           return rng(1, 4) == 4 end
function trap_caught(rng)              return rng(2, 3) == 3 end
```

### 3.5. Цены

```lua
-- rules/formulas/pricing.lua
local function tattoo_mult(beast) return beast.tattoo and 2 or 1 end
local function traits_sum(beast) return beast.figure + beast.agility + beast.wits + beast.maturity end
local function market_base(beast) return beast.looks * tattoo_mult(beast) * 2 * traits_sum(beast) end

function market_price(beast, rng)      return market_base(beast) + rng(1, 50) end   -- продажа Фариду
function market_estimate(beast, bonus) return market_base(beast) + bonus end        -- Фарид: 24, птица: 0

function butcher_price(beast, base)    -- лавка; base = { 50, 25, 15, 15 }
    return base[beast.maturity] * tattoo_mult(beast)
end
```

Максимум у Фарида: `6 · 2 · 2 · (7+7+7+4) + 50 = 650`.

| Зрелость | Лавка | С тату |
|---|---|---|
| 1 | 50 | 100 |
| 2 | 25 | 50 |
| 3 | 15 | 30 |
| 4 | 15 | 30 |

### 3.6. Облик дикарки

Облик (`beast_look`) — Definition, которая задаёт, к каким значениям черт подходит картинка, и набор картинок для разных состояний. Облик выбирается один раз при создании экземпляра и хранится в нём.

```json5
{
  id: "rh:beast_look.blond_02_foxy",
  hair: "blond",
  maturity: [2, 2],                       // диапазон зрелости
  when: [{ trait: "wits", min: 5, max: 6 }],
  images: {
    portrait: "rh:resource.beast.portrait.blond_02_foxy",
    // dance, fashion, corpse — необязательны; без них — запасные картинки (§ 7.2)
  },
}
```

```lua
-- rules/formulas/look.lua
local function fits(look, beast)
    if look.hair ~= beast.hair then return false end
    if beast.maturity < look.maturity[1] or beast.maturity > look.maturity[2] then return false end
    for _, c in ipairs(look.when or {}) do
        local v = beast[c.trait]
        if (c.min and v < c.min) or (c.max and v > c.max) then return false end
    end
    return true
end

-- Побеждает самый точный облик (больше условий); при равенстве — случайный
function choose_look(beast, looks, rng)     -- looks: все beast_look в порядке Stable ID
    local best, best_score = {}, -1
    for _, look in ipairs(looks) do
        if fits(look, beast) then
            local score = #(look.when or {})
            if score > best_score then best, best_score = { look }, score
            elseif score == best_score then table.insert(best, look) end
        end
    end
    return best[rng(1, #best)]              -- бросок есть и при одном кандидате
end

function beast_image(beast, purpose, fallbacks)   -- purpose: portrait | dance | fashion | corpse
    if purpose == "portrait" and not beast.alive then purpose = "corpse" end
    local image = beast.beast_look.images[purpose]
    return image or fallbacks[purpose](beast)
end
```

Правила системы:

- Для каждой пары «цвет × зрелость» обязан существовать базовый облик без условий `when`, поэтому кандидат есть всегда. Перечислитель проверки — значения enum `hair` и диапазон поля `maturity` из `STATS`.
- Новая картинка — новая Definition облика, без кода. Старая таблица портретов переводится в облики построчно (§ 7.1); её порядок «последнее совпадение побеждает» заменён правилом точности.
- Облик не меняется после создания: смерть меняет картинку через `images.corpse`, а не выбор облика.

### 3.7. Танец (чистое ядро)

Ядро повторяет устройство исходного модуля: одно состояние, цикл обычного танца, подрежим fashion со своим циклом, отдельные функции движений, мягких и жёстких действий, финалов. Переменные переименованы, номера текстов и пороги сохранены. Ядро canonical state не трогает: итоги — тату, усталость, бонус — применяет оболочка при выходе (§ 4.10).

| Источник | Здесь | Значения |
|---|---|---|
| `hunter_a` | `hunter_engagement` | 0…100+ |
| `beast_a` | `beast_engagement` | может уходить в минус |
| `temp` | `tempo` | 0 стоп, 1 медленно, 2 средне, 3 жёстко |
| `text` | `text` | номер текущего текста |
| `recent_b_finish` | `beast_finished_recently` | |
| `b_finish` | `beast_finished` | дикарка уже завершала в этом танце |
| `finish` | `finish` | 0 — нет, 1, 2 — как завершил |
| `fashion` | `fashion` | подрежим fashion |

```lua
-- rules/formulas/dance.lua

-- StartDance / StartFashion: «Танец» — fashion = false, «Танец (красивый)» — fashion = true
function dance_start(fashion)
    return {
        hunter_engagement = 0, beast_engagement = 0, tempo = 0, text = 0,
        beast_finished_recently = false, beast_finished = false,
        finish = 0, fashion = fashion, offer_kill = false,
    }
end

-- DanceLoop, шаг 1: пороги после каждого действия
function dance_check(s)
    if s.beast_engagement > 100 and not s.beast_finished_recently then return "beast_finish" end
    if s.hunter_engagement > 100 then return "finish" end
    return nil
end

-- DanceLoop, шаг 3: какие действия доступны. Экран показывает все; false — неактивно
-- с причиной rh:text.error.dance.<действие>
function dance_actions(s)
    if s.fashion then return fashion_actions(s) end
    return {
        finish = s.hunter_engagement > 75,          -- «Закончить»
        pause  = s.hunter_engagement > 75,          -- «Приостановиться»
        soft   = true,                              -- «Танцевать дальше» → подменю 5 действий
        hard   = true,                              -- «Танцевать ближе» → подменю 5 действий
        start  = s.tempo == 0,                      -- «Начать движения»
        softer = s.tempo > 1,                       -- «Двигаться нежнее»
        keep   = s.tempo > 0,                       -- «Держать темп»
        harder = s.tempo > 0 and s.tempo < 3,       -- «Двигаться жёстче»
        kill   = s.offer_kill,                      -- после текста 651
        leave  = true,                              -- «Оставить в покое»
    }
end

-- Beast_Dance_Moves
function dance_move(s, move, rng)
    local b = s.beast_engagement
    if move == "pause" then
        s.hunter_engagement, s.tempo, s.text = 0, 0, 1
        if b > 20 then
            if b < 51 then b = b - rng(5, 10)
            elseif b < 81 then b = b - rng(10, 20)
            else b = b - rng(20, 40) end
        end
    elseif move == "start" then
        s.text, s.tempo = 21, 1
        s.hunter_engagement = s.hunter_engagement + rng(5, 10)
    elseif move == "softer" then
        if s.tempo == 2 then
            s.text = 22
            if b < 51 then b = b + rng(5, 10) end
        else                                        -- tempo == 3
            s.text = 23
            if b > 50 and b < 81 then b = b + rng(5, 10) end
        end
        s.tempo = s.tempo - 1
    elseif move == "keep" then
        if s.tempo == 1 then
            s.text = (s.text == 31) and 301 or 31
            s.hunter_engagement = s.hunter_engagement + rng(5, 10)
            if b < 51 then b = b + rng(10, 20)
            elseif b < 81 then b = b + rng(5, 10) end
        elseif s.tempo == 2 then
            s.text = (s.text == 32) and 302 or 32
            s.hunter_engagement = s.hunter_engagement + rng(10, 20)
            if b > 20 and b < 81 then b = b + rng(15, 20)
            elseif b > 80 then b = b + rng(5, 10) end
        else                                        -- tempo == 3
            s.text = (s.text == 33) and 303 or 33
            s.hunter_engagement = s.hunter_engagement + rng(15, 25)
            if b > 80 then b = b + rng(10, 20)
            elseif b > 50 then b = b + rng(5, 10)
            elseif b < 21 then b = b - rng(5, 10) end
        end
    elseif move == "harder" then
        if s.tempo == 2 then
            s.text = 42
            s.hunter_engagement = s.hunter_engagement + rng(10, 20)
        else                                        -- tempo == 1
            s.text = 41
            s.hunter_engagement = s.hunter_engagement + rng(5, 10)
        end
        s.tempo = s.tempo + 1
    end
    s.beast_engagement = b
end

-- Beast_Soft_Acts
function dance_soft(s, n, rng)
    local b = s.beast_engagement
    if n == 1 then
        if b < 21 then b = b + rng(3, 6); s.text = 51 else s.text = 511 end
    elseif n == 2 then
        if b < 51 then b = b + rng(4, 8); s.text = 52 else s.text = 521 end
    elseif n == 3 then
        if b < 21 then b = b + rng(3, 6); s.text = 53
        elseif b <= 80 then b = b + rng(6, 12); s.text = 531
        else s.text = 532 end
    elseif n == 4 then
        if b < 51 then b = b + rng(6, 12); s.text = 54 else s.text = 541 end
    elseif n == 5 then
        if b < 21 then b = b - rng(3, 6); s.text = 55
        elseif b <= 50 then b = b + rng(3, 6); s.text = 551
        elseif b <= 80 then b = b + rng(6, 12); s.text = 552
        else s.text = 553 end
    end
    s.beast_engagement = b
end

-- Beast_Hard_Acts
function dance_hard(s, n, rng)
    local b = s.beast_engagement
    if n == 1 then b = b - rng(3, 9); s.text = 61
    elseif n == 2 then b = b - rng(5, 15); s.text = 62
    elseif n == 3 then
        if b < 81 then b = b - rng(3, 9); s.text = 63
        else b = b + rng(1, 3); s.text = 631 end
    elseif n == 4 then b = b - rng(4, 12); s.text = 64
    elseif n == 5 then
        if b > 0 then
            b = 0; s.beast_finished_recently = false; s.text = 65
        else
            s.text = 651; s.offer_kill = true       -- предложение убить; нож не нужен
        end
    end
    s.beast_engagement = b
end

-- BeastFinish: дикарка завершает; итог отдаётся оболочке
function dance_beast_finish(s)
    local first = not s.beast_finished
    s.beast_finished_recently = true
    s.beast_finished = true
    return { fatigue_reset = true, first = first }
end

-- Finish: финал обычного танца
function dance_finish(s, choice)                    -- "bow" | "flourish" | "fashion"
    if choice == "bow" then s.text, s.finish = 71, 1             -- «Поклониться»
    elseif choice == "flourish" then s.text, s.finish = 72, 1    -- «Артистично поклониться»
    else s.text, s.finish, s.fashion = 87, 2, true end           -- источник: переход к финалу fashion
end

-- FashionLoop
function fashion_actions(s)
    local h = s.hunter_engagement
    return {
        finish_close = h > 10,                      -- «Закончить танец близко»
        finish_far   = h > 10,                      -- «Закончить танец дальше»
        begin        = h == 0,                      -- «Заставить танцевать»
        hold         = h > 0 and h <= 10,           -- «Удерживать дистанцию»
        close_in     = h > 0 and h <= 10,           -- «Сократить дистанцию»
        move_fast    = h > 0 and h <= 10,           -- «Двигаться быстро»
        leave        = true,                        -- «Оставить в покое»
    }
end

-- FashionAct
function fashion_act(s, act)
    if act == "begin" then s.hunter_engagement, s.text = 1, 83
    elseif act == "hold" then s.hunter_engagement, s.text = s.hunter_engagement + 3, 84
    elseif act == "close_in" then s.hunter_engagement, s.text = s.hunter_engagement + 5, 85
    elseif act == "move_fast" then s.hunter_engagement, s.text = 11, 86 end
end

-- FashionFinish
function fashion_finish(s, where)                   -- "close" | "far"
    if where == "close" then s.text, s.finish = 81, 2       -- в источнике здесь же тексты 91 / 92
    else s.text, s.finish = 82, 1 end
end
```

**Отрицательная вовлечённость дикарки.** Проверка формулы:

- уменьшают `beast_engagement`: пауза, «держать темп» на темпе 3 при значении < 21, мягкое 5 при ≤ 20, жёсткие 1, 2, 4 всегда, жёсткое 3 при ≤ 80;
- пауза в минус не уводит: она срабатывает только при значении > 20, и наименьший результат по полосам — 11, 31 и 41;
- в минус уводят «держать темп 3», мягкое 5 и жёсткие 1–4: из 0 — до −10, −6, −9, −15, −9 и −12;
- формула минус учитывает: жёсткое 5 проверяет `≤ 0` и открывает «Убить»; остальные ветви трактуют минус как «меньше 21» и поднимают значение.

Вывод: отрицательные значения — часть правил, ядро их не обрезает. Полоска на экране показывает `clamp(beast_engagement / 100, 0, 1)` — это presentation, не правило.

### 3.8. Описание черт

Шкала (`beast_trait_scale`) — данные: полосы значений по возрастанию, каждой полосе — текст.

```json5
{
  id: "rh:beast_trait_scale.looks",
  trait: "looks",
  label: "rh:text.beast.trait.looks",
  bands: [
    { max: 2, text: "rh:text.beast.looks.attractive" },   // привлекательная
    { max: 4, text: "rh:text.beast.looks.pretty" },       // красивая
    { max: 5, text: "rh:text.beast.looks.beautiful" },    // прекрасная
    { max: 6, text: "rh:text.beast.looks.dazzling" },     // ослепительно прекрасная
  ],
}
```

```lua
-- rules/formulas/description.lua
function trait_text(scale, value)
    for _, band in ipairs(scale.bands) do
        if value <= band.max then return band.text end
    end
end

function beast_description(beast, scales)   -- строки «Черта: текст» (GAP-011)
    local lines = {}
    for _, scale in ipairs(scales) do
        table.insert(lines, { label = scale.label, value = trait_text(scale, beast[scale.trait]) })
    end
    return lines
end
```

Предлагаемые шкалы:

| Черта | Полосы | Тексты (смысл) |
|---|---|---|
| `looks` 1–6 | 1–2, 3–4, 5, 6 | привлекательная → ослепительно прекрасная |
| `height` 1–3 | 1, 2, 3 | миниатюрная, обычная, высокая |
| `hair` | 3 значения | блондинка, брюнетка, рыжая |
| `figure` 1–7 | 1–2, 3–4, 5–6, 7 | пухлая → совершенное тело |
| `agility` 1–7 | 1–2, 3–4, 5–6, 7 | неловкая → завораживающие движения |
| `wits` 1–7 | 1–2, 3–4, 5–6, 7 | коровий взгляд → гордый пронзительный взор |
| `maturity` 1–4 | 1, 2, 3, 4 | молодая женщина, женщина, зрелая женщина, взрослая фигуристая женщина |

Плюс состояние: тату, укрощена, мертва. Правило проверки: полосы идут по возрастанию, последняя `max` равна верхней границе поля, каждое значение диапазона попадает ровно в одну полосу. Перечислитель — диапазон поля из `STATS`, а не список в тесте.

## 4. Механики «Охотника»

### 4.1. Итог команды

Правила игры для общего обработчика после команды ([архитектура, § 5.4](GameplayArchitecture.md#54-итог-команды)):

```lua
-- rules/outcome.lua — правила «Охотника»
rules_of_game.death_cause = function(hunter)
    if dies_of_fatigue(hunter.fatigue) then return "tired" end      -- усталость > 7
    return nil                                                       -- «murdered» ставят эффекты: ретривер, совомедведь
end
rules_of_game.game_over_dialogue = "rh:dialogue.system.game_over"
rules_of_game.check_victory = function(scope)
    if reached_victory(player.gold) and not player.endless_mode then
        start_dialogue(scope, "rh:dialogue.system.victory")
    end
end

services.effect_die = { apply = function(ctx, p) ctx.scope.death = p.reason end }
```

Смерть важнее победы; победа при активном режиме ждёт его конца.

### 4.2. Фазы времени

Время суток — четыре периода: `morning → day → evening → night → morning`. Сдвиг — эффект `advance_time` ([архитектура, § 5.5](GameplayArchitecture.md#55-время)); на каждый сдвиг исполняются фазы:

```lua
-- фаза 100: голод охотника
services.phase_hunger = { run = function(ctx) player.hunger = player.hunger + 1 end }

-- фаза 200: голод питомцев
services.phase_pets = {
    run = function(ctx)
        local hunter = player
        if hunter:has(HOUND) then
            hunter.hound_hunger = hunter.hound_hunger + 1
            if hound_lost(hunter.hound_hunger) then
                hunter:take(HOUND, 1)
                hunter.hound_hunger = 0
                say(ctx, "pets.hound_lost")
            end
        end
        if hunter:has(RETRIEVER) then
            hunter.retriever_hunger = hunter.retriever_hunger + 1
            if retriever_turns(hunter.retriever_hunger) then
                hunter:take(RETRIEVER, 1)
                hunter.retriever_hunger = 0
                ctx.scope.death = "murdered"
            end
        end
    end,
}

services.effect_gain_fatigue = {
    apply = function(ctx) player.fatigue = player.fatigue + fatigue_gain(player.hunger) end,
}
```

Команду «Завершить ход» «Охотник» не объявляет.

### 4.3. Локации и переходы

| Локация | ID | Зона | Переходы |
|---|---|---|---|
| Площадь Рима | `rh:location.rome.square` | rome | магазин, лавка, рынок, Туманы (нужен кристалл) |
| Магазин Менхантера | `rh:location.rome.menhunter` | rome | площадь |
| Лавка | `rh:location.rome.butcher` | rome | площадь |
| Рынок | `rh:location.rome.market` | rome | площадь |
| Туманы | `rh:location.fog.mists` | fog | выход только блужданием или на коне |
| Лес | `rh:location.woods.forest` | woods | лагерь; Туманы — только с опушки, нужен кристалл |
| Лагерь | `rh:location.woods.camp` | camp | лес с той же глубиной |

Встреча с конём, охота и совомедведь — не локации, а сцены и режимы. Фон локации выбирает и публикует presenter `rh` по зоне, времени суток и глубине.

```lua
-- rules/travel.lua
-- площадь → Туманы: requires has_item nav_crystal
function enter_fog_from_rome(ctx)
    player.fog_destination = location("woods.forest")
    player:move_to(location("fog.mists"))
    fog_tick(ctx)
end

-- лес → Туманы: requires has_item nav_crystal, woods_depth_in [0]
function enter_fog_from_woods(ctx)
    player.fog_destination = location("rome.square")
    player:move_to(location("fog.mists"))
    fog_tick(ctx)
end

function leave_fog(ctx)
    local hunter = player
    local target = hunter.fog_destination
    if target == location("rome.square") then
        hunter:take_all(MEAT); hunter:take_all(CANNED_MEAT)   -- портятся
        hunter.fatigue, hunter.hunger = 0, 0                  -- вход в Рим
    else
        hunter.woods_depth = 0
    end
    hunter:move_to(target)
end

function fog_tick(ctx)
    local hunter = player
    hunter.fog_steps = hunter.fog_steps + 1
    if hunter.fog_steps == config.fog_horse_steps then
        hunter.fog_steps = 0
        if not hunter.ghost_horse_tamed and not hunter.ghost_horse_nearby then
            hunter.ghost_horse_nearby = true
            say(ctx, "fog.horse_heard")
        end
    end
end
```

Взаимодействия Туманов:

| Взаимодействие | Условия | Эффекты |
|---|---|---|
| Блуждать | — | `gain_fatigue`; `fog_tick`; `fog_exit_found` → `leave_fog`, иначе `say fog.lost`. Время не сдвигается |
| Спать | — | `set_fatigue 0` (одеяло не нужно); `advance_time`; `fog_tick` |
| «Лошаа-аадка!» | `flag ghost_horse_nearby` | `start_dialogue rh:dialogue.fog.ghost_horse`; исход — `ghost_horse_tamed = true`, `ghost_horse_nearby = false` |
| Сесть на коня | `flag ghost_horse_tamed` | `leave_fog` |

### 4.4. Лес

```lua
-- rules/woods.lua — эффект woods_walk {direction = +1 | -1}
services.effect_woods_walk = {
    check = function(ctx, p)
        local depth = player.woods_depth + p.direction
        if depth < 0 or depth > 3 then return reason("woods.edge") end
    end,
    apply = function(ctx, p)
        local hunter = player
        hunter.woods_depth = hunter.woods_depth + p.direction
        services.time.advance(ctx)
        local rng = stream("rh:random_stream.wilds")
        if not hunter:has(OWLBEAR_FEATHER) and owlbear_appears(hunter.woods_depth, rng) then
            services.effect_start_combat.apply(ctx, { combat = "rh:combat.owlbear" })   -- побег на этом шаге не проверяется
            return
        end
        services.catch.check_escape(ctx, config.escape_threshold.walk, "catch.escaped_walk")
    end,
}
```

| Взаимодействие | Условия | Эффекты |
|---|---|---|
| Дальше в лес | `woods_depth_in [0,1,2]` | `woods_walk +1` |
| Назад к опушке | `woods_depth_in [1,2,3]` | `woods_walk -1` |
| Выслеживать | `no_catch` | `start_hunt` (§ 4.5) |
| Использовать приманку | `item_count_at_least lure 1`, `no_catch` | `start_hunt {lure = true}` |
| Поставить ловушки | `woods_depth_in [0]`, `time_of_day_in [morning, day, evening]`, `has_item traps`, `flag traps_set = false` | `set_traps` |
| Проверить ловушки | `woods_depth_in [0]`, `time_of_day_in [morning]`, `no_catch`, `flag traps_set = true` | `check_traps` |
| Разбить лагерь | — | `travel camp` |
| В Туманы | `woods_depth_in [0]`, `has_item nav_crystal` | `enter_fog_from_woods` |

```lua
-- ловушки
services.effect_set_traps = { apply = function(ctx)
    player:take(TRAPS, 1)                            -- исчезают навсегда
    player.traps_set = true
    services.time.advance(ctx)
end }

services.effect_check_traps = { apply = function(ctx)
    if trap_caught(stream("rh:random_stream.wilds")) then
        local beast = services.catch.encounter(ctx, 0)
        beast.tattoo = false                         -- ловушка снимает тату
        services.catch.take(ctx, beast)
        say(ctx, "traps.caught")
    else
        say(ctx, "traps.empty")
    end
    services.time.advance(ctx)                       -- переустановка; traps_set остаётся true
end }
```

### 4.5. Охота

Охота — сцена [модуля диалога](GameplayArchitecture.md#512-диалог-rhmodedialogue). Поиск и проверка «заметила первой» идут до сцены: если дикарка заметила охотника, встречи нет и экземпляр не создаётся.

```lua
-- эффект start_hunt
services.effect_start_hunt = { apply = function(ctx, p)
    local hunter = player
    local hunt_rng = stream("rh:random_stream.hunt")
    local beast, scene
    if p.lure then                                    -- приманка: без усталости, поиска и «заметила»
        hunter:take(LURE, 1)
        services.time.advance(ctx)
        beast = services.catch.encounter(ctx, hunter.woods_depth)
        say(ctx, "hunt.lured")
    else
        hunter.fatigue = hunter.fatigue + fatigue_gain(hunter.hunger)
        services.time.advance(ctx)
        if not is_found(find_roll(hunter:has(HOUND), hunt_rng)) then return say(ctx, "hunt.nobody") end
        local values = generate_beast(hunter.woods_depth, stream("rh:random_stream.generation"))
        if beast_notices(values, hunter:stat("stealth"), hunt_rng) then return say(ctx, "hunt.noticed") end
        beast = services.catch.create(ctx, values)
        say(ctx, "hunt.unnoticed")
        scene = random_scene(hunt_rng)                -- без приманки: сцена обнаружения
    end
    services.effect_start_dialogue.apply(ctx, {
        dialogue = "rh:dialogue.hunt.encounter", subject = beast, vars = { scene = scene },
    })
end }
```

```json5
{
  id: "rh:dialogue.hunt.encounter",
  start: "stalking",
  subject: "argument",
  nodes: {
    stalking: {
      image: [
        { when: [{ type: "var_set", var: "scene" }], source: "hunt_scene" },
        { subject_image: "portrait" },
      ],
      text: [
        { when: [{ type: "var_set", var: "scene" }], source: "hunt_scene" },
        { text: "rh:text.dialogue.hunt.encounter.stalking.text" },    // с приманкой
      ],
      description: [{ when: [{ type: "var_is", var: "looked", value: true }], source: "subject_traits" }],
      choices: [
        { id: "look", effects: [{ type: "set_var", var: "looked", value: true }], next: "stalking" },
        { id: "sneak", check: { type: "beast_notices" },
          on_true: { say: "hunt.branch_snapped", next: "chase" },
          on_false: { say: "hunt.close", next: "fight" } },
        { source: "capture_methods", params: { stage: "stalking" } },
        { id: "leave", finish: "left" },
      ],
    },
    chase: {
      image: { subject_image: "portrait" },
      text: [{ text: "rh:text.dialogue.hunt.encounter.chase.text" }],
      choices: [
        { id: "pursue", effects: [{ type: "gain_fatigue" }, { type: "advance_time" }],
          check: { type: "beast_outruns" },
          on_true: { check: { type: "chance", one_in: 2 },
                     on_true: { finish: "lost" },
                     on_false: { say: "hunt.cant_catch_up", next: "chase" } },
          on_false: { say: "hunt.caught_up", next: "fight" } },
        { id: "leave", finish: "left" },
      ],
    },
    fight: {
      image: { subject_image: "portrait" },
      text: [{ text: "rh:text.dialogue.hunt.encounter.fight.text" }],
      choices: [
        { source: "capture_methods", params: { stage: "fight" } },
        { id: "leave", finish: "left" },
      ],
    },
  },
  outcomes: {
    captured_alive: [{ type: "take_subject", alive: true }],
    captured_dead:  [{ type: "take_subject", alive: false }],
    left:    [{ type: "discard_subject" }],
    escaped: [{ type: "discard_subject" }, { type: "say", key: "hunt.escaped" }],
    lost:    [{ type: "discard_subject" }, { type: "say", key: "hunt.lost" }],
  },
}
```

**Сцена обнаружения.** Сцена — Definition `hunt_scene`: текст подкрадывания и своя картинка. Она выбирается случайно из всех сцен при встрече без приманки и на шансы не влияет. С приманкой сцены нет: дикарка сама выходит к пузырьку, стадия показывает общий текст и портрет.

```json5
{ id: "rh:hunt_scene.drink", text: "rh:text.hunt.scene.drink", image: "rh:resource.hunt_scene.drink.image" }
```

```lua
function random_scene(rng)                              -- 12 сцен; бросок из потока охоты
    local scenes = all_definitions("hunt_scene")        -- GAP-010, порядок Stable ID
    return scenes[rng(1, #scenes)].id
end
services.text_hunt_scene  = { lines = function(ctx, run) return { def.hunt_scene(run.vars.scene).text } end }
services.image_hunt_scene = { resource = function(ctx, run) return def.hunt_scene(run.vars.scene).image end }
```

Сцены: `drink`, `s1`, `s2`, `sleep`, `apple`, `waterfall`, `hotspring`, `rabbit`, `butterfly`, `splinter`, `s3`, `chase_butterfly`.

Способы захвата — Definitions `capture_method`; источник `capture_methods` превращает их в варианты стадии. Принятый вариант: стадии как в источнике, каждая попытка тратит усталость, провал любого ближнего приёма ведёт в погоню.

```lua
services.choices_capture_methods = { list = function(ctx, run, p)
    local out = {}
    for _, m in ipairs(all_definitions("capture_method")) do  -- GAP-010
        if m.stage == p.stage then
            local cost = { { type = "gain_fatigue" } }
            if m.consumes then table.insert(cost, 1, { type = "take_item", item = m.consumes, amount = 1 }) end
            table.insert(out, {
                id = m.id, text = m.title, requires = m.requires, effects = cost,
                check = { type = "capture", method = m.id },
                on_true = { finish = (m.outcome == "alive") and "captured_alive" or "captured_dead" },
                on_false = (m.on_fail == "chase") and { say = "hunt.capture_failed", next = "chase" }
                                                   or { finish = "escaped" },
            })
        end
    end
    return out
end }

services.check_capture = { roll = function(ctx, run, p)
    local m = def.capture_method(p.method)
    return CAPTURE_CHECKS[m.check.type](run.subject, player, m.check, roller(run))   -- run-поток сцены
end }
services.check_beast_notices = { roll = function(ctx, run)
    return beast_notices(run.subject, player:stat("stealth"), roller(run))
end }
services.check_beast_outruns = { roll = function(ctx, run)
    return beast_outruns(run.subject, player:stat("pursuit"), roller(run))
end }
services.check_chance = { roll = function(ctx, run, p)
    return run.rng:next_int(1, p.one_in) == 1
end }
services.text_subject_traits = { lines = function(ctx, run)
    return beast_description(run.subject, all_definitions("beast_trait_scale"))
end }
```

| ID | Стадия | Требует | Расход | Проверка | Успех | Провал |
|---|---|---|---|---|---|---|
| `rh:capture_method.crossbow` | stalking | `has_item crossbow` | — | `always` | dead | — |
| `rh:capture_method.throwing_knives` | stalking | `class_trait throwing_knives` | — | `always` | dead | — |
| `rh:capture_method.net` | stalking | `class_trait net` | — | `roll_above_agility` (0…9) | alive | escape |
| `rh:capture_method.stone` | stalking | — | — | `roll_equals` (1…10 = 10) | alive | escape |
| `rh:capture_method.sleeping_gas` | stalking | `item_count_at_least gas 1` | gas | `always` | alive | — |
| `rh:capture_method.retriever` | stalking | `has_item retriever` | — | `always` | alive | — |
| `rh:capture_method.bear_hug` | fight | `class_trait bear_hug` | — | `agility_vs_attack` die 10 | alive | chase |
| `rh:capture_method.club` | fight | `has_item club` | — | `agility_vs_attack` die 10, +2 | alive | chase |
| `rh:capture_method.knife` | fight | `has_item knife` | — | `agility_vs_attack` die 6 | dead | chase |
| `rh:capture_method.taser` | fight | `has_item taser` | — | `always` | alive | — |
| `rh:capture_method.bare_hands` | fight | — | — | `agility_vs_attack` die 10 | alive | chase |

**Другие диалоги «Охотника».**

| Диалог | Узлы и особенности | Исходы |
|---|---|---|
| `rh:dialogue.intro.start` | выбор класса → ввод имени → предыстория класса → Вечный Рим, две части повествования | кристалл, записки, площадь Рима (§ 4.11) |
| `rh:dialogue.camp.inspect` | `subject: catch`; описание `subject_traits` и состояние | — |
| `rh:dialogue.fog.ghost_horse` | приручение | `ghost_horse_tamed = true` |
| `rh:dialogue.market.farid` | приветствие по полу охотника; вопросы «Самая дорогая дикарка?», «От чего зависит цена?»; «Продать добычу» | `trade` — магазин Фарида; любой исход — `met_farid = true` |
| `rh:dialogue.rome.menhunter` | приветствие; вопросы «Что взять в дорогу?», «Где продать добычу?» | — |
| `rh:dialogue.butcher.lessons` | обучение | `trade_skill_learned = true` |
| `rh:dialogue.misc.hunter_notes` | текст по зоне охотника: Рим, Туманы, лес, лагерь (`zone_in`) | — |
| `rh:dialogue.owlbear.peaceful` | мирная встреча Госпожи: «Отступить», «Погладить» | перо совомедведя |
| `rh:dialogue.system.game_over` | текст по причине (`var_is reason`); «Начать заново», «Продолжить тем же персонажем», «Закончить игру» → узел «Конец» (`terminal`) | `restart`: `restart_game`; `continue`: `revive` |
| `rh:dialogue.system.victory` | «Бесконечный режим» → завершение; «Закончить» → конечный узел «Конец» (`terminal`) | `endless_mode = true` |

Условия, которые вводят эти диалоги: `is_female` — класс охотника женский; `class_is` — класс охотника; `zone_in` — зона текущей локации (`rome`, `fog`, `woods`, `camp`). В варианты текста они входят как `when`, причина отказа им не нужна.


### 4.6. Добыча

```lua
-- rules/catch.lua
services.catch = {
    create = function(ctx, values)                    -- встреча: экземпляр и облик
        local looks = all_definitions("beast_look")   -- GAP-010
        values.beast_look = choose_look(values, looks, stream("rh:random_stream.generation"))
        return instances.create("actor", merge({ definition = def.actor("beast.wild") }, values))
    end,
    encounter = function(ctx, depth)
        return services.catch.create(ctx, generate_beast(depth, stream("rh:random_stream.generation")))
    end,
    take = function(ctx, beast) player.catch = beast end,
    discard = function(beast) instances.remove(beast) end,      -- GAP-018
    drop = function(ctx)
        local beast = player.catch
        player.catch = nil
        services.catch.discard(beast)
    end,
    check_escape = function(ctx, threshold, message)   -- сообщение: при ходьбе или во сне
        local hunter, beast = player, player.catch
        if beast == nil or not beast.alive then return end
        if hunter:has_trait("catch_never_escapes") or hunter:has(SHACKLES) then return end
        if escape_roll(beast, stream("rh:random_stream.wilds")) > threshold then
            services.catch.drop(ctx)
            say(ctx, message)
        end
    end,
}
```

Взаимодействия с добычей и едой доступны только в лагере:

| Взаимодействие | Условия | Эффекты |
|---|---|---|
| Осмотреть | `has_catch` | `start_dialogue rh:dialogue.camp.inspect` |
| Показать, кто главный | `class_trait subdue`, `catch_alive`, `catch_not_subdued` | `subdued = true`; база `stealth`, `attack`, `pursuit` +1; `set_fatigue 0` |
| Танец | `lacks_class_trait no_dance`, `catch_alive` | `start_mode dance {fashion = false}` |
| Танец (красивый) | `lacks_class_trait no_dance`, `catch_alive`, `catch_tattoo` | `start_mode dance {fashion = true}` |
| Зарезать | `has_item knife`, `catch_alive` | `alive = false` |
| Обокрасть | `has_item knife`, `catch_dead`, `flag trade_skill_learned` | тату → `give canned_meat 10`, иначе `give meat 10`; `drop` |
| Отпустить | `catch_alive` | `drop` |
| Выбросить труп | `catch_dead` | `drop` |
| Съесть припасы / мясо | `item_count_at_least <food> 1` | `take_item <food> 1`; `set_hunger 0` |
| Съесть консервы | `item_count_at_least canned 1` | `take_item canned 1`; `set_hunger 0`; `reduce_fatigue 1` |
| Покормить ищейку / ретривера | питомец и еда есть | `take_item <food> 1`; голод питомца = 0 |
| Отдыхать / спать | — | `rest` |

```lua
services.effect_rest = { apply = function(ctx)
    local hunter = player
    if world.time_of_day ~= "night" then
        hunter.fatigue = math.max(0, hunter.fatigue - 1)
        services.time.advance(ctx)
        services.catch.check_escape(ctx, config.escape_threshold.rest, "catch.escaped")
    else
        if hunter:has(BLANKET) then
            hunter.fatigue = 0; say(ctx, "camp.slept_well")
        else
            say(ctx, "camp.froze")                    -- усталость не меняется
        end
        services.time.advance(ctx)
        services.catch.check_escape(ctx, config.escape_threshold.night_sleep, "catch.escaped")
    end
end }
```

### 4.7. Совомедведь

Встреча — бой [модуля боя](GameplayArchitecture.md#513-бой-rhmodecombat): противник не ходит, каждое действие игрока сразу решает исход. Мирная встреча Госпожи — альтернатива боя, сцена диалога.

```json5
{
  id: "rh:combat.owlbear",
  enemies: [{ key: "owlbear", actor: "rh:actor.monster.owlbear" }],
  alternatives: [
    { when: [{ type: "class_trait", trait: "peaceful_owlbear" }], dialogue: "rh:dialogue.owlbear.peaceful" },
  ],
  intro: [{ text: "rh:text.combat.owlbear.intro" }],
  player_actions: [
    { id: "flee", check: { type: "chance", one_in: 2 },
      on_true: { finish: "died" }, on_false: { finish: "fled" } },
    { id: "fight", requires: [{ type: "has_item", item: "rh:item.weapon.sword" }],
      cost: [{ type: "gain_fatigue" }],
      check: { type: "owlbear_fight" }, on_true: { finish: "won" }, on_false: { finish: "died" } },
    { id: "drop_catch", requires: [{ type: "has_catch" }],
      cost: [{ type: "drop_catch" }], finish: "escaped" },
    { id: "sic_hound", requires: [{ type: "has_item", item: "rh:item.pet.hound" }],
      cost: [{ type: "take_item", item: "rh:item.pet.hound", amount: 1 }], finish: "escaped" },
    { id: "sic_retriever", requires: [{ type: "has_item", item: "rh:item.pet.retriever" }],
      cost: [{ type: "take_item", item: "rh:item.pet.retriever", amount: 1 }], finish: "escaped" },
    { id: "smoke_bomb", requires: [{ type: "item_count_at_least", item: "rh:item.potion.smoke_bomb", amount: 1 }],
      cost: [{ type: "take_item", item: "rh:item.potion.smoke_bomb", amount: 1 }], finish: "escaped" },
  ],
  enemy_behavior: { type: "none" },
  outcomes: {
    won:     [{ type: "give_item_once", item: "rh:item.trophy.owlbear_head" }],
    fled:    [{ type: "gain_fatigue" }, { type: "advance_time" }],
    escaped: [],
    died:    [{ type: "die", reason: "murdered" }],
  },
}
```

```lua
services.check_owlbear_fight = { roll = function(ctx, run)
    return owlbear_fight_won(player:has(CHAINMAIL), roller(run))
end }
```

Недоступные действия показываются неактивными с причиной: «Сражаться» без меча, «Бросить дикарку» без добычи и т. д.

### 4.8. Тоник, записки, птица-секретарь

| Взаимодействие | Условия | Эффекты |
|---|---|---|
| Выпить тоник | `item_count_at_least tonic 1` | `take_item tonic 1`; `set_fatigue 0` |
| Читать записки | `has_item hunter_notes` | диалог `rh:dialogue.misc.hunter_notes`: совет по текущей зоне |
| Птица-секретарь | `has_item secretary_bird` | `secretary_bird` |

Источник меню — панель инвентаря общего интерфейса в любой зоне; в режиме неактивны с причиной `mode.busy`.

```lua
services.effect_secretary_bird = { apply = function(ctx)
    local beast = player.catch
    if beast == nil then return say(ctx, "bird.describe") end
    local shop = butcher_price(beast, config.butcher_base_price)
    if beast.alive then
        say(ctx, "bird.estimate_alive", {
            market = market_estimate(beast, config.market_minimum_bonus),   -- без броска
            shop = shop,
        })
    else
        say(ctx, "bird.estimate_dead", { shop = shop })
    end
end }
```

### 4.9. Торговля

Три магазина — [модуль торговли](GameplayArchitecture.md#514-торговля-rhmodeshop). Формулы цены и источник ассортимента «Охотника»:

```lua
-- rules/trade_hunter.lua
services.price_butcher = {
    quote  = function(ctx, offer) return butcher_price(player.catch, config.butcher_base_price) end,
    settle = function(ctx, offer) return butcher_price(player.catch, config.butcher_base_price) end,
}
services.price_market = {
    quote  = function(ctx, offer) return market_estimate(player.catch, config.market_estimate_bonus) end,
    settle = function(ctx, offer) return market_price(player.catch, roller(ctx.run)) end,   -- run-поток магазина
}

-- динамический ассортимент: все предметы категории, условия покупки — из Definition предмета
services.offers_items_by_category = { list = function(ctx, run, p)
    local out = {}
    for _, item in ipairs(all_definitions("item")) do       -- GAP-010
        if item.category == p.category and item.price then
            table.insert(out, { kind = "buy", item = item.id, requires = item.buy_requires })
        end
    end
    return out
end }
```

```json5
{
  id: "rh:shop.rome.menhunter",
  tabs: [
    { id: "tools",       offers: [{ source: "items_by_category", params: { category: "tools" } }] },
    { id: "weapons",     offers: [{ source: "items_by_category", params: { category: "weapons" } }] },
    { id: "clothes",     offers: [{ source: "items_by_category", params: { category: "clothes" } }] },
    { id: "pets",        offers: [{ source: "items_by_category", params: { category: "pets" } }] },
    { id: "consumables", offers: [{ source: "items_by_category", params: { category: "consumables" } }] },
    { id: "sell",        offers: [{ kind: "sell", item: "rh:item.trophy.owlbear_head", price: { base: 50 } }] },
  ],
}
```

Условия покупки живут в Definition предмета (`buy_requires`): ловушки — `flag traps_set = false`; кольчуга — `lacks_class_trait refuses_chainmail`. Уникальный предмет второй раз не покупается — общее правило модуля.

| Магазин | Вкладки и предложения | Действия |
|---|---|---|
| `rh:shop.rome.butcher` (лавка) | `sell`: `subject: catch`, `price: { formula: "butcher" }` — живая и мёртвая | «Обучение»: `requires flag trade_skill_learned = false`, `start_dialogue rh:dialogue.butcher.lessons` |
| `rh:shop.rome.farid` (рынок) | `sell`: `subject: catch`, `requires catch_alive` (причина `market.dead_not_accepted`), `price: { formula: "market" }` | — |

Вход в магазины Менхантера и лавки — взаимодействия локаций с эффектом `start_shop`. С Фаридом торговля идёт только через разговор: на рынке одно взаимодействие «Подойти к Фариду» запускает `rh:dialogue.market.farid`, вариант «Продать добычу» (`requires has_catch`) завершает его исходом `trade`, который запускает магазин Фарида — завершение одного режима и старт другого в одной команде ([ADR-0051](../ADR/0051-temporary-modes-and-presentation-routing.md)). В магазине Менхантера, кроме «Торговать», есть «Поговорить» — диалог `rh:dialogue.rome.menhunter`.


### 4.10. Танец (режим `rh:mode.dance`)

Один режим на оба входа: «Танец» и «Танец (красивый)». Ядро — § 3.7. Каждая команда — один шаг цикла источника: действие, затем проверка порогов. Итог применяется при выходе из режима любым путём.

```lua
-- modes/dance.lua
modes.dance = {
    start = function(args)
        return {
            core = dance_start(args.fashion),
            started_as_fashion = args.fashion,
            core_interface = true,                    -- танец показывает общий интерфейс всегда
            group = nil,                              -- открытое подменю: soft | hard
            result = { fatigue_reset = false, stat_bonus = false },
        }
    end,
    view = function(run) return dance_view(run) end,  -- сцена с участниками (§ 6)
}

-- run-поток режима (ADR-0051): modes.dance.rng = field.random_stream()
local function roller(run) return function(a, b) return run.rng:next_int(a, b) end end

local function after_step(run)                        -- DanceLoop, шаг 1
    run.group = nil
    if run.core.fashion then return end
    local event = dance_check(run.core)
    if event == "beast_finish" then
        local r = dance_beast_finish(run.core)
        local beast = player.catch
        run.result.fatigue_reset = true
        if r.first and beast.tattoo and not beast.dance_bonus_given then
            run.result.stat_bonus = true              -- один раз на дикарку
        end
    elseif event == "finish" then
        run.must_finish = true                        -- источник сразу уходит в Finish
    end
end

local function apply_result(run, finished)
    local hunter, beast = player, player.catch
    if run.result.fatigue_reset then hunter.fatigue = 0 end
    if run.result.stat_bonus then
        hunter.stealth = hunter.stealth + 1
        hunter.attack = hunter.attack + 1
        hunter.pursuit = hunter.pursuit + 1
        beast.dance_bonus_given = true
    end
    if finished then
        hunter.fatigue = 0
        if not run.started_as_fashion then beast.tattoo = false end   -- «Танец (красивый)» тату не снимает
    end
end

commands["dance.group"] = function(group)              -- открыть подменю или вернуться (nil)
    modes.dance:require_active().group = group
end

commands["dance.move"] = function(move)                -- pause | start | softer | keep | harder
    local run = modes.dance:require_active()
    require_available(dance_actions(run.core), move)
    dance_move(run.core, move, roller(run))
    after_step(run)
end

commands["dance.soft"] = function(n)
    local run = modes.dance:require_active()
    require_available(dance_actions(run.core), "soft")
    dance_soft(run.core, n, roller(run))
    after_step(run)
end

commands["dance.hard"] = function(n)
    local run = modes.dance:require_active()
    require_available(dance_actions(run.core), "hard")
    dance_hard(run.core, n, roller(run))
    after_step(run)
end

commands["dance.finish"] = function(choice)            -- bow | flourish | fashion
    local run = modes.dance:require_active()
    require(not run.core.fashion and (run.must_finish or dance_actions(run.core).finish))
    dance_finish(run.core, choice)
    if run.core.fashion then return end               -- 87: дальше цикл fashion
    apply_result(run, true)
    modes.dance:finish({ outcome = choice, text = run.core.text })
end

commands["dance.fashion_act"] = function(act)          -- begin | hold | close_in | move_fast
    local run = modes.dance:require_active()
    require_available(dance_actions(run.core), act)
    fashion_act(run.core, act)
end

commands["dance.fashion_finish"] = function(where)     -- close | far
    local run = modes.dance:require_active()
    require_available(dance_actions(run.core), "finish_" .. where)
    fashion_finish(run.core, where)
    apply_result(run, true)
    modes.dance:finish({ outcome = "fashion_" .. where, text = run.core.text })
end

commands["dance.kill"] = function()                    -- после текста 651
    local run = modes.dance:require_active()
    require(run.core.offer_kill)
    apply_result(run, false)
    player.catch.alive = false
    modes.dance:finish({ outcome = "killed" })
end

commands["dance.leave"] = function()
    local run = modes.dance:require_active()
    require(not run.must_finish)
    apply_result(run, false)
    modes.dance:finish({ outcome = "left" })
end
```

### 4.11. Старт, конец игры и победа

Все три сцены — диалоги ([модуль диалога](GameplayArchitecture.md#512-диалог-rhmodedialogue)); отдельных режимов нет.

**Старт.** Главного меню нет: при запуске сразу начинается новая игра. Порядок: выбор класса → ввод имени → предыстория класса → Вечный Рим (две части) → кристалл, записки и площадь Рима. Старый охотник к началу игры умер; вступление — повествование, а не разговор.

```lua
commands["game.start"] = function()                   -- мост старта (GAP-015)
    instances.create("actor", {
        definition = def.actor("player.hunter"), is_player = true,
        current_location = nil,                       -- локация появляется после вступления
        gold = config.start_gold,                     -- стартовый инвентарь — только золото
    })
    world.time_of_day = "morning"
    services.effect_start_dialogue.apply(context(player), { dialogue = "rh:dialogue.intro.start" })
end

services.effect_set_class = { apply = function(ctx, p)
    local class = def.hunter_class(p.class)
    player.hunter_class = class
    player.stealth, player.attack, player.pursuit = class.stealth, class.attack, class.pursuit
end }

services.effect_set_name = { apply = function(ctx, p) player.display_name = p.value end }
services.input_name = { check = function(value)                -- 1…40 символов, не пустое после trim
    if not valid_name(value) then return reason("intro.bad_name") end
end }
```

```json5
{
  id: "rh:dialogue.intro.start",
  start: "class",
  core_interface: false,
  nodes: {
    class: {
      text: [{ text: "rh:text.intro.choose_hero" }],
      choices: [
        { id: "trapper",   effects: [{ type: "set_class", class: "rh:hunter_class.trapper" }],   next: "name" },
        { id: "assassin",  effects: [{ type: "set_class", class: "rh:hunter_class.assassin" }],  next: "name" },
        { id: "barbarian", effects: [{ type: "set_class", class: "rh:hunter_class.barbarian" }], next: "name" },
        { id: "mistress",  effects: [{ type: "set_class", class: "rh:hunter_class.mistress" }],  next: "name" },
      ],
    },
    name: {
      text: [
        { when: [{ type: "is_female" }], text: "rh:text.dialogue.intro.start.name.placeholder_heroine" },
        { text: "rh:text.dialogue.intro.start.name.placeholder_hero" },
      ],
      input: { kind: "text", validate: "name", effect: "set_name", next: "history" },
    },
    history: {
      text: [
        { when: [{ type: "class_is", class: "rh:hunter_class.trapper" }],   text: "rh:text.intro.history.trapper" },
        { when: [{ type: "class_is", class: "rh:hunter_class.assassin" }],  text: "rh:text.intro.history.assassin" },
        { when: [{ type: "class_is", class: "rh:hunter_class.barbarian" }], text: "rh:text.intro.history.barbarian" },
        { text: "rh:text.intro.history.mistress" },
      ],
      choices: [{ id: "next", next: "rome_1" }],
    },
    rome_1: { text: [{ text: "rh:text.intro.rome1" }], choices: [{ id: "next", next: "rome_2" }] },
    rome_2: { text: [{ text: "rh:text.intro.rome2" }], choices: [{ id: "next", finish: "done" }] },
  },
  outcomes: {
    done: [
      { type: "give_item", item: "rh:item.misc.nav_crystal" },
      { type: "give_item", item: "rh:item.misc.hunter_notes" },
      { type: "travel", location: "rh:location.rome.square" },
    ],
  },
}
```

Игрок без локации до конца вступления допустим, только если это разрешает движок; иначе игрок создаётся сразу на площади Рима, а экран локации закрыт диалогом до конца вступления.

**Конец игры.** Обработчик после команды (§ 4.1) при смерти завершает активный режим и запускает `rh:dialogue.system.game_over` с переменной `reason`. Экран: текст по причине и три варианта.

- «Начать заново» — эффект `restart_game`, перезапуск игры из Lua ([GAP-015](../Status/EngineGaps.md#gap-015-старт-и-перезапуск-новой-игры-из-authoring)). Новая игра обязана получить новое зерно случайности, иначе повторит броски прошлой.
- «Продолжить тем же персонажем» — эффект `revive`: охотник просыпается в Вечном Риме (текст `rh:text.system.game_over.continue_msg`).
- «Закончить игру» — конечный узел «Конец» без вариантов, как у победы.

```lua
services.effect_revive = { apply = function(ctx)
    local hunter = player
    if hunter.catch ~= nil then services.catch.drop(ctx) end     -- добыча потеряна
    hunter.fatigue, hunter.hunger = 0, 0
    hunter.fog_steps = 0
    hunter:move_to(location("rome.square"))                     -- золото, предметы и характеристики сохраняются
end }
```

Смерть от ретривера уже забрала ретривера; смерть в бою с совомедведем ничего не забирает, кроме добычи.

**Победа.** Обработчик после команды запускает `rh:dialogue.system.victory`, когда активного режима нет. «Бесконечный режим» — эффект `set_flag endless_mode = true` и завершение; «Закончить» — переход в конечный узел «Конец» без вариантов.

## 5. Случайность

Вне режима броски идут из потоков подсистем; внутри режима — из run-потока этого режима ([ADR-0051](../ADR/0051-temporary-modes-and-presentation-routing.md)).

| Поток | Что бросает |
|---|---|
| `rh:random_stream.generation` | генерация дикарки, выбор облика |
| `rh:random_stream.hunt` | поиск дикарки, проверка «заметила первой» и выбор сцены обнаружения — до начала сцены охоты |
| `rh:random_stream.wilds` | появление совомедведя, выход из Туманов, ловушки, побег добычи |
| run-поток сцены охоты | подкрасться, погоня, потеря в погоне, захват |
| run-поток боя | бегство и схватка с совомедведем |
| run-поток магазина | случайная часть цены Фарида |
| run-поток танца | шаги танца |

## 6. Экраны

Экран собирается по [архитектуре, § 7](GameplayArchitecture.md#7-экран): общий интерфейс плюс экран локации или режима. Шаблоны экранов готовятся отдельно.

### 6.1. Общий интерфейс: значения «Охотника»

| Панель | Поля | Источник |
|---|---|---|
| Статус | имя, класс, золото (всегда), время суток, глубина леса | охотник, мир |
| Состояние | усталость — шкала с порогом смерти; голод — текст состояния | охотник |
| Добыча | миниатюра портрета, жива или мертва, тату | роль `catch` |
| Инвентарь | предметы с количеством; действия тоника, птицы-секретаря, записок с доступностью и причиной | инвентарь, взаимодействия с источником «инвентарь» |
| Последнее событие | сообщения последней команды | область команды ([архитектура, § 5.4](GameplayArchitecture.md#54-итог-команды)) |

Показ общего интерфейса во время режима выбирает режим ([архитектура, § 5.11](GameplayArchitecture.md#511-временные-режимы)); танец показывает его всегда.

| Сцена | `core_interface` |
|---|---|
| Вступление `rh:dialogue.intro.start` | `false` — класса и имени ещё нет |
| Остальные диалоги, бой, магазины, танец | `true` |

Для конца игры и победы выбор — данные их диалогов; дизайнер может скрыть интерфейс.

### 6.2. Экраны режимов

| Сцена | Экран |
|---|---|
| Охота, вступление, осмотр добычи, приручение коня, разговоры с Фаридом и Менхантером, обучение, записки, мирная встреча с совомедведем, конец игры, победа | экран диалога |
| Совомедведь, танец | сцена с участниками |
| Менхантер, лавка, Фарид | магазин |

## 7. Ресурсы с номерами источника

Полный перечень картинок и текстов игры с ID — [Resource Requests](../Status/ResourceRequests.md).

### 7.1. Облики из старой таблицы портретов

Базовые облики (без условий): `rh:beast_look.<hair>_<NN>`, `NN` = зрелость 01…04, портрет `rh:resource.beast.portrait.<hair>_<NN>` (источник: `beast_blond_01…04`, `beast_black_01…04`, `beast_red_01…04`; `brunette` → `black` в имени ресурса). Особые облики:

| Облик и портрет | Цвет | Зрелость | `when` | Источник |
|---|---|---|---|---|
| `blond_01_perfect` | blond | 1 | figure 6–7 | `beast_blond_01_perfect` |
| `blond_02_strong` | blond | 2 | figure 5–7 | `beast_blond_02_strong` |
| `blond_02_foxy` | blond | 2 | wits 5–6 | `beast_blond_02_foxy` |
| `blond_02_proud` | blond | 2 | wits 7 | `beast_blond_02_proud` |
| `blond_03_strong` | blond | 3 | figure 5–7 | `beast_blond_03_strong` |
| `blond_03_foxy` | blond | 3 | wits 6–7 | `beast_blond_03_foxy` |
| `blond_04_perfect` | blond | 4 | figure 7 | `beast_blond_04_perfect` |
| `blond_04_foxy` | blond | 4 | wits 5–7 | `beast_blond_04_foxy` |
| `black_02_foxy` | brunette | 2 | wits 6 | `beast_black_02_foxy` |
| `black_02_perfect` | brunette | 2 | figure 7 | `beast_black_02_perfect` |
| `black_03_strong` | brunette | 3 | figure 6–7 | `beast_black_03_strong` |
| `black_03_perfect` | brunette | 3 | figure 7 | `beast_black_03_perfect` |
| `black_03_foxy` | brunette | 3 | wits 6 | `beast_black_03_foxy` |
| `black_03_proud` | brunette | 3 | wits 7 | `beast_black_03_proud` |
| `black_03_cow` | brunette | 3 | wits 1–3 | `beast_black_03_cow` |
| `black_04_afraid` | brunette | 4 | wits 5 | `beast_black_04_afraid` |
| `black_04_cow` | brunette | 4 | wits 1–2 | `beast_black_04_cow` |
| `black_04_proud` | brunette | 4 | wits 7, figure 7 | `beast_black_04_proud` |
| `red_01_cute` | red | 1 | looks 5–6 | `beast_red_01_cute` |
| `red_02_foxy` | red | 2 | wits 6–7 | `beast_red_02_foxy` |
| `red_03_foxy` | red | 3 | wits 6–7 | `beast_red_03_foxy` |
| `red_04_perfect` | red | 4 | looks 6 | `beast_red_04_perfect` |
| `red_04_cow` | red | 4 | figure 1–3 | `beast_red_04_cow` |

Новое правило отличается от старого «последнее совпадение побеждает» там, где подходят два облика с одним условием (например, блондинка 2 с `figure 6` и `wits 5`: `strong` или `foxy`). Теперь выбор между ними случайный при создании и сохраняется в экземпляре.

### 7.2. Запасные картинки сцен и состояний

Используются, если у облика нет своей картинки:

| Назначение | ID | Выбор | Источник |
|---|---|---|---|
| Танец | `rh:resource.beast.dance.dance_01…04` | по зрелости | `dance_01…04` |
| Fashion | `rh:resource.beast.fashion.blond`, `.black`, `.red` | по цвету | `fashion_blond/black/red` |
| Мёртвая | `rh:resource.beast.corpse.corpse01…04` | по зрелости 1…4 | `corpse01…03` + новая `corpse04` |

### 7.3. Тексты танца по номерам источника

`rh:text.dance.step_<N>`:

| Группа | Номера |
|---|---|
| Пауза, начало | 1, 21 |
| Нежнее | 22, 23 |
| Держать темп | 31, 301, 32, 302, 33, 303 |
| Жёстче | 41, 42 |
| Мягкие действия | 51, 511, 52, 521, 53, 531, 532, 54, 541, 55, 551, 552, 553 |
| Жёсткие действия | 61, 62, 63, 631, 64, 65, 651 |
| Финалы обычного танца | 71, 72, 87 |
| Fashion | 81, 82, 83, 84, 85, 86, 91, 92 |

### 7.4. Прочие тексты

Описания черт — по шкалам § 3.8; состояния голода `rh:text.hunger.fed|hungry|very_hungry|famished|exhausted`; сообщения `rh:text.message.<key>` (ключи `say` в § 4); причины отказа `rh:text.error.<key>` (§ 3.7, § 4.9); названия действий танца `rh:text.dance.action.<id>`.

### 7.5. Хранение текстов

Формат — [ADR-0022](../ADR/0022-external-translation-catalog.md). Язык авторинга «Охотника» — русский:

```text
GameData/rh/
  definitions/texts_hunter_<область>.json5   определения kind text; source_message — русская строка
  localization/en.po                         английский перевод; msgctxt — text_id
```

- Область — `system`, `intro`, `characters`, `beast`, `world`, `hunt`, `combat`, `dialogues`, `shops`, `messages`, `interactions`, `dance`, `errors`, `core_interface`.
- Вариант текста — отдельный `text_id`; выбор между вариантами делает правило или условие данных, а не разбор строки. Например, побег добычи — `catch.escaped` при отдыхе и сне, `catch.escaped_walk` при ходьбе.
- Параметры — `{имя}`, окончание по числу — `{n}|plural(…)` с формами своего языка, выделение — `<style=emphasis>…</>`. Токен `emphasis` объявляет тема ([заявка](../Status/ResourceRequests.md#26-токены-темы)).
- Набор параметров и тегов в переводе совпадает с исходной строкой.

## 8. Проверка

- Specs чистых ядер: генерация на каждой глубине (границы диапазонов); цены (максимум 650, таблица лавки, оценки 24 и 0); выбор облика — точность и случайность при равенстве; каждая ветвь `dance_move`, `dance_soft`, `dance_hard` по порогам источника, включая уход в минус и жёсткое 5 при `beast_engagement ≤ 0`; доступность в `dance_actions` и `fashion_actions`; финалы 71, 72, 87, 81, 82.
- Покрытие данных с перечислителем из модели: базовый облик для каждой пары «цвет × зрелость» (enum `hair` × диапазон `maturity`); шкала каждой черты покрывает весь диапазон поля из `STATS`.
- Specs механик: смерть при `fatigue = 8`, но не при 7; `fatigue_gain` при голоде 6, 7, 8; потеря ищейки при 8; смерть от ретривера при 9; совомедведь не появляется с пером; добыча не убегает в оковах и у Госпожи, но на охоте Госпожа замечается как все; встреча без поимки не оставляет экземпляра; бонус танца даётся один раз на дикарку; fashion не снимает тату; ботинки и плащ меняют `stat("stealth")`, но не базу; «Начать заново» начинает новую игру с выбора класса; кристалл и записки появляются только в конце вступления; «Продолжить тем же персонажем» теряет добычу и оставляет золото; сцена охоты не меняет шансов (одинаковые броски при разных сценах); сохранение отклоняется в режиме.
- Specs модулей на синтетических Definitions, а не на контенте «Охотника»: диалог — варианты текста и описания по условиям, скрытый и неактивный вариант, вложенные переходы, переменные, динамические варианты, ввод, конечный узел; бой — альтернатива, затраты применяются только при завершении, урон и поражение по здоровью, поведение противника; торговля — источник ассортимента, модификаторы цены, `quote` и `settle`, продажа персонажа, запас, действие магазина.
- Эталонные прогоны: выбор класса → имя → предыстория → Вечный Рим → Туманы → охота → лагерь → танец → разговор с Фаридом и продажа; смерть от усталости → «Начать заново»; смерть → «Продолжить тем же персонажем»; приручение коня через 10 шагов в Туманах; победа → бесконечный режим; победа → «Конец».
- Перечислители: Definitions `capture_method`, `hunt_scene`, `beast_look`, `beast_trait_scale`, `time_phase`, `interaction` — обход repository; все команды `rh` проходят через обработчик после команды — это гарантирует механизм движка ([GAP-019](../Status/EngineGaps.md#gap-019-обработчик-после-команды-и-область-команды)).

## 9. Открытые вопросы

- Тексты fashion 91 и 92 и финал 87 обычного танца, переводящий в цикл fashion: в ядре они стоят там же, где в исходном проекте (§ 3.7); их смысл уточняет дизайнер.
- Шаблоны экранов диалога, магазина, сцены с участниками, общего интерфейса и модального окна ввода готовятся отдельно.
