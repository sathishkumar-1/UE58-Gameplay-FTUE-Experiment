# Gameplay System Map

**Source implementation:** UE 5.8.1

**Production target:** UE 5.8.0

Use this map to find the current owner of a gameplay change. Content asset paths use Unreal `/Game/...` notation; source paths are relative to the project root.

## Where do I change...?

| I want to change... | Go here |
| --- | --- |
| Player stationary movement | `ACombatCharacter` -> `Source/GSGR/Variant_Combat/CombatCharacter.cpp` -> `BeginPlay()`, `Move()`, `DoMove()`; Blueprint Defaults -> **Stationary Combat** |
| Player walk/movement speed | Stationary mode forces `MaxWalkSpeed = 0` in `ACombatCharacter::BeginPlay()`; non-stationary base default is set in the constructor and can be overridden in `BP_CombatCharacter` -> **Character Movement** |
| Enemy movement speed | `/Game/Variant_Combat/Blueprints/AI/BP_CombatEnemy` -> **Character Movement -> Max Walk Speed** |
| Enemy combat distance | `BP_CombatEnemy` -> **Stationary Arena -> ArenaAttackRange** (`ACombatEnemy`) |
| Enemy attack interval/pacing | `BP_CombatEnemy` -> **Stationary Arena -> ArenaAttackWindupDelay**, `ArenaAttackCooldown`, `ArenaAttackDelayVariation` |
| Enemy difficulty/aggression | `BP_CombatEnemy` -> **Stationary Arena** tuning plus **Character Movement -> Max Walk Speed** and **Damage** |
| Light Attack | `ACombatCharacter::DoComboAttackStart()`/`ComboAttack()` and `/Game/Variant_Combat/Anims/AM_ComboAttack` |
| Heavy Attack | `ACombatCharacter::DoChargedAttackStart()`/`DoChargedAttackEnd()`/`ChargedAttack()` and `/Game/Variant_Combat/Anims/AM_ChargedAttack` |
| Dodge | `ACombatCharacter::DoBackDodge()` and Blueprint Defaults -> **Dodge**, **Dodge \| Movement**, **Dodge \| Timing** |
| Dodge animation/movement | Visual: `/Game/Variant_Platforming/Anims/AM_Dash`; displacement: `ACombatCharacter::DoBackDodge()`, `UpdateBackDodge()`, `FinishBackDodge()` |
| Attack movement/root motion | `ACombatCharacter::BeginPlay()` and attack completion in `CombatCharacter.cpp`; stationary mode ignores root motion and restores the anchor transform |
| FTUE timing values | `/Game/Variant_Combat/Blueprints/BP_CombatGameMode` -> Blueprint Defaults -> **FTUE \| Timing** (`ACombatGameMode` properties) |
| FTUE Light Attack step | `ACombatGameMode::EnterFTUEState(LightAttack)` and `HandleEnemyDamaged()` |
| FTUE Heavy Attack step | `ACombatGameMode::EnterFTUEState(HeavyAttack)` and `HandleEnemyDamaged()` |
| FTUE Dodge sequence | `ACombatGameMode::EnterFTUEState(Dodge)`, `HandleTutorialDodgeWindow()`, `ShowDodgePrompt()`, `HandlePlayerDodgeStarted()` |
| FTUE Start hook | `ACombatGameMode::BeginFTUE()` -> `OnFTUEStarted` |
| FTUE Complete hook | `ACombatGameMode::CompleteFTUE()` -> `OnFTUECompleted` |
| FTUE UI | `Source/GSGR/Variant_Combat/UI/CombatRunWidget.h/.cpp`; state/copy selected by `ACombatGameMode` |
| FTUE save state | `UCombatFTUESaveGame::bHasCompletedFTUE`; loaded/saved by `ACombatGameMode::LoadFTUEProfile()`/`SaveFTUECompletion()` |
| `FTUE.Reset` | Console registration and `ResetFTUEProfile()` in `Source/GSGR/Variant_Combat/CombatGameMode.cpp` |
| Startup menu | `ACombatGameMode::ShowStartupMenu()` and `UCombatRunWidget::ShowStartupMenu()` |
| Survival timer | `ACombatGameMode::Tick()`/`SurvivalTime`; display in `UCombatRunWidget` |
| Game Over | `ACombatGameMode::HandlePlayerDied()` and `UCombatRunWidget::ShowGameOver()` |
| Restart | `ACombatGameMode::HandleRestartSelected()`/`RestartLevel()`; button routing in `ACombatPlayerController` |

