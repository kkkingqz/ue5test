---
title: Resource Requests
status: informative
version: 0.3
updated: 2026-10-18
depends_on:
  - ../RH/HunterGameplay.md
  - ../RH/GameplayArchitecture.md
---

# Заявки на ресурсы

> **Показывает:** картинки и тексты, которые нужны игре «Охотник» пакета `rh`, с их Stable ID, назначением и номерами исходного проекта; документ описывает потребность и не вводит новых правил.

Правила игры и места использования — [Охотник: правила игры](../RH/HunterGameplay.md); форма ID — [архитектура геймплея RH, § 4.4](../RH/GameplayArchitecture.md#44-соглашения-об-id). Шаблоны экранов готовятся отдельно и сюда не входят.

Колонка «Основание»: **док** — ресурс или ключ уже назван в правилах игры; **нужно** — следует из правил, но ID в правилах не записан и предлагается здесь; **решить** — нужен выбор дизайнера. Номер в колонке «Источник» — имя или номер ресурса исходного проекта.

## 1. Сводка

| Группа | Картинок | Текстов |
|---|---|---|
| Дикарки | 46 | 36 |
| Персонажи, классы, сцены | 16 | — |
| Сцены охоты | 12 | 12 |
| Локации | 7 обязательных, до 31 с вариантами | 14 |
| Предметы | 25 | 50 |
| Классы | — | 12 |
| Взаимодействия | — | 40 |
| Охота | — | 20 |
| Бой с совомедведем | — | 12 |
| Диалоги и вступление | — | 52 |
| Магазины и имена NPC | — | 14 |
| Танец | — | 88 |
| Сообщения | — | 23 обязательных + 18 предложенных |
| Причины отказа | — | 23 |
| Голод и время суток | — | 9 |
| Общий интерфейс | — | 20 |
| **Итого** | **99 обязательных** (до 130) | **425 + 18 предложенных** |

## 2. Графические ресурсы

### 2.1. Дикарки

Портреты — по [облику](../RH/HunterGameplay.md#36-облик-дикарки). Один портрет используется и для крупного показа, и для миниатюры на панели добычи.

| Назначение | ID | Шт. | Источник | Основание |
|---|---|---|---|---|
| Базовые портреты | `rh:resource.beast.portrait.blond_01…04` | 4 | `beast_blond_01…04` | док |
| | `rh:resource.beast.portrait.black_01…04` | 4 | `beast_black_01…04` | док |
| | `rh:resource.beast.portrait.red_01…04` | 4 | `beast_red_01…04` | док |
| Особые портреты | 23 облика из [таблицы § 7.1](../RH/HunterGameplay.md#71-облики-из-старой-таблицы-портретов) | 23 | `beast_<облик>` | док |
| Танец | `rh:resource.beast.dance.dance_01…04` | 4 | `dance_01…04` | док |
| Fashion | `rh:resource.beast.fashion.blond`, `.black`, `.red` | 3 | `fashion_blond/black/red` | док |
| Мёртвая | `rh:resource.beast.corpse.corpse01…03` | 3 | `corpse01…03` | док |
| | `rh:resource.beast.corpse.corpse04` | 1 | — | док, **новая картинка** для зрелости 4 |

Особые облики могут получить свои картинки танца, fashion и трупа (`images.dance`, `.fashion`, `.corpse`); без них берутся запасные из этой таблицы. Сейчас таких картинок нет.

### 2.2. Персонажи и классы

| Назначение | ID | Где видно | Основание |
|---|---|---|---|
| Охотник-Ловчий | `rh:resource.hunter_class.trapper.portrait` | выбор класса, общий интерфейс, сцена танца и боя | нужно |
| Охотник-Ассасин | `rh:resource.hunter_class.assassin.portrait` | то же | нужно |
| Охотник-Варвар | `rh:resource.hunter_class.barbarian.portrait` | то же | нужно |
| Госпожа | `rh:resource.hunter_class.mistress.portrait` | то же | нужно |
| Менхантер | `rh:resource.actor.npc.menhunter.portrait` | магазин, разговор | нужно |
| Мясник | `rh:resource.actor.npc.butcher.portrait` | лавка, обучение | нужно |
| Фарид | `rh:resource.actor.npc.farid.portrait` | разговор, магазин | нужно |
| Совомедведь | `rh:resource.actor.monster.owlbear.portrait` | бой, мирная встреча | нужно |

Портреты классов — своя система портретов «Охотника», а не перенос из источника. Старый охотник к началу игры умер и во вступлении не появляется; его портрет не нужен.

### 2.3. Картинки сцен

| Сцена | ID | Основание |
|---|---|---|
| Вступление: выбор класса, имя, предыстория, Вечный Рим | `rh:resource.dialogue.intro.start.background` | нужно; решить, нужны ли отдельные картинки предысторий и Рима |
| Конь-призрак | `rh:resource.dialogue.fog.ghost_horse.image` | нужно |
| Бой с совомедведем | `rh:resource.combat.owlbear.background` | нужно |
| Мирная встреча Госпожи с совомедведем | `rh:resource.dialogue.owlbear.peaceful.image` | нужно |
| Записки охотника | `rh:resource.dialogue.misc.hunter_notes.image` | решить: нужна ли картинка |
| Конец игры: усталость | `rh:resource.dialogue.system.game_over.tired` | нужно |
| Конец игры: убит | `rh:resource.dialogue.system.game_over.murdered` | нужно |
| Победа и экран «Конец» | `rh:resource.dialogue.system.victory.image` | нужно |

Осмотр добычи, погоня, схватка и танец используют картинки дикарки (§ 2.1).

**Сцены охоты.** У каждой из 12 сцен обнаружения своя картинка `rh:resource.hunt_scene.<сцена>.image`: `drink`, `s1`, `s2`, `sleep`, `apple`, `waterfall`, `hotspring`, `rabbit`, `butterfly`, `splinter`, `s3`, `chase_butterfly` — 12 шт., основание — док ([правила, § 4.5](../RH/HunterGameplay.md#45-охота)). Сцена выбирается случайно и показывается на стадии подкрадывания; с приманкой сцены нет — показывается портрет дикарки.

### 2.4. Фоны локаций

Фон выбирает presenter `rh` по зоне, времени суток и глубине леса ([§ 4.3](../RH/HunterGameplay.md#43-локации-и-переходы)).

| Локация | Обязательно | Полный набор | Основание |
|---|---|---|---|
| Площадь Рима | `rh:resource.location.rome.square.background` | + `background_morning`, `_day`, `_evening`, `_night` | нужно |
| Магазин Менхантера | `rh:resource.location.rome.menhunter.background` | — (помещение) | нужно |
| Лавка | `rh:resource.location.rome.butcher.background` | — | нужно |
| Рынок | `rh:resource.location.rome.market.background` | + 4 времени суток | нужно |
| Туманы | `rh:resource.location.fog.mists.background` | — (время в Туманах не видно) | нужно |
| Лес | `rh:resource.location.woods.forest.background` | `background_depth0…3` × 4 времени суток = 16 | нужно |
| Лагерь | `rh:resource.location.woods.camp.background` | + 4 времени суток | нужно |

Решить: какой набор делать сразу. Правило выбора с запасным фоном (нет варианта по времени → общий фон локации) войдёт в presenter вместе с выбранным набором.

### 2.5. Иконки предметов

Иконка — `rh:resource.item.<группа>.<имя>.icon` для каждого предмета [§ 2.8](../RH/HunterGameplay.md#28-предметы): 25 шт. Используются на панели инвентаря и в магазине.

| Группа | Предметы |
|---|---|
| `tool` | `blanket`, `shackles`, `traps` |
| `weapon` | `knife`, `club`, `crossbow`, `sword`, `taser` |
| `clothes` | `soft_boots`, `chainmail`, `elven_cloak` |
| `pet` | `secretary_bird`, `hound`, `retriever` |
| `food` | `supplies`, `meat`, `canned_meat` |
| `potion` | `tonic`, `smoke_bomb`, `lure`, `sleeping_gas` |
| `trophy` | `owlbear_head`, `owlbear_feather` |
| `misc` | `nav_crystal`, `hunter_notes` |

### 2.6. Токены темы

| Токен | Вид | Где используется | Основание |
|---|---|---|---|
| `emphasis` | `TextStyleTokens` темы | выделение в описаниях классов: `<style=emphasis>…</>` | нужно; без токена текст не отрисуется; реализует [EGC-20C](../Plans/EngineGapClosure/PresentationAndUi.md#egc-20c-токен-выделения-в-теме) |

## 3. Тексты

Формат хранения — [правила игры, § 7.5](../RH/HunterGameplay.md#75-хранение-текстов): исходная строка русская, английский — перевод в `en.po`.


### 3.1. Дикарка: описание черт

Шкалы — [§ 3.8](../RH/HunterGameplay.md#38-описание-черт). Подпись черты `rh:text.beast.trait.<черта>`, текст полосы `rh:text.beast.<черта>.<полоса>`.

| Черта | Подпись | Полосы (смысл) | Текстов |
|---|---|---|---|
| `looks` | Внешность | `attractive`, `pretty`, `beautiful`, `dazzling` | 1 + 4 |
| `height` | Рост | миниатюрная, обычная, высокая | 1 + 3 |
| `hair` | Волосы | блондинка, брюнетка, рыжая | 1 + 3 |
| `figure` | Фигура | 4 полосы: пухлая → совершенное тело | 1 + 4 |
| `agility` | Ловкость | 4 полосы: неловкая → завораживающие движения | 1 + 4 |
| `wits` | Взгляд | 4 полосы: коровий взгляд → гордый пронзительный взор | 1 + 4 |
| `maturity` | Возраст | молодая женщина, женщина, зрелая женщина, взрослая фигуристая женщина | 1 + 4 |
| Состояние | — | `rh:text.beast.state.tattoo`, `.subdued`, `.dead` | 3 |

Итого 36. Имена полос, кроме `looks`, выбираются при написании текстов; правило — полосы покрывают весь диапазон черты.

### 3.2. Классы и выбор персонажа

| Текст | ID | Шт. | Основание |
|---|---|---|---|
| Название класса | `rh:text.hunter_class.<класс>.title` | 4 | док (тексты названия) |
| Сложность | `rh:text.hunter_class.<класс>.difficulty` | 4 | док |
| Описание класса и его черт | `rh:text.hunter_class.<класс>.description` | 4 | нужно: черты `net`, `throwing_knives`, `bear_hug`, `subdue` и ограничения Госпожи |

### 3.3. Локации

`rh:text.location.<группа>.<имя>.title` и `.description` для 7 локаций — 14 текстов. Решить: нужны ли описания леса по глубине (`description_depth0…3`) и названия глубин для панели статуса (§ 3.14).

### 3.4. Предметы

`rh:text.item.<группа>.<имя>.title` и `.description` для 25 предметов — 50 текстов. В описании указывается действие предмета: модификатор скрытности, расход, ограничение для Госпожи.

### 3.5. Взаимодействия

Название пункта меню — `rh:text.interaction.<группа>.<имя>.title`. ID взаимодействий в правилах не записаны и предлагаются здесь.

| Группа | Имена (пункт меню) | Шт. |
|---|---|---|
| `rome` | `to_menhunter`, `to_butcher`, `to_market`, `to_fog` (В Туманы), `to_square` (На площадь), `talk_farid` (Подойти к Фариду), `talk_menhunter` (Поговорить), `trade_menhunter`, `trade_butcher` | 9 |
| `fog` | `wander` (Блуждать), `sleep` (Спать), `call_horse` («Лошаа-аадка!»), `ride_horse` (Сесть на коня) | 4 |
| `woods` | `deeper` (Дальше в лес), `back` (Назад к опушке), `hunt` (Выслеживать), `lure` (Использовать приманку), `set_traps`, `check_traps`, `make_camp` (Разбить лагерь), `to_fog` | 8 |
| `camp` | `inspect`, `subdue` (Показать, кто главный), `dance`, `dance_fashion` (Танец (красивый)), `kill` (Зарезать), `rob` (Обокрасть), `release` (Отпустить), `dump_corpse` (Выбросить труп), `feed_hound`, `feed_retriever`, `rest` (Отдыхать), `sleep` (Спать), `to_woods` (Вернуться в лес) | 13 |
| `food` | `eat_supplies`, `eat_meat`, `eat_canned` — в лагере и в Туманах | 3 |
| `inventory` | `drink_tonic`, `read_notes`, `ask_bird` | 3 |

Итого 40. Решить: «Отдыхать» и «Спать» — один пункт с текстом по времени суток или два; кормить питомца одной едой или выбором еды.

### 3.6. Охота

Сцена `rh:dialogue.hunt.encounter` ([§ 4.5](../RH/HunterGameplay.md#45-охота)).

| Текст | ID | Шт. | Основание |
|---|---|---|---|
| Текст стадий | `rh:text.dialogue.hunt.encounter.stalking.text` (с приманкой), `.chase.text`, `.fight.text` | 3 | док |
| Сцены обнаружения | `rh:text.hunt.scene.<сцена>` — 12 сцен (§ 2.3) | 12 | док, заполнено |
| Варианты | `…stalking.look` (Осмотреть), `…stalking.sneak` (Подкрасться), `…stalking.leave`, `…chase.pursue` (Догонять), `…chase.leave`, `…fight.leave` | 6 | нужно |
| Способы захвата | `rh:text.capture_method.<способ>.title` для 11 способов: `crossbow`, `throwing_knives`, `net`, `stone`, `sleeping_gas`, `retriever`, `bear_hug`, `club`, `knife`, `taser`, `bare_hands` | 11 | док (поле `title`) |
| Причины недоступности способа | общие причины условий (§ 3.12) | — | — |

Итого 20, плюс сообщения охоты (§ 3.11).

### 3.7. Бой с совомедведем

| Текст | ID | Шт. | Основание |
|---|---|---|---|
| Имя противника | `rh:text.actor.monster.owlbear.title` | 1 | нужно |
| Вступление | `rh:text.combat.owlbear.intro` | 1 | док |
| Действия | `rh:text.combat.owlbear.<действие>`: `flee` (Бежать), `fight` (Сражаться), `drop_catch` (Бросить дикарку), `sic_hound`, `sic_retriever`, `smoke_bomb` | 6 | нужно |
| Исходы | `rh:text.combat.owlbear.outcome_won`, `_fled`, `_escaped`, `_died` | 4 | нужно |

### 3.8. Диалоги

Текст узла — `rh:text.dialogue.<группа>.<имя>.<узел>.text`, вариант — `rh:text.dialogue.<группа>.<имя>.<узел>.<вариант>`.

| Диалог | Тексты | Шт. | Основание |
|---|---|---|---|
| `intro.start` | `rh:text.intro.choose_hero`; названия классов (§ 3.2); `name.placeholder_hero`, `_heroine`; `rh:text.intro.history.<класс>` × 4; `rh:text.intro.rome1`, `rome2` | 9 | док, заполнено |
| Общие варианты | `rh:text.dialogue.common.next` (Далее), `.back` (Спросить о другом), `.end_talk` (Закончить разговор) | 3 | нужно |
| `camp.inspect` | `main.text`, `main.close` | 2 | нужно |
| `fog.ghost_horse` | `main.text`, `main.tame`, `main.leave` | 3 | заполнено, кроме `leave` |
| `market.farid` | `rh:text.farid.hello_male`, `hello_fem`, вопросы `ask_most_expensive`, `ask_price_factors`, вариант `sell`; ответы `most_expensive`, `price_factors1`, `price_factors2` | 8 | заполнено; ответ о цене обрывается на первом факторе |
| `rome.menhunter` | `rh:text.menhunter.greeting`, вопросы `ask_gear`, `ask_where_to_sell`; ответы `advice_gear`, `where_to_sell` | 5 | заполнено |
| `butcher.lessons` | реплики мясника и вариант завершения | ≥ 2 | решить сценарий |
| `misc.hunter_notes` | `rh:text.notes.rome`, `.fog`, `.woods`, `.camp` | 4 | заполнено |
| `owlbear.peaceful` | `main.text`, `main.retreat`, `main.pet`, `pet.text`, `pet.close` | 5 | заполнено, кроме `pet.close` |
| `system.game_over` | `main.text_tired`, `main.text_murdered`, `main.restart`, `rh:text.system.game_over.continue`, `.continue_msg`, `.finish` | 6 | заполнено |
| `system.victory` | `main.text`, `main.endless`, `main.finish`, «Конец» `rh:text.system.the_end` | 4 | заполнено |
| Помощь | `rh:text.help.main` | 1 | заполнено; показ — решить вместе с главным меню |

Главное меню и проверка возраста (`rh:text.system.main_menu.*`, `rh:text.system.age_check.*`) — тексты на будущее: меню пока нет.

### 3.9. Магазины

| Текст | ID | Шт. | Основание |
|---|---|---|---|
| Названия | `rh:text.shop.rome.menhunter.title`, `.butcher.title`, `.farid.title` | 3 | нужно |
| Имена торговцев | `rh:text.actor.npc.menhunter.title`, `.butcher.title`, `.farid.title` | 3 | нужно |
| Вкладки Менхантера | `rh:text.shop.rome.menhunter.tab_<id>`: `tools`, `weapons`, `clothes`, `pets`, `consumables`, `sell` | 6 | нужно |
| Действие лавки | `rh:text.shop.rome.butcher.lessons` (Обучение) | 1 | нужно |

Имя Старого охотника — `rh:text.actor.npc.old_hunter.title`, ещё 1 текст: оно встречается в повествовании и записках.

### 3.10. Танец

| Текст | ID | Шт. | Основание |
|---|---|---|---|
| Шаги по номерам источника | `rh:text.dance.step_<N>`, номера — [§ 7.3](../RH/HunterGameplay.md#73-тексты-танца-по-номерам-источника) | 43 | док; смысл 87, 91, 92 открыт |
| Действия цикла | `rh:text.dance.action.<id>`: `finish`, `pause`, `soft`, `hard`, `start`, `softer`, `keep`, `harder`, `kill`, `leave` | 10 | док |
| Мягкие и жёсткие действия подменю | `rh:text.dance.action.soft_1…5`, `hard_1…5` | 10 | док |
| Финал | `rh:text.dance.action.bow`, `flourish`, `fashion` | 3 | док |
| Fashion | `rh:text.dance.action.begin`, `hold`, `close_in`, `move_fast`, `finish_close`, `finish_far` | 6 | док |
| Возврат из подменю | `rh:text.dance.action.back` | 1 | нужно |
| Подписи шкал | `rh:text.dance.hunter_engagement`, `rh:text.dance.beast_engagement` | 2 | нужно |
| Причины неактивности | `rh:text.error.dance.<действие>` для `finish`, `pause`, `start`, `softer`, `keep`, `harder`, `kill`, `begin`, `hold`, `close_in`, `move_fast`, `finish_close`, `finish_far` | 13 | док |

Итого 88 с причинами отказа; причины учтены здесь, а не в § 3.12.

### 3.11. Сообщения

`rh:text.message.<ключ>`; показываются в «последнем событии» и в журнале сцены. Параметры подставляются в текст.

**Названы в правилах (23):**

| Механика | Ключи |
|---|---|
| Питомцы | `pets.hound_lost` |
| Туманы | `fog.horse_heard`, `fog.lost` |
| Ловушки | `traps.caught`, `traps.empty` |
| Охота | `hunt.lured`, `hunt.nobody`, `hunt.noticed`, `hunt.unnoticed`, `hunt.branch_snapped`, `hunt.close`, `hunt.cant_catch_up`, `hunt.caught_up`, `hunt.capture_failed`, `hunt.escaped`, `hunt.lost` |
| Добыча | `catch.escaped` (отдых и сон), `catch.escaped_walk` (ходьба по лесу) |
| Лагерь | `camp.slept_well`, `camp.froze` |
| Птица-секретарь | `bird.describe`, `bird.estimate_alive` (`{market}`, `{shop}`), `bird.estimate_dead` (`{shop}`) |

**Предложены (18), решить:** без них действие проходит молча, и «последнее событие» не меняется.

| Механика | Ключи |
|---|---|
| Туманы | `fog.reached_rome` (мясо испортилось, усталость и голод сброшены), `fog.reached_woods`, `fog.slept`, `fog.horse_ridden` |
| Охота | `hunt.captured_alive`, `hunt.captured_dead` |
| Ловушки | `traps.set` |
| Лагерь | `camp.rested`, `camp.ate`, `camp.fed_pet`, `camp.subdued`, `camp.killed`, `camp.robbed` (`{item}`, `{amount}`), `camp.released`, `camp.corpse_dumped` |
| Предметы | `tonic.drunk` |
| Торговля | `shop.bought` (`{item}`, `{price}`), `shop.sold` (`{price}`) |

### 3.12. Причины отказа

`rh:text.error.<ключ>`; показываются у неактивного пункта.

| Ключ | Когда | Основание |
|---|---|---|
| `mode.busy` | действие инвентаря во время режима | док |
| `woods.no_deeper`, `woods.at_edge` | дальше идти некуда; уже на опушке | док |
| `intro.bad_name` | пустое или длиннее 40 символов имя | док |
| `market.dead_not_accepted` | Фарид не берёт мёртвую | док |
| `shop.bad_quantity`, `shop.already_owned`, `shop.out_of_stock`, `shop.not_enough_gold` (`{price}`, `{gold}`), `shop.nothing_to_sell` | общие отказы магазина | док (архитектура) |
| `condition.has_item` (`{item}`), `condition.item_count_at_least`, `condition.class_trait`, `condition.lacks_class_trait`, `condition.woods_depth_in`, `condition.time_of_day_in`, `condition.flag`, `condition.no_catch`, `condition.has_catch`, `condition.catch_alive`, `condition.catch_dead`, `condition.catch_not_subdued`, `condition.catch_tattoo`, `condition.var_is` | общий текст на тип условия | нужно |

Итого 23; ещё 13 причин танца учтены в § 3.10. Решить: где общего текста условия мало, причину можно задать в самом условии — например, «Ловушки уже стоят» вместо общего текста флага. Каждая такая причина добавляет текст.

### 3.13. Голод и время суток

| Текст | ID | Шт. | Основание |
|---|---|---|---|
| Состояния голода | `rh:text.hunger.fed`, `.hungry`, `.very_hungry`, `.famished`, `.exhausted` | 5 | док |
| Время суток | `rh:text.time_of_day.morning`, `.day`, `.evening`, `.night` | 4 | нужно |

### 3.14. Общий интерфейс

Подписи панелей — `rh:text.core_interface.<поле>` (форма предлагается): имя, класс, золото, время суток, глубина леса, усталость, голод, добыча, жива, мертва, тату, укрощена, нет добычи, инвентарь, инвентарь пуст, последнее событие — 16. Названия глубин `rh:text.woods.depth_0…3` (опушка … чаща) — 4. Итого 20; состояния голода и время суток учтены в § 3.13.

## 4. Открытые вопросы

- Смысл текстов fashion 91 и 92 и финала 87 ([правила, § 9](../RH/HunterGameplay.md#9-открытые-вопросы)).
- Набор фонов: обязательный минимум или варианты по времени суток и глубине (§ 2.4).
- Сценарий обучения у мясника; окончание ответа Фарида о цене (§ 3.8).
- Предложенные сообщения (§ 3.11) и частные причины отказа вместо общих (§ 3.12).
- Нужна ли картинка записок (§ 2.3).
