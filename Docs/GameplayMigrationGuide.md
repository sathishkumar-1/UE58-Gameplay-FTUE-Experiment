# Gameplay Migration Guide

**Source:** Unreal Engine 5.8.1

**Destination:** Unreal Engine 5.8.0

This is a backward patch-version migration. Manually integrate text/source files (`.h`, `.cpp`, `.Build.cs`, and relevant `.ini` entries), then compile them in UE 5.8.0. Treat every Unreal-serialized file (`.uasset` or `.umap`) as a separate compatibility decision: an asset last saved by UE 5.8.1 is not guaranteed to load in UE 5.8.0.

The implementation includes player movement and stationary arena behaviour, light/combo attack, heavy/charged attack, procedural back-dodge movement and its animation, attack/root-motion containment, enemy combat and spawning, optional StateTree/EQS AI, the startup menu, FTUE state machine/UI/tuning/persistence, `FTUE.Reset`, FTUE start/completion hooks, survival timing, Game Over, restart/quit flow, Enhanced Input, and the supporting animation assets.

## UE 5.8.1 to 5.8.0 compatibility

### Text and source files

The source under `Source/GSGR/` and selected settings under `Config/` can be reviewed, renamed for the destination module, and integrated manually. The main expected adjustments are the destination module API macro/name, include paths, logging category, `.Build.cs` dependencies, and any API signature that UE 5.8.0 rejects.

The current source is organized directly under `Source/GSGR/Variant_Combat/`; it does not use separate `Public/` and `Private/` directories. Preserve that layout or update include paths deliberately in the destination.

### Unreal serialized assets

Blueprints, Widget Blueprints, animations, montages, Input Actions, Input Mapping Contexts, StateTrees, EQS queries, materials, VFX, maps, and external actor packages are serialized assets. Do not assume assets saved by UE 5.8.1 will open in UE 5.8.0, even when the engine association is recorded only as `5.8`.

- First test any migration in a backed-up/disposable copy of the UE 5.8.0 destination.
- Prefer recreating simple assets in UE 5.8.0.
- Prefer destination/original UE 5.8.0-compatible mannequin and animation assets.
- Do not edit package/version metadata to force a 5.8.1 asset to load.
- If UE 5.8.0 rejects an asset, recreate it or source an original compatible version; do not repeatedly resave or hex-edit it.

## Migration compatibility matrix

