# TestVR

Небольшое VR-приложение на Unreal Engine 5 для Meta Quest 3. Проект демонстрирует OpenXR-взаимодействие двумя контроллерами, наведение и захват объектов, world-space UI, создание и удаление объектов, а также дополнительные механики Snap Zone, Save/Load и Two-hand Interaction.

За основу взят UE шаблон VRTemplate (VR-фреймворк на Blueprints: XR-пешка, локомоция, телепортация, меню контроллера). Контент ассета лежит в `Content/XRFramework`; весь код тестового задания в модуле `Source/TestVR` (C++) и в `BP_XRPawn`, где C++-компоненты подключены к событиям Enhanced Input.

## Видео

- [Запись трансляции SteamVR](https://drive.google.com/file/d/1CUhsXiDcN7lvLVZU2USYvCsN0TTDOz_9/view?usp=sharing) - PC VR: hover, grab, two-hand, VR UI, Snap Zone, Save/Load.
- [Запись APK билда](https://drive.google.com/file/d/1pduazBU7_E2t-AeK3oQMZIO6mtXDQ1nv/view?usp=sharing) - сборка ASTC на Meta Quest 3.

## Версия Unreal Engine

- Unreal Engine **5.8.2** (`++UE5+Release-5.8`);
- основной runtime — **OpenXR**;
- целевая гарнитура — **Meta Quest 3**;
- Android package flavor — **ASTC / arm64**.

### Для сборки под Android

- Android SDK Platform 36;
- Android SDK Build Tools 36.0.0;
- NDK 27.2.12479018;
- CMake 3.22.1;
- OpenJDK 21;

## Запуск
1. Клонировать репозиторий;
2. TestVR.uproject -> Generate Visual Studio project files;
3. Открыть TestVR.sln, собрать конфигурацию `TestVREditor | Development | Win64`;
4. PC VR: включить OpenXR runtime (Meta Quest Link / SteamVR) -> в редакторе кнопка "VR Preview";
5. Quest 3: Platforms -> Android -> Package Project (flavor ASTC);

## Проверка
- PC VR: Meta Quest 3 через SteamVR / Meta Quest Link — hover, grab, two-hand, UI, Snap Zone, Save/Load работают.
- Android: APK собран (ASTC), запущен на Meta Quest 3.

## Управление

| Действие | Meta Quest Touch |
|---|---|
| Наведение на объект | направление контроллера |
| Захват / отпускание | Grip соответствующей руки |
| Two-hand Interaction | удерживать объект правой рукой и нажать Grip левой рядом с объектом |
| Взаимодействие с VR UI | Trigger |
| Открыть / закрыть меню | `Y` на левом или `B` на правом контроллере |
| Перемещение и поворот | thumbsticks |
| Телепортация | стандартное управление XR Framework через thumbstick |

Захват намеренно на **Grip**, а не на Trigger (как в пункте 4 ТЗ): Trigger отдан кликам по VR UI,
что убирает неоднозначность «клик по панели» vs «захват объекта» и совпадает с конвенцией
VR-шаблонов. Enhanced Input Actions и mappings — в `Content/XRFramework/Input`.

## Интерактивные объекты

На уровне размещено не менее пяти объектов нескольких типов: cube, sphere и cone, включая варианты с физической симуляцией. Все они наследуются от `AInteractiveObject` и предоставляют данные через `IInteractable`:

- `Name` — отображаемое имя;
- `Type` — категория `Generic`, `Tool`, `Container` или `Device`;
- `Value` — числовой параметр.

Новый тип добавляется созданием C++- или Blueprint-наследника `AInteractiveObject` и настройкой его mesh/data. Изменять `UVRInteractionComponent` или добавлять отдельный cast для нового класса не требуется

## Архитектура

Логика контроллеров (`UVRInteractionComponent`) работает только через интерфейс `IInteractable` и не знает о конкретных классах объектов; объекты не знают о руках и жестах. Реестр, фабрика и Save/Load вынесены в `UWorldSubsystem`.

### Как определяется интерактивный объект

Объект интерактивен тогда и только тогда, когда его класс реализует `IInteractable`. `UVRInteractionComponent` пускает line trace от контроллера и фильтрует попадание через `Cast<IInteractable>(HitActor)` — приведений к конкретным классам (`cube`, `sphere`, `cone`) нигде нет. Новый тип добавляется C++- или Blueprint-наследником `AInteractiveObject`; код взаимодействия при этом не меняется.

### Где хранится состояние взаимодействия

Состояние взаимодействия («какая рука на что наведена / что держит», активен ли two-hand) живёт только в `UVRInteractionComponent`:

- по одной структуре `FVRHandInteraction` на руку — `Source` (компонент прицела), `Target` (наведённый или удерживаемый актор), `Role` ∈ `Idle | Hovering | Holding`, таймер hover-гистерезиса;
- флаг `bTwoHandActive` и опорный кадр жеста (исходные дистанция между контроллерами, масштаб, поворот, вектор между руками);
- единственная точка смены роли — `SetHandRole()`: оттуда идут `BeginHover`/`EndHover`, подписка на уничтожение цели и делегат `OnInteractionTargetChanged`.

`AInteractiveObject` хранит только собственное физическое состояние (`bIsGrabbed`, `bIsSecondHandGrabbed`, `bIsSnapped`, `bPhysicsEnabled`) и не знает, какая рука его держит.

### Interaction

`IInteractable` определяет контракт: hover, grab, second-hand grab, release, delete и получение данных для панели. `UVRInteractionComponent` выполняет line trace, управляет лучами Niagara, переключает hover/held через `SetHandRole()`, гейтит two-hand по дистанции до объекта и каждый тик пересчитывает two-hand transform.

### Interactive Object

`AInteractiveObject` реализует `IInteractable`, владеет mesh, данными, highlight и физическим состоянием. При grab симуляция временно отключается, объект прикрепляется к руке, а при release остаётся в новой позиции и восстанавливает физику, если он не находится в Snap Zone.

### Object Registry

`UInteractiveObjectSubsystem` — `UWorldSubsystem`, который хранит weak references на объекты, создаёт экземпляры перед камерой, обрабатывает удаление и является точкой входа Save/Load. Объекты регистрируются в `BeginPlay` и удаляются из реестра в `EndPlay`/`OnDestroyed`

### VR UI

`WBP_VRInfo` размещается как world-space widget в меню контроллера. Панель отображает `Name`, `Type`, `Value` и актуальное количество объектов. Текущая цель определяется в порядке held object -> right hovered object -> left hovered object

Каталог Add строится один раз при первом открытии меню и кэшируется в подсистеме. В редакторе он собирается через Asset Registry (виден любой наследник `AInteractiveObject`, в том числе не размещённый на сцене); в упакованной сборке — из уже загруженных классов, без синхронной подгрузки пакетов на игровом потоке. Delete list обновляется инкрементально: существующие строки сохраняются, а при изменении реестра добавляются или удаляются только затронутые элементы.

### Snap Zone

`ASnapZone` проверяет объект в момент release, выравнивает его по `SnapPoint` и занимает зону одним объектом. Физика snapped object остаётся отключённой до следующего grab. `ZoneId` связывает объект с той же зоной после Load

### Save / Load

`UInteractiveSceneSaveGame` хранит:

- soft class reference;
- object data;
- полный transform, включая non-uniform scale;
- идентификатор занятой Snap Zone;

При Load текущие интерактивные объекты заменяются сохранёнными экземплярами. Для scale используется `ESpawnActorScaleMethod::OverrideRootScale`

## Разделение C++ и Blueprint

### C++

- интерфейс и структура данных объекта;
- hover/grab/release/two-hand state machine;
- lifetime и обработка уничтожения;
- runtime spawn/delete и реестр;
- Snap Zone;
- Save/Load;
- модель и обновление VR UI;

### Blueprint

- настройка XR pawn и компонентов контроллеров;
- Enhanced Input Actions и Mapping Contexts;
- визуальная сцена и освещение;
- mesh, материалы и значения конкретных типов объектов;
- внешний вид world-space меню и UI;
- стандартная XR Framework locomotion/teleportation;

## Структура исходников

```text
Source/TestVR/
  Interaction/   IInteractable, data types, controller interaction state
  Objects/       InteractiveObject, registry/factory, SnapZone
  SaveGame/      SaveGame record and slot
  UI/            VR info panel and dynamic list entries
```

## Известные ограничения

- Видео записаны при TraceDistance = 10 м; в текущей сборке - 2 м (правится на компоненте VRInteraction в BP_XRPawn).
- Массовое быстрое создание физических объектов пока не ограничено. Объекты появляются в одной области перед камерой, поэтому большое количество пересекающихся rigid bodies может перегрузить физику Quest 3
- Save/Load выполняется синхронно и рассчитан на небольшой объём тестовой сцены
- Захват на Grip, а не на Trigger (см. раздел «Управление»)
- Каталог типов для `Add Object` строится один раз при первом открытии меню и кэшируется. В упакованной сборке в список попадают только уже загруженные наследники `AInteractiveObject` (все, что размещены на сцене) — это осознанный компромисс: синхронный обход Asset Registry с подгрузкой классов на старте вешает standalone-устройство
- Two-hand: отрабатываются позиция, поворот вокруг линии между руками и масштаб; twist (roll вокруг оси рук) не учитывается