## Player

| Area | Owner/location |
| --- | --- |
| Main class/Blueprint | `ACombatCharacter` in `Source/GSGR/Variant_Combat/CombatCharacter.h/.cpp`; `/Game/Variant_Combat/Blueprints/BP_CombatCharacter` |
| Movement | `ACombatCharacter::Move()`/`DoMove()` and `BeginPlay()`; Blueprint Defaults -> **Stationary Combat** and **Character Movement** |
| Walk speed | Current stationary mode sets zero in `BeginPlay()`; constructor non-stationary default is `400`; Blueprint **Character Movement -> Max Walk Speed** can override the non-stationary value |
| Health | `ACombatCharacter` -> Blueprint Defaults -> **Damage -> MaxHP**; damage/death logic in `CombatCharacter.cpp` |
| Light/combo attack | Input `IA_ComboAttack`; `DoComboAttackStart()`, `ComboAttack()`, `CheckCombo()`; Blueprint Defaults -> **Melee Attack \| Combo**; montage `AM_ComboAttack` |
| Heavy/charged attack | Input `IA_ChargedAttack`; `DoChargedAttackStart()`, `DoChargedAttackEnd()`, `ChargedAttack()`; Blueprint Defaults -> **Melee Attack \| Charged**; montage `AM_ChargedAttack` |
| Dodge | Input property `JumpAction`/asset `IA_Jump`; `DoBackDodge()`; Blueprint Defaults -> **Dodge**, **Dodge \| Movement**, **Dodge \| Timing** |
| Attack traces/damage | `ACombatCharacter` -> **Melee Attack** defaults and `DoAttackTrace()`; montage notify `UAnimNotify_DoAttackTrace` |
| Attack/root motion | Stationary anchor/root-motion setup in `BeginPlay()`; attack end restores anchor; dodge uses procedural displacement while the montage supplies the visual |
| Input binding code | `ACombatCharacter::SetupPlayerInputComponent()`; contexts are applied by `ACombatPlayerController` |
| Mesh/animation | `BP_CombatCharacter` -> mesh/Anim Class; `/Game/Variant_Combat/Anims/ABP_Manny_Combat` |

## Enemy

| Area | Owner/location |
| --- | --- |
| Main class/Blueprint | `ACombatEnemy` in `Source/GSGR/Variant_Combat/AI/CombatEnemy.h/.cpp`; `/Game/Variant_Combat/Blueprints/AI/BP_CombatEnemy` |
| Movement speed | `BP_CombatEnemy` -> **Character Movement -> Max Walk Speed** |
| Combat distance | Blueprint Defaults -> **Stationary Arena -> ArenaAttackRange** |
| Attack pacing | **Stationary Arena -> ArenaAttackWindupDelay**, `ArenaAttackCooldown`, `ArenaAttackDelayVariation` |
| Aggression/difficulty | Adjust movement speed, attack range/timing, and **Damage -> MaxHP**/melee damage. The active simple state transitions are in `ACombatEnemy::Tick()` |
| Attack behaviour | Default stationary loop is Approach -> Wait -> Attack -> Recovery in `ACombatEnemy::Tick()`; attack starts through `DoAIComboAttack()` |
| Health | `ACombatEnemy` -> Blueprint Defaults -> **Damage -> MaxHP**; damage/death in `CombatEnemy.cpp` |
| Animation | `BP_CombatEnemy` uses `/Game/Variant_Combat/Anims/ABP_Manny_Combat`; combo/charged montages are assigned in Blueprint defaults |
| AI Controller | `ACombatAIController` in `AI/CombatAIController.h/.cpp`; `/Game/Variant_Combat/Blueprints/AI/BP_CombatAIController` |
| StateTree/EQS | Optional non-stationary path: `ST_CombatEnemy`, `EnvQuery_Evade`, `EnvQuery_Fallback`, `EnvQuery_Flank` under `/Game/Variant_Combat/Blueprints/AI/` |
| Spawn/respawn | `ACombatEnemySpawner` in `AI/CombatEnemySpawner.h/.cpp`; `/Game/Variant_Combat/Blueprints/AI/BP_CombatEnemySpawner` |