| System / Asset | Current Path | Recommended Migration Method | Risk | Notes |
| --- | --- | --- | --- | --- |
| Combat C++ foundation | `Source/GSGR/Variant_Combat/` | Manual C++ integration | Low | Rename module-facing includes/logging and add destination API exports if classes cross module boundaries. Compile before Blueprint work. |
| Module dependencies | `Source/GSGR/GSGR.Build.cs` | Manual C++ integration | Low | Required modules are listed under [Migration prerequisites](#migration-prerequisites). |
| FTUE persistence | `CombatFTUESaveGame.h/.cpp` | Manual C++ integration | Low | Uses `ULocalPlayerSaveGame`; verify destination profile/slot policy. |
| Startup/FTUE/Game Over UI | `UI/CombatRunWidget.h/.cpp` | Manual C++ integration | Low | The overlay is built in C++; there is no run-flow Widget Blueprint to migrate. |
| Life bar widget | `/Game/Variant_Combat/UI/UI_LifeBar` | Recreate in UE 5.8.0 | Medium | Parent is `UCombatLifeBar`; create/compile the C++ parent first. |
| Core combat Blueprint subclasses | `/Game/Variant_Combat/Blueprints/BP_CombatGameMode`, `BP_CombatCharacter`, `BP_CombatPlayerController`; AI children under `/Game/Variant_Combat/Blueprints/AI/` | Recreate in UE 5.8.0 | Medium | Safer than importing legacy `/Script/TP_ThirdPerson` references. Copy defaults after C++ parents compile. |
| Enhanced Input actions/contexts | `/Game/Input/` and `/Game/Variant_Combat/Input/` | Recreate in UE 5.8.0 | Low | Small assets; recreate and assign them on `BP_CombatCharacter`/`BP_CombatPlayerController`. |
| Light/combo montage | `/Game/Variant_Combat/Anims/AM_ComboAttack` | Requires UE 5.8.0 verification | Medium | Depends on mannequin skeleton, attack sequences, montage sections, and custom C++ notifies. Try dependency-aware migration only after its C++ notify classes exist. |
| Heavy/charged montage | `/Game/Variant_Combat/Anims/AM_ChargedAttack` | Requires UE 5.8.0 verification | Medium | Same downgrade and notify dependency concerns as the combo montage. |
| Combat Anim Blueprint | `/Game/Variant_Combat/Anims/ABP_Manny_Combat` | Requires UE 5.8.0 verification | Medium | Retarget/recreate if the destination character or skeleton differs. |
| Dodge montage | `/Game/Variant_Platforming/Anims/AM_Dash` | Recreate in UE 5.8.0 | High | Cross-variant hard reference; displacement is procedural, so a clean destination montage can supply only the visual. Current asset also references `AnimNotify_EndDash`. |
| Manny/Quinn meshes, skeleton, base motion | `/Game/Characters/Mannequins/` | Use destination equivalent | Low | Prefer assets already shipped with/saved by the UE 5.8.0 destination. Keep one compatible skeleton across AnimBP, sequences, and montages. |
| Attack animation sequences | mannequin sequences including `MM_Attack_01`, `MM_Attack_02`, `MM_Attack_03`, and `MM_ChargedAttack` | Use original compatible asset | Medium | Use destination equivalents when available; otherwise the source packages require verification in 5.8.0. |
| Current stationary enemy AI | `ACombatEnemy` in `AI/CombatEnemy.h/.cpp` | Manual C++ integration | Low | This is the active default arena AI and does not execute the StateTree. |
| Optional StateTree AI | `/Game/Variant_Combat/Blueprints/AI/ST_CombatEnemy` and `AI/CombatStateTreeUtility.h/.cpp` | Requires UE 5.8.0 verification | High | StateTree node APIs and serialized schemas require explicit verification; recreate the asset in 5.8.0 if rejected. Not required while `bUseStationaryArenaAI` is true. |
| EQS queries | `/Game/Variant_Combat/Blueprints/AI/EnvQuery_Evade`, `EnvQuery_Fallback`, `EnvQuery_Flank` | Requires UE 5.8.0 verification | High | Used by the optional StateTree path, not the default stationary arena path; recreate in 5.8.0 if rejected. |
| Combat VFX/camera shakes | `/Game/Variant_Combat/VFX/NS_Damage`, `/Game/Variant_Combat/Blueprints/BP_CameraShake_Hit_Player`, `/Game/Variant_Combat/Blueprints/BP_CameraShake_Hit_Enemy` | Requires UE 5.8.0 verification | Medium | Migrate through a top-level character/enemy Blueprint only after testing package compatibility. |
| Touch controls | `/Game/Variant_Combat/Input/UI_TouchInterface_Combat` and `/Game/Variant_Combat/Input/BPI_TouchInterface_Combat` | Use destination equivalent | Medium | Needed only if the destination retains this mobile/touch flow; source packages require verification if reused. |
| Combat level | `/Game/Variant_Combat/Lvl_Combat` plus its `__ExternalActors__`/`__ExternalObjects__` packages | Use destination equivalent | High | World-partition/external packages and downgrade risk make this optional prototype map the last content step. |

## Migration prerequisites

### Destination module and C++

- A destination C++ game module must exist before importing Blueprint subclasses.
- Add the equivalents of the current dependencies from `Source/GSGR/GSGR.Build.cs`: `Core`, `CoreUObject`, `Engine`, `InputCore`, `EnhancedInput`, `AIModule`, `StateTreeModule`, `GameplayStateTreeModule`, `UMG`, `Slate`, and `SlateCore`.
- Enable the `StateTree` and `GameplayStateTree` plugins if migrating the optional StateTree/EQS path. They are required in `GSGR.uproject` today.
- Replace `GSGR` module names, `GSGR_API` where applicable, `#include "GSGR.h"`, and `LogGSGR` with destination equivalents. Several combat UCLASS/USTRUCT declarations currently have no export macro; add `DESTINATIONMODULE_API` where a type must be referenced across destination modules.
- Preserve or revise the include-path strategy. Current includes assume the `Variant_Combat` subdirectories declared in `GSGR.Build.cs`.
- Existing serialized combat assets contain legacy `/Script/TP_ThirdPerson` imports. Recreating destination Blueprint subclasses avoids most of this. If assets are migrated, add narrowly scoped `CoreRedirects` for the actual old/current class paths to the destination classes.

### Gameplay framework assumptions

- `ACombatGameMode` is an authoritative `AGameModeBase` flow owner. No custom GameState is required.
- The prototype is local/single-player oriented and consistently uses player/controller index `0`.
- The active pawn must derive from `ACombatCharacter`; the active controller must derive from `ACombatPlayerController`.
- The level needs an `ACombatEnemySpawner`/`BP_CombatEnemySpawner` or an existing `ACombatEnemy` for the tutorial. The FTUE currently finds actors globally and uses the first suitable tutorial enemy/spawner. A destination level with several candidates must be checked for deterministic placement.
- Player starts may use tags such as `Player0`; otherwise the GameMode falls back to any PlayerStart.
- The player carries the actor tag `Player`; enemy attack traces rely on that tag.

### Character, skeleton, animation, and collision

- Use one mannequin-compatible skeleton for `BP_CombatCharacter`, `BP_CombatEnemy`, `ABP_Manny_Combat`, attack sequences, and montages, or retarget all of them consistently.
- Stationary combat intentionally sets player walk speed to zero, constrains movement to the arena plane, ignores montage root motion, and snaps attack translation back to the anchor. Dodge displacement is procedural; its montage is visual.
- Enemy approach speed comes from `BP_CombatEnemy` -> **Character Movement** -> **Max Walk Speed**. The current stationary enemy C++ adds movement input but does not set the speed.
- No project-defined collision channel/profile is required by this implementation. It uses built-in profiles/channels (`NoCollision`, `OverlapAllDynamic`, `BlockAllDynamic`, `ECC_Pawn`, `ECC_WorldDynamic`, and `ECC_Visibility`). Confirm the destination has not repurposed those behaviours.

### Input, profile, maps, and platform

- Enhanced Input must be active. The source `Config/DefaultInput.ini` selects `EnhancedPlayerInput` and `EnhancedInputComponent`.
- Decide whether the destination retains its own GameMode/PlayerController and composes these features, or uses destination subclasses of the combat classes. Do not replace a production GameMode blindly.
- Decide how FTUE completion maps to the destination profile. The source default slot is `GSGRPlayerProfile` and stores `bHasCompletedFTUE` in `UCombatFTUESaveGame`; a production profile should normally own the final truth through the integration hooks.
- The source project defaults to the Third Person map/GameMode, while `/Game/Variant_Combat/Lvl_Combat` overrides its GameMode to `BP_CombatGameMode`. Configure the destination under **Project Settings -> Maps & Modes** or per-map **World Settings -> GameMode Override** as appropriate.
- No PS5-specific code/configuration is present. For PS5, verify destination platform-user/local-player SaveGame behaviour, controller mappings, any input glyph/UI expectations, plugin availability, and a destination cook/package. This is **Requires UE 5.8.0 Verification** and platform verification.

## Exact recommended migration order

### 1. Move the C++ foundation and module dependencies

- **Move/create:** Combat interfaces, animation notifies, character/controller, enemy/spawner/AI controller, GameMode, FTUE SaveGame, run-flow/life-bar UI classes, and optional StateTree/EQS utility code.
- **Source:** `Source/GSGR/Variant_Combat/`, relevant `Source/GSGR/Variant_Platforming/Animation/AnimNotify_EndDash.*` only if retaining the source dash montage, and dependencies from `Source/GSGR/GSGR.Build.cs`.
- **Method:** Manual C++ integration; rename module-facing macros/includes/log category.
- **Needs first:** A working UE 5.8.0 destination C++ module and the required engine modules/plugins.
- **Destination setup:** Update destination `.Build.cs`; enable StateTree plugins only if that path will be used; preserve include layout or normalize includes locally.
- **Depends on this:** Every combat Blueprint, montage notify, UI subclass, StateTree asset, and integration binding.
- **Quick verification:** Build the destination Editor target and confirm the classes appear in the Class Viewer. Mark any rejected signature **Requires UE 5.8.0 Verification** and adjust against the UE 5.8.0 headers.

### 2. Create destination Blueprint subclasses and class wiring

- **Move/create:** Destination subclasses corresponding to `BP_CombatCharacter`, `BP_CombatPlayerController`, `BP_CombatEnemy`, `BP_CombatAIController`, `BP_CombatEnemySpawner`, `BP_CombatGameMode`, and `UI_LifeBar` where needed.
- **Source:** `/Game/Variant_Combat/Blueprints/` and `/Game/Variant_Combat/UI/UI_LifeBar` for defaults/reference.
- **Method:** Recreate in UE 5.8.0. Only try Unreal Migrate in a disposable destination copy if preserving complex Blueprint graphs is materially faster.
- **Needs first:** Step 1 C++ parents compiled and loadable.
- **Destination setup:** Assign destination character/controller classes in the destination GameMode, enemy class in the spawner, AI controller on the enemy, and life-bar class where exposed. Copy only relevant class defaults.
- **Depends on this:** Player/enemy spawning, map GameMode setup, content references, and destination-specific integration.
- **Quick verification:** Compile each Blueprint with no missing parent or source-module reference; spawn player and one enemy in a test level.

### 3. Bring compatible mannequin, skeleton, and base animation content

- **Move/create:** Manny/Quinn-compatible meshes/skeleton and locomotion/attack/dash base sequences actually used by the destination setup.
- **Source:** `/Game/Characters/Mannequins/` and dependencies reported by the combat animation assets.
- **Method:** Use destination equivalent or original UE 5.8.0-compatible assets. Use **Content Browser -> Asset Actions -> Migrate** only if the packages have been proven to load in 5.8.0.
- **Needs first:** Destination character Blueprint skeleton choice.
- **Destination setup:** Retarget sequences/AnimBP if the production skeleton differs; update mesh and Anim Class defaults on player/enemy Blueprints.
- **Depends on this:** Combat AnimBP and every combat montage.
- **Quick verification:** Preview the mesh and representative locomotion/attack sequences on the chosen skeleton without warnings.

### 4. Bring or recreate combat montages and supporting content

- **Move/create:** `ABP_Manny_Combat`, `AM_ComboAttack`, `AM_ChargedAttack`, a destination dodge montage, and selected VFX/camera-shake content.
- **Source:** `/Game/Variant_Combat/Anims/`, `/Game/Variant_Platforming/Anims/AM_Dash`, `/Game/Variant_Combat/VFX/NS_Damage`, `/Game/Variant_Combat/Blueprints/BP_CameraShake_Hit_Player`, and `/Game/Variant_Combat/Blueprints/BP_CameraShake_Hit_Enemy`.
- **Method:** For a tested candidate, select the top-level montage/AnimBP and use **Content Browser -> Asset Actions -> Migrate** so Unreal gathers dependencies. Because this is a downgrade, recreate rejected montages in UE 5.8.0. Prefer recreating the small dodge montage to remove the Platforming dependency.
- **Needs first:** C++ notify classes and a compatible skeleton/base sequences.
- **Destination setup:** Restore montage sections and notifies, assign montage properties on player/enemy Blueprint defaults, and keep root-motion handling consistent with the stationary code.
- **Depends on this:** Light/heavy attacks, attack traces, combo/charge branching, dodge visuals, and enemy attacks.
- **Quick verification:** Open each montage in 5.8.0, confirm its skeleton/sections/notifies resolve, then play one light, heavy, dodge, and enemy attack in-editor.

### 5. Recreate/verify input, optional StateTree/EQS, and UI assets

- **Move/create:** Enhanced Input actions/contexts, life-bar widget if used, and StateTree/EQS assets only if the destination needs non-stationary AI.
- **Source:** `/Game/Input/`, `/Game/Variant_Combat/Input/`, `/Game/Variant_Combat/UI/`, and `/Game/Variant_Combat/Blueprints/AI/`.
- **Method:** Recreate simple Input assets in UE 5.8.0. Recreate or explicitly verify serialized StateTree/EQS/UI assets. The startup/FTUE/Game Over overlay already comes from C++ and needs no Widget Blueprint.
- **Needs first:** Destination controller/character and, for StateTree, all custom C++ node structs.
- **Destination setup:** Assign actions on the character, contexts on the player controller, StateTree on the AI controller if used, and Widget classes on relevant defaults.
- **Depends on this:** Player control, displayed input prompts, life bars, and optional advanced enemy navigation.
- **Quick verification:** Confirm each action is mapped and its displayed prompt resolves; compile the life-bar Blueprint; validate StateTree/EQS assets in their editors if included.

### 6. Wire FTUE integration hooks into destination logic

- **Move/create:** Bind destination functions to `ACombatGameMode::OnFTUEStarted` and `ACombatGameMode::OnFTUECompleted`, or bind equivalent delegates if the destination wraps the reusable GameMode flow.
- **Source:** `CombatGameMode.h/.cpp`; see [FTUE integration hooks](#ftue-integration-hooks).
- **Method:** Blueprint event binding or C++ dynamic-multicast binding. Keep destination-specific functions outside the FTUE state-machine internals.
- **Needs first:** The destination GameMode/profile/service owners and compiled FTUE GameMode code.
- **Destination setup:** Route start/completion to production analytics, profile, unlock, or flow functions as required. Reconcile the prototype SaveGame with the production profile rather than creating two competing truths.
- **Depends on this:** Destination-specific FTUE side effects; the reusable tutorial itself does not.
- **Quick verification:** With a reset profile, observe each debug log once and confirm each destination callback runs once; returning completed profiles must not emit either hook.

### 7. Add the prototype level only if it is required

- **Move/create:** `/Game/Variant_Combat/Lvl_Combat` and its external actor/object packages, or reproduce only the necessary actors in a destination map.
- **Source:** `/Game/Variant_Combat/Lvl_Combat`, `Content/__ExternalActors__/Variant_Combat/Lvl_Combat/`, and `Content/__ExternalObjects__/Variant_Combat/Lvl_Combat/`.
- **Method:** Prefer the destination map and place a PlayerStart plus destination enemy spawner. If the prototype map is essential, select `Lvl_Combat` and use **Content Browser -> Asset Actions -> Migrate** only in a disposable UE 5.8.0 copy first.
- **Needs first:** All Blueprint classes/assets referenced by the map.
- **Destination setup:** Set the correct GameMode override, verify PlayerStart tags, ensure the tutorial has a suitable enemy/spawner, and choose the production Maps & Modes defaults.
- **Depends on this:** Only workflows that require the exact prototype level layout.
- **Quick verification:** Open the map without missing external actors, confirm the intended GameMode in World Settings, and perform a short Play-in-Editor spawn check.

## C++ migration reference

Compile this foundation in UE 5.8.0 before creating, migrating, or wiring dependent Blueprint assets.

### Run flow, player, and UI

Unless a full path is shown, header/CPP paths in the following C++ tables are relative to `Source/GSGR/Variant_Combat/`.

| Class | Header / CPP | Responsibility | Required destination module/dependencies | UE 5.8.0 concern |
| --- | --- | --- | --- | --- |
| `ACombatGameMode` | `Source/GSGR/Variant_Combat/CombatGameMode.h` / `.cpp` | Startup, FTUE states/hooks, survival run, Game Over, restart/quit, FTUE persistence orchestration | Game module; `Engine`, `UMG` | Delegate/SaveGame APIs are conventional; verify exact 5.8.0 compile. Replace `GSGR.h`/`LogGSGR`. |
| `ACombatPlayerController` | `CombatPlayerController.h` / `.cpp` | Mapping contexts, input-mode/UI ownership, input prompt lookup, flow button routing | Game module; `EnhancedInput`, `UMG`, `Slate`, `SlateCore` | `QueryKeysMappedToAction` and Enhanced Input signatures: **Requires UE 5.8.0 Verification**. |
| `ACombatCharacter` | `CombatCharacter.h` / `.cpp` | Stationary/player movement, attacks, dodge, health, root-motion containment, input handlers | Game module; `EnhancedInput`, `Engine` | CharacterMovement and montage/root-motion APIs: **Requires UE 5.8.0 Verification**. Contains a hardcoded soft asset path to `/Game/Variant_Platforming/Anims/AM_Dash.AM_Dash`. |
| `UCombatFTUESaveGame` | `CombatFTUESaveGame.h` / `.cpp` | Local-player FTUE completion record | Game module; `Engine` | `ULocalPlayerSaveGame` availability/signatures: **Requires UE 5.8.0 Verification**. |
| `UCombatRunWidget` | `UI/CombatRunWidget.h` / `.cpp` | Code-built Startup/FTUE/Game Over overlay | Game module; `UMG`, `Slate`, `SlateCore` | UMG widget-tree construction APIs: **Requires UE 5.8.0 Verification**. |
| `UCombatLifeBar` | `UI/CombatLifeBar.h` / `.cpp` | Base class/logic for `UI_LifeBar` | Game module; `UMG` | Low source risk; verify Blueprint parent after compile. |

### Enemy and AI

| Class | Header / CPP | Responsibility | Required destination module/dependencies | UE 5.8.0 concern |
| --- | --- | --- | --- | --- |
| `ACombatEnemy` | `AI/CombatEnemy.h` / `.cpp` | Health/attacks and default stationary Approach/Wait/Attack/Recovery AI | Game module; `Engine`, `AIModule` | Character/montage APIs: **Requires UE 5.8.0 Verification**. |
| `ACombatEnemySpawner` | `AI/CombatEnemySpawner.h` / `.cpp` | Initial spawn, respawn, count, spawn event | Game module; `Engine` | Low source risk. |
| `ACombatAIController` | `AI/CombatAIController.h` / `.cpp` | Optional StateTree controller; bypassed by stationary arena AI | Game module; `AIModule`, `GameplayStateTreeModule` | StateTree component APIs: **Requires UE 5.8.0 Verification**. |
| StateTree task/condition/evaluator structs | `AI/CombatStateTreeUtility.h` / `.cpp` | Optional combo/charged attacks, landing, facing, speed, and player-info StateTree nodes | Game module; `StateTreeModule`, `GameplayStateTreeModule`, `AIModule` | Highest source-API risk; compile against UE 5.8.0 headers before loading the StateTree. |
| `UEnvQueryContext_Player`, `UEnvQueryContext_Danger` | `AI/EnvQueryContext_Player.h/.cpp`, `AI/EnvQueryContext_Danger.h/.cpp` | EQS contexts for optional AI queries | Game module; `AIModule` | Verify EQS context signature in UE 5.8.0. |

### Combat contracts and animation notifies

| Classes | Paths | Responsibility | Dependencies / notes |
| --- | --- | --- | --- |
| `ICombatAttacker`, `ICombatDamageable`, `ICombatActivatable` | `Source/GSGR/Variant_Combat/Interfaces/*.h/.cpp` | Combat-facing interfaces | Move before the classes that implement/call them. Add destination export macros if used across modules. |
| `UAnimNotify_DoAttackTrace`, `UAnimNotify_CheckCombo`, `UAnimNotify_CheckChargedAttack` | `Source/GSGR/Variant_Combat/Animation/*.h/.cpp` | Montage-driven trace and combo/charge decisions | Must compile before migrated combat montages load their notify classes. |
| `UAnimNotify_EndDash` | `Source/GSGR/Variant_Platforming/Animation/AnimNotify_EndDash.h/.cpp` | Notify referenced by the current `AM_Dash` | Avoid this extra dependency by recreating a clean visual-only destination dodge montage when practical. |

Optional prototype-only activation/checkpoint/damageable actors under `Variant_Combat/Gameplay/` are not required for the core stationary FTUE/run unless the destination explicitly uses their Blueprint counterparts.

### Module-name and include changes

The destination must update:

- `IMPLEMENT_PRIMARY_GAME_MODULE`/module declarations only if copying the source module itself; normally integrate classes into the existing destination module instead.
- `GSGR_API` (and missing exports where cross-module access is needed) to `DESTINATIONMODULE_API`.
- `#include "GSGR.h"` and `LogGSGR` to the destination logging header/category.
- `GSGR/Variant_Combat/...` include roots if the destination directory/module layout changes.
- Any CoreRedirects required for migrated assets. Do not keep broad redirects that could capture unrelated destination classes.

## Unreal content migration

When an asset is a direct candidate, use **Content Browser -> Asset Actions -> Migrate** so Unreal gathers its dependencies. Select the smallest meaningful top-level owner:

- Player bundle: select `/Game/Variant_Combat/Blueprints/BP_CombatCharacter` after its C++ parent exists. This can collect the AnimBP, montages, UI/VFX, and input references, so review the dependency report carefully.
- Enemy bundle: select `/Game/Variant_Combat/Blueprints/AI/BP_CombatEnemySpawner` when the destination needs the prototype enemy/spawn setup. It collects the enemy and its downstream dependencies.
- Combat animation only: select the individual top-level montage (`AM_ComboAttack` or `AM_ChargedAttack`) after the skeleton and C++ notify classes exist.
- Exact prototype level: select `/Game/Variant_Combat/Lvl_Combat`; do this last and only if the level is required.

Because all of these source packages may have been saved by UE 5.8.1, “Migrate” describes dependency collection, not a guarantee that UE 5.8.0 can deserialize them.

### Classification summary

- **Direct migration candidate:** selected VFX/camera-shake assets that open successfully in a disposable 5.8.0 project and have no newer package dependency.
- **Recreate in UE 5.8.0:** Input Actions/Mapping Contexts, destination Blueprint subclasses/default assignments, and preferably the simple visual dodge montage.
- **Use destination equivalent:** production GameMode/profile/UI where they already exist; integrate reusable flow through subclassing/composition and the hooks.
- **Use original compatible asset:** Manny/Quinn skeleton, meshes, and engine/template animation content.
- **Requires verification:** combat BPs, `ABP_Manny_Combat`, attack montages/sequences, StateTree/EQS assets, `UI_LifeBar`, touch UI, VFX/materials, and `Lvl_Combat`.

## Recreation instructions for simple assets

### Enhanced Input

Create these in the UE 5.8.0 destination, then assign them to the matching properties on the destination character/controller Blueprint defaults.

| Asset | Source path | Value type | Context / handling |
| --- | --- | --- | --- |
| `IA_Move` | `/Game/Input/Actions/IA_Move` | Axis2D | Add to destination `IMC_Combat`; handled by `ACombatCharacter::Move`/`DoMove`. Stationary mode ignores movement. |
| `IA_Look` | `/Game/Input/Actions/IA_Look` | Axis2D | `IMC_Combat`; handled by `Look`/`DoLook`. |
| `IA_MouseLook` | `/Game/Input/Actions/IA_MouseLook` | Axis2D | `/Game/Input/IMC_MouseLook`; handled by `Look`/`DoLook`. |
| `IA_Jump` | `/Game/Input/Actions/IA_Jump` | Boolean | In this combat setup it is the dodge action (Space and gamepad face-button-left in the source context); handled by `BackDodgePressed`/`DoBackDodge`. |
| `IA_ComboAttack` | `/Game/Variant_Combat/Input/Actions/IA_ComboAttack` | Boolean | `IMC_Combat`; handled by `DoComboAttackStart`. |
| `IA_ChargedAttack` | `/Game/Variant_Combat/Input/Actions/IA_ChargedAttack` | Boolean | `IMC_Combat`; Started/Completed drive `DoChargedAttackStart`/`DoChargedAttackEnd`. |
| `IA_ToggleCameraSide` | `/Game/Variant_Combat/Input/Actions/IA_ToggleCameraSide` | Boolean | `IMC_Combat`; handled by `ACombatCharacter::ToggleCamera()`. |

Recreate `/Game/Variant_Combat/Input/IMC_Combat` and `/Game/Input/IMC_MouseLook` in UE 5.8.0, matching the source key choices or the destination control scheme. Assign `IMC_Combat` and `IMC_MouseLook` to the default mapping-context list on the destination `BP_CombatPlayerController`. The FTUE UI asks `ACombatPlayerController` for the key currently mapped to each action, so correct assignments also drive prompt text.

### Destination Blueprint subclasses

Create the C++ parent first, then a destination Blueprint child. Copy only the needed defaults:

1. Character: mesh/Anim Class, attack/dodge montages, Input Actions, life-bar/VFX/camera shake references, and Stationary Combat/Dodge/Melee tuning.
2. PlayerController: default mapping contexts, character class, and optional touch interface.
3. Enemy: mesh/Anim Class, attack montages, AI controller, life bar/VFX, Character Movement speed, and Stationary Arena tuning.
4. AIController: StateTree only if using the optional non-stationary path.
5. Spawner: enemy class, count, initial-spawn setting, and respawn delay.
6. GameMode: destination character/controller classes and `FTUE | Timing`/`FTUE | Persistence` defaults.

### Dodge montage

The code performs dodge displacement and timing. In UE 5.8.0, a small montage using a compatible backward-dodge/dash sequence can replace `AM_Dash`; assign it to `BP_CombatCharacter` -> **Dodge** -> `DodgeMontage`. Avoid translational root motion or keep it ignored, and do not copy `AnimNotify_EndDash` unless destination logic actually needs it.

### Life bar widget

After `UCombatLifeBar` compiles, create a UE 5.8.0 Widget Blueprint child corresponding to `UI_LifeBar`. Recreate its progress-bar presentation and implement the inherited `SetLifePercentage(float)` and `SetBarColor(FLinearColor)` Blueprint events. Assign the resulting class on the destination player/enemy defaults where the life-bar class is exposed.

## Blueprint ownership and parent relationships

| Asset | C++ parent | Key ownership/wiring |
| --- | --- | --- |
| `/Game/Variant_Combat/Blueprints/BP_CombatGameMode` | `ACombatGameMode` | Character/controller classes, FTUE timing/persistence, run ownership. |
| `/Game/Variant_Combat/Blueprints/BP_CombatCharacter` | `ACombatCharacter` | Mesh/AnimBP, player Input Actions, combat/dodge content and tuning. |
| `/Game/Variant_Combat/Blueprints/BP_CombatPlayerController` | `ACombatPlayerController` | Mapping Contexts, pawn class, optional touch controls. |
| `/Game/Variant_Combat/Blueprints/AI/BP_CombatEnemy` | `ACombatEnemy` | Movement speed, arena pacing, attacks, health, mesh/AnimBP, AI controller. |
| `/Game/Variant_Combat/Blueprints/AI/BP_CombatAIController` | `ACombatAIController` | Optional `ST_CombatEnemy` assignment. |
| `/Game/Variant_Combat/Blueprints/AI/BP_CombatEnemySpawner` | `ACombatEnemySpawner` | Enemy class and spawn/respawn defaults. |
| `/Game/Variant_Combat/UI/UI_LifeBar` | `UCombatLifeBar` | Character/enemy health display. |

The startup menu, FTUE prompts, completion panel, and Game Over panel are not Blueprint widgets. `ACombatPlayerController` creates `UCombatRunWidget` directly and the GameMode changes its displayed state.

If the destination already owns equivalent Blueprints, retain them and either reparent a dedicated child to the new C++ parent or copy only the required component/default wiring. Do not overwrite production GameMode/Character assets. C++ parents must compile before any reparenting or migration attempt.

## Project settings and configuration

Unreal Migrate does not transfer module/plugin/project configuration. Check these manually:

- **Destination module `.Build.cs`:** add `EnhancedInput`, `AIModule`, `UMG`, `Slate`, and `SlateCore`; add `StateTreeModule`/`GameplayStateTreeModule` only when their C++ types are retained.
- **Edit -> Plugins:** enable `StateTree` and `GameplayStateTree` only for the optional StateTree path. The source project’s Modeling, MCP, Terminal, and EditorToolset plugins are not gameplay migration requirements.
- **Config/DefaultInput.ini / Project Settings -> Input:** use Enhanced Player Input and Enhanced Input Component; retain the console key if `FTUE.Reset` must be entered through the in-editor console.
- **Project Settings -> Maps & Modes** and **World Settings -> GameMode Override:** choose the destination GameMode deliberately. The source global defaults are Third Person; only `Lvl_Combat` overrides to `BP_CombatGameMode`.
- **Config/DefaultEngine.ini -> CoreRedirects:** add only redirects required by any migrated source assets. Source combat assets may still reference `/Script/TP_ThirdPerson`.
- **Collision:** no custom channel must be copied; confirm destination built-in Pawn/WorldDynamic/Visibility responses still satisfy combat traces and spawning.
- **Save/profile:** set `ACombatGameMode` -> **FTUE | Persistence** -> `FTUEProfileSlotName` or replace prototype persistence through destination integration. The default is `GSGRPlayerProfile`.
- **Gameplay Tags:** none are required by the Gameplay Tags subsystem. The implementation uses the ordinary actor tag `Player`.
- **Console command:** `FTUE.Reset` is registered in C++ for non-Shipping builds; it is not an `.ini` console-command entry.

## FTUE integration hooks

### Start

- **Owner:** `ACombatGameMode`
- **Header:** `Source/GSGR/Variant_Combat/CombatGameMode.h`
- **Declaration:** Blueprint-assignable dynamic multicast delegate property `OnFTUEStarted` (currently near line 45)
- **Emitter:** `ACombatGameMode::BeginFTUE()` in `Source/GSGR/Variant_Combat/CombatGameMode.cpp` (function starts near line 183; broadcast is currently near line 196)
- **Meaning:** Fires once only after Play is selected, the loaded profile requires FTUE, and the FTUE flow has actually entered `Welcome`.
- **Debug log:** `FTUE Started integration hook fired`

Destination concept:

```text
OnFTUEStarted
  -> DestinationFunction_OnFTUEStart()
```

### Completion

- **Owner:** `ACombatGameMode`
- **Header:** `Source/GSGR/Variant_Combat/CombatGameMode.h`
- **Declaration:** Blueprint-assignable dynamic multicast delegate property `OnFTUECompleted` (currently near line 49)
- **Emitter:** `ACombatGameMode::CompleteFTUE()` in `Source/GSGR/Variant_Combat/CombatGameMode.cpp` (function starts near line 386; broadcast is currently near line 396)
- **Meaning:** Fires once only after the full Welcome -> Light Attack -> Heavy Attack -> Dodge sequence succeeds and completion is saved/normal gameplay begins.
- **Debug log:** `FTUE Completed integration hook fired`

Destination concept:

```text
OnFTUECompleted
  -> DestinationFunction_OnFTUEComplete()
```

Bind to these public hooks from destination C++ or Blueprint. The destination does not need to edit `EnterFTUEState`, attack success handlers, or other FTUE internals.

## FTUE state flow

State ownership is `ACombatGameMode`; transitions are applied by `EnterFTUEState()`.

| State | Brief behaviour | Primary owner/function |
| --- | --- | --- |
| `None` | No active FTUE state; startup/normal flow decides what follows. | `InitializeRunFlow()`, `ShowStartupMenu()` |
| `Welcome` | Shows welcome copy and waits for a deliberate input press/release. | `EnterFTUEState(Welcome)` plus `ACombatPlayerController::InputKey()` |
| `LightAttack` | Enables only light attack and waits for the tutorial enemy to be damaged by that attack type. | `EnterFTUEState(LightAttack)`, `HandleEnemyDamaged()` |
| `HeavyAttack` | Enables heavy attack and waits for the matching successful damage. | `EnterFTUEState(HeavyAttack)`, `HandleEnemyDamaged()` |
| `Dodge` | Controls a tutorial enemy attack, shows the dodge prompt, and waits for player dodge start. | `EnterFTUEState(Dodge)`, `HandleTutorialDodgeWindow()`, `HandlePlayerDodgeStarted()` |
| `Complete` | Persists completion, cleans up FTUE, starts the survival run, and emits the completion hook once. | `EnterFTUEState(Complete)`, `CompleteFTUE()` |

Timing properties are owned by `ACombatGameMode` and exposed in Blueprint Defaults under **FTUE | Timing**: `WelcomeToLightDelay`, `LightSuccessFeedbackDuration`, `HeavySuccessFeedbackDuration`, `DodgeAttackStartDelay`, `DodgePromptDelay`, `DodgeSuccessFeedbackDuration`, and `TutorialCompleteFeedbackDuration`.

## Post-migration verification checklist

1. [ ] Destination Editor target compiles in UE 5.8.0.
2. [ ] Destination Blueprint parents resolve and critical Blueprints compile without errors.
3. [ ] Correct GameMode, PlayerController, pawn, enemy, and spawner classes are selected.
4. [ ] Player spawns and the stationary/non-stationary movement setting is intentional.
5. [ ] Player speed/settings resolve; stationary mode remains anchored and non-stationary speed is correct.
6. [ ] Light/combo attack, sections, notifies, traces, and damage work.
7. [ ] Heavy/charged attack, charge/release, notifies, traces, and damage work.
8. [ ] Dodge animation, procedural out/back movement, cooldown, and invulnerability work.
9. [ ] Attack montage translation does not displace the stationary player; root-motion settings match the source intent.
10. [ ] Enemy speed, approach, attack windup, cooldown/variation, damage, death, and respawn work.
11. [ ] If included, StateTree and EQS assets load and the non-stationary AI path runs.
12. [ ] Startup menu appears; Play, Quit, and input modes work.
13. [ ] With a fresh/reset profile, FTUE starts after Play and `OnFTUEStarted` plus its destination callback fires exactly once.
14. [ ] Welcome, Light Attack, Heavy Attack, and Dodge stages advance only on the intended actions.
15. [ ] Successful completion emits `OnFTUECompleted` plus its destination callback exactly once.
16. [ ] FTUE completion persists after restarting the editor/session as intended by the destination profile.
17. [ ] `FTUE.Reset` resets the correct local profile/slot and permits FTUE replay.
18. [ ] A returning completed profile skips FTUE and does not emit either integration hook.
19. [ ] Survival timer starts after FTUE/normal Play and displays correctly.
20. [ ] Player death shows Game Over with the final time; Restart and Quit work.
21. [ ] Run one destination cook/package sanity check after editor validation; include the intended platform (PS5 where applicable).

## Common migration problems

| Symptom | Likely project-specific cause | Concise fix |
| --- | --- | --- |
| Link/import error for a combat class | Source module name/API macro remains, or a class needed across modules has no export macro | Replace module-facing names and add `DESTINATIONMODULE_API` where required. |
| Compile failure for input/UI/AI code | Missing `.Build.cs` dependency | Add the relevant module from the source dependency list; enable StateTree plugins only when used. |
| UE 5.8.0 rejects StateTree node code | Patch-level StateTree signature difference | Compare against UE 5.8.0 headers and adapt locally; mark/track as **Requires UE 5.8.0 Verification**. |
| Blueprint parent is missing | C++ was not compiled first or asset still points to `/Script/GSGR`/`/Script/TP_ThirdPerson` | Compile the destination parent, recreate/reparent the BP, or add a precise CoreRedirect. |
| A 5.8.1 asset will not load | Serialized package is newer than the destination reader supports | Recreate it in 5.8.0 or use an original compatible asset; do not edit package metadata. |
| Character or montage will not animate | Skeleton/AnimBP mismatch | Retarget consistently or use the destination mannequin set; reassign Anim Class/montage defaults. |
| Stationary attacks move the character | Root-motion mode or stationary anchor setup differs | Verify `bStationaryCombatMode`, `BeginPlay()` root-motion settings, and attack-end anchor restoration. |
| Dodge movement doubles or ends incorrectly | Montage root motion is also translating the character, or source dash notify was copied without need | Use a visual-only/ignored-root-motion dodge montage and rely on `DoBackDodge()` procedural movement. |
| Player input or FTUE prompt is missing | Input Action not assigned, Mapping Context not applied, or key absent | Recreate/assign actions and add contexts through `ACombatPlayerController`; confirm the action has a mapped key. |
| Enemy does not move/attacks too quickly | `BP_CombatEnemy` movement speed or Stationary Arena timing differs | Check **Character Movement -> Max Walk Speed** and `ArenaAttackRange`, windup, cooldown, and variation defaults. |
| StateTree appears unused | `bUseStationaryArenaAI` is true | This is expected for the current simple arena path; disable it only when intentionally using StateTree AI. |
| FTUE cannot find/control the intended enemy | No enemy/spawner exists, or multiple candidates make the first global match unsuitable | Place a single intended tutorial spawner/enemy and verify it is the one discovered. |
| Startup/FTUE/Game Over UI does not appear | Looking for an absent Widget Blueprint or wrong controller/GameMode is active | Use `ACombatPlayerController` + `UCombatRunWidget` and confirm the map’s GameMode override. |
| Completion is stored in the wrong place | Prototype `GSGRPlayerProfile` conflicts with production profile architecture | Set the slot intentionally and route the hooks into the destination profile; keep a single production source of truth. |
| `FTUE.Reset` affects nothing | Shipping build, wrong active GameMode, or wrong profile slot | Use a non-Shipping build with `ACombatGameMode` active and verify `FTUEProfileSlotName`; restart/re-enter flow after reset. |
| Destination functions never run | Delegates were not bound on the active GameMode instance | Bind `OnFTUEStarted`/`OnFTUECompleted` in the destination subclass/owner and verify the hook logs first. |
| Correct classes exist but the wrong experience starts | Destination Maps & Modes or per-level override still selects another GameMode | Set the intended default/override explicitly; do not rely on source global defaults. |