The current default `bUseStationaryArenaAI = true` bypasses the StateTree in `ACombatAIController::OnPossess()`. Tune the simple/dumb combat behaviour on `BP_CombatEnemy` under **Stationary Arena**, not in `ST_CombatEnemy`.

## FTUE

The state machine owner is `ACombatGameMode` in `Source/GSGR/Variant_Combat/CombatGameMode.h/.cpp`. `EnterFTUEState()` applies state permissions/UI and the success handlers schedule transitions.

```text
None -> Welcome -> LightAttack -> HeavyAttack -> Dodge -> Complete
```

| State/area | Owner/location |
| --- | --- |
| `None` / initial decision | `InitializeRunFlow()`, `ShowStartupMenu()`, `HandlePlaySelected()` |
| `Welcome` | `EnterFTUEState(Welcome)`; deliberate press/release detected by `ACombatPlayerController::InputKey()` |
| `LightAttack` | `EnterFTUEState(LightAttack)`, `HandleEnemyDamaged()` |
| `HeavyAttack` | `EnterFTUEState(HeavyAttack)`, `HandleEnemyDamaged()` |
| `Dodge` | `EnterFTUEState(Dodge)`, `HandleTutorialDodgeWindow()`, `ShowDodgePrompt()`, `HandlePlayerDodgeStarted()` |
| `Complete` | `EnterFTUEState(Complete)`, `CompleteFTUE()` |
| Start hook | `OnFTUEStarted`, emitted once by `BeginFTUE()` |
| Complete hook | `OnFTUECompleted`, emitted once by `CompleteFTUE()` |
| Timing/tuning | `BP_CombatGameMode` -> Blueprint Defaults -> **FTUE \| Timing**: welcome/light/heavy/dodge/completion delays |
| Tutorial enemy control | `ACombatGameMode` discovers/binds the enemy; `ACombatEnemy::SetTutorialControlled()`, `StartTutorialDodgeAttack()`, `ResolveTutorialDodgeAttack()`, `TickTutorialControl()` |
| UI | `UCombatRunWidget` in `UI/CombatRunWidget.h/.cpp`; no FTUE Widget Blueprint |
| Persistence | `UCombatFTUESaveGame` plus `ACombatGameMode::LoadFTUEProfile()`/`SaveFTUECompletion()`; slot under **FTUE \| Persistence** |
| Reset | Non-Shipping console command `FTUE.Reset` -> `ACombatGameMode::ResetFTUEProfile()` |

## Startup and game flow

| Flow | Owner/location |
| --- | --- |
| Startup Menu | `ACombatGameMode::ShowStartupMenu()` -> `ACombatPlayerController::ShowStartupMenu()` -> `UCombatRunWidget::ShowStartupMenu()` |
| Play | `UCombatRunWidget` button -> `ACombatPlayerController` -> `ACombatGameMode::HandlePlaySelected()` |
| First-time/returning check | `ACombatGameMode::LoadFTUEProfile()` and `HandlePlaySelected()` inspect `bHasCompletedFTUE` |
| Starting gameplay | First-time: `BeginFTUE()`; returning: `StartNormalGameplay()` |
| Survival timer | `ACombatGameMode::Tick()` increments `SurvivalTime` only while `bRunActive` |
| Death | Player death delegate -> `ACombatGameMode::HandlePlayerDied()` |
| Game Over | `HandlePlayerDied()` pauses flow and calls `UCombatRunWidget::ShowGameOver()` |
| Restart | `HandleRestartSelected()` -> dynamic current-level restart with `RestartRun=1` |
| Quit | `HandleQuitSelected()`; UI button routing lives in `ACombatPlayerController`/`UCombatRunWidget` |

The map must run the intended combat GameMode. `/Game/Variant_Combat/Lvl_Combat` uses `/Game/Variant_Combat/Blueprints/BP_CombatGameMode` as its per-map override; the source project’s global default is still the Third Person GameMode.

## UI

| Class/asset | Responsibility |
| --- | --- |
| `Source/GSGR/Variant_Combat/UI/CombatRunWidget.h/.cpp` (`UCombatRunWidget`) | Code-built Startup, FTUE prompts/feedback, survival time, and Game Over/Restart/Quit overlay |
| `Source/GSGR/Variant_Combat/UI/CombatLifeBar.h/.cpp` (`UCombatLifeBar`) | C++ base for health display |
| `/Game/Variant_Combat/UI/UI_LifeBar` | Player/enemy life-bar Widget Blueprint |
| `/Game/Variant_Combat/Input/UI_TouchInterface_Combat` | Optional combat touch controls; interface asset is `/Game/Variant_Combat/Input/BPI_TouchInterface_Combat` |

`ACombatPlayerController` creates `UCombatRunWidget` directly using `UCombatRunWidget::StaticClass()` and manages UI/game input modes.

## Input

| Action/context | Path | Player-side handler/owner |
| --- | --- | --- |
| Move (`IA_Move`, Axis2D) | `/Game/Input/Actions/IA_Move` | `ACombatCharacter::Move()`/`DoMove()` |
| Look (`IA_Look`, Axis2D) | `/Game/Input/Actions/IA_Look` | `ACombatCharacter::Look()`/`DoLook()` |
| Mouse look (`IA_MouseLook`, Axis2D) | `/Game/Input/Actions/IA_MouseLook` | `ACombatCharacter::Look()`/`DoLook()` |
| Dodge (`IA_Jump`, Boolean) | `/Game/Input/Actions/IA_Jump` | `ACombatCharacter::BackDodgePressed()`/`DoBackDodge()` |
| Light (`IA_ComboAttack`, Boolean) | `/Game/Variant_Combat/Input/Actions/IA_ComboAttack` | `ACombatCharacter::DoComboAttackStart()` |
| Heavy (`IA_ChargedAttack`, Boolean) | `/Game/Variant_Combat/Input/Actions/IA_ChargedAttack` | `DoChargedAttackStart()`/`DoChargedAttackEnd()` |
| Camera side (`IA_ToggleCameraSide`, Boolean) | `/Game/Variant_Combat/Input/Actions/IA_ToggleCameraSide` | `ACombatCharacter::ToggleCamera()` |
| Combat context | `/Game/Variant_Combat/Input/IMC_Combat` | Applied by `ACombatPlayerController`; source includes Space/gamepad face-button-left for dodge |
| Mouse context | `/Game/Input/IMC_MouseLook` | Applied by `ACombatPlayerController` |

Action asset properties live on `BP_CombatCharacter` under **Input**. Default Mapping Contexts live on `BP_CombatPlayerController`. FTUE prompts query the active mapping for the relevant action.

## Animation

| Asset | Who plays it | Root-motion relevance |
| --- | --- | --- |
| `/Game/Variant_Combat/Anims/ABP_Manny_Combat` | Player and enemy mesh Anim Class | Locomotion/combat animation owner; requires compatible mannequin skeleton |
| `/Game/Variant_Combat/Anims/AM_ComboAttack` | `ACombatCharacter` and `ACombatEnemy` | Uses combo sections and custom combo/trace notifies; stationary actors ignore translation |
| `/Game/Variant_Combat/Anims/AM_ChargedAttack` | `ACombatCharacter` and optional enemy charge path | Uses charge loop/attack sections and custom charge/trace notifies; stationary actors ignore translation |
| `/Game/Variant_Platforming/Anims/AM_Dash` | `ACombatCharacter::DoBackDodge()` | Visual only in current combat flow; code supplies dodge displacement and ignores root motion |
| Mannequin attack/dash sequences | `/Game/Characters/Mannequins/` dependencies such as `MM_Attack_01/02/03`, `MM_ChargedAttack`, `MM_Dash` | Must match the mesh/AnimBP skeleton |

Animation notify classes are in `Source/GSGR/Variant_Combat/Animation/`. The current dash montage also references `Source/GSGR/Variant_Platforming/Animation/AnimNotify_EndDash.*`; a clean destination dodge montage can avoid that cross-variant dependency.

## AI

| Asset/class | Responsibility |
| --- | --- |
| `ACombatEnemy` / `BP_CombatEnemy` | Active default stationary arena brain and its speed/range/pacing/difficulty tuning |
| `ACombatAIController` / `BP_CombatAIController` | Starts the optional StateTree only for non-stationary enemies |
| `/Game/Variant_Combat/Blueprints/AI/ST_CombatEnemy` | Optional general combat decision flow |
| `AI/CombatStateTreeUtility.h/.cpp` | Custom StateTree conditions/evaluators/tasks for attacks, landing, facing, speed, and player data |
| `EnvQuery_Evade`, `EnvQuery_Fallback`, `EnvQuery_Flank` | Optional EQS location selection for the StateTree path |
| `UEnvQueryContext_Player`, `UEnvQueryContext_Danger` | C++ EQS contexts |

For “enemy is too fast/aggressive,” start with `BP_CombatEnemy` -> **Character Movement** and **Stationary Arena**. Do not edit StateTree/EQS unless `bUseStationaryArenaAI` is intentionally disabled.

## Save and persistence

- **Class:** `UCombatFTUESaveGame` in `Source/GSGR/Variant_Combat/CombatFTUESaveGame.h/.cpp`, derived from `ULocalPlayerSaveGame`.
- **Stored value:** `bHasCompletedFTUE` (`SaveGame` property); save format version is currently `1`.
- **Slot:** `ACombatGameMode::FTUEProfileSlotName`, default `GSGRPlayerProfile`, exposed under **FTUE | Persistence**.
- **Load:** `ACombatGameMode::LoadFTUEProfile()` during `InitializeRunFlow()`.
- **Save:** `ACombatGameMode::SaveFTUECompletion()` during successful FTUE completion; `ResetFTUEProfile()` saves the reset profile directly.
- **Reset:** `ACombatGameMode::ResetFTUEProfile()`.
- **Command:** enter exactly `FTUE.Reset` in a non-Shipping build while `ACombatGameMode` is active; replay after returning to startup/restarting.

## FTUE integration reference

### FTUE Start

`OnFTUEStarted` -> `ACombatGameMode` -> `BeginFTUE()` in `Source/GSGR/Variant_Combat/CombatGameMode.cpp` (function near line 183; broadcast near line 196).

Log: `FTUE Started integration hook fired`

### FTUE Complete

`OnFTUECompleted` -> `ACombatGameMode` -> `CompleteFTUE()` in `Source/GSGR/Variant_Combat/CombatGameMode.cpp` (function near line 386; broadcast near line 396).

Log: `FTUE Completed integration hook fired`

Function and hook names are the stable references; line numbers are only a current convenience.
