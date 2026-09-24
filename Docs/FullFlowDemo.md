# Full Flow demo

The project starts in `/Game/Variant_Combat/Level_Main_Menu`. Play opens
`Level_Full_Flow` and begins the tutorial every time, independent of the saved
FTUE completion flag. The original `Lvl_Combat` still uses its saved-profile
startup behavior. `FTUE.Reset` remains the only profile reset command.

## Route

Welcome -> Light Attack -> Heavy Attack -> Dodge -> Evade a real flurry hit ->
one basic-enemy encounter -> `Level_Control_Recap_Showcase` -> Return -> one
flurry enemy -> one-at-a-time randomized basic/flurry spawning. Showcase return
restores full health and resets the survival timer. The existing
`OnFTUECompleted` hook fires after Evade; `OnShowcaseDeparted` and
`OnShowcaseReturned` mark the travel boundary.

## Integration callbacks

All four callbacks are `BlueprintAssignable` dynamic multicast delegates on
`ACombatGameMode`, declared in `Source/GSGR/Variant_Combat/CombatGameMode.h`.
They take no parameters. The demo maps use `/Game/Variant_Combat/Blueprints/BP_CombatGameMode`
as their World Settings GameMode override.

| Delegate | When it fires | Where it fires |
| --- | --- | --- |
| `OnFTUEStarted` | After the tutorial enters Welcome, once per GameMode instance. Full Flow does this on every Play, regardless of the saved FTUE flag. | `Level_Full_Flow`; also `Lvl_Combat` when its saved profile requires FTUE. |
| `OnFTUECompleted` | After tutorial completion; in Full Flow, the basic encounter has already been started. Once per GameMode instance. | `Level_Full_Flow`; also `Lvl_Combat` after its FTUE. |
| `OnShowcaseDeparted` | Immediately before `OpenLevel` leaves Full Flow for the showcase, including the direct `1` jump. | The **old** `Level_Full_Flow` GameMode. |
| `OnShowcaseReturned` | When a **new** Full Flow GameMode starts with the `FromShowcase=1` travel option, before the post-showcase flurry starts on the next tick. | The **new** `Level_Full_Flow` GameMode. |

`OnShowcaseDeparted` marks departure, not arrival in the showcase map. To run
logic after the showcase map loads, use that map's Level Blueprint `Event BeginPlay`
or a showcase actor's `BeginPlay`. `OnShowcaseReturned` does not fire for the
direct `2` jump, which stays in the same level. `OpenLevel` creates a new world
and GameMode, so bindings and ordinary instance state from before travel do not
survive the transition. Persist any needed data separately or pass it through
travel options. The return broadcast runs before the new player's widget setup;
schedule player/UI work for a later tick or the relevant player's `BeginPlay`.

### Blueprint: bind in the GameMode Blueprint

1. Open `Content/Variant_Combat/Blueprints/BP_CombatGameMode` and its Event Graph.
   Add `Event BeginPlay` if needed.
2. From `Self`, add **Bind Event to On FTUE Started**, **Bind Event to On FTUE
   Completed**, **Bind Event to On Showcase Departed**, and **Bind Event to On
   Showcase Returned** for the callbacks you need. Connect a matching custom
   event to each node's `Event` pin, and put your setup logic in those custom
   events. The events have no input pins.
3. Execute the bind nodes **before** `Call to Parent Function` on `BeginPlay`.
   The parent `ACombatGameMode::BeginPlay()` can broadcast `OnFTUEStarted` or
   `OnShowcaseReturned` during startup. Binding afterward misses that broadcast.
   Keep the parent call so the normal route still initializes.
4. Compile and save the Blueprint. In PIE, run the route from the menu, enter
   the showcase, and click Return. Put a Print String or breakpoint in each
   custom event while connecting your own showcase systems.

If another Blueprint owns the response, bind through the active GameMode or
forward the event from `BP_CombatGameMode`. An arbitrary actor's `BeginPlay` is
not a reliable place to catch the two startup broadcasts, because its order
relative to GameMode initialization is not guaranteed. GameMode exists only on
the authoritative side, so clients need an explicit replicated handoff for
client-side effects.

### C++: bind before the base GameMode starts

Subclass `ACombatGameMode` and select that subclass (or a Blueprint based on it)
as the map's GameMode. Declare each handler with `UFUNCTION()` in your subclass
header; then bind in its `BeginPlay` before calling `Super::BeginPlay()`:

```cpp
void AMyCombatGameMode::BeginPlay()
{
    OnFTUEStarted.AddUniqueDynamic(this, &AMyCombatGameMode::HandleFTUEStarted);
    OnFTUECompleted.AddUniqueDynamic(this, &AMyCombatGameMode::HandleFTUECompleted);
    OnShowcaseDeparted.AddUniqueDynamic(this, &AMyCombatGameMode::HandleShowcaseDeparted);
    OnShowcaseReturned.AddUniqueDynamic(this, &AMyCombatGameMode::HandleShowcaseReturned);
    Super::BeginPlay();
}
```

The handler signatures are `void HandleFTUEStarted()`,
`void HandleFTUECompleted()`, `void HandleShowcaseDeparted()`, and
`void HandleShowcaseReturned()`. Implement only the handlers you bind. If a
separate C++ object listens, get the authoritative mode with
`GetWorld()->GetAuthGameMode<ACombatGameMode>()` and bind early enough for the
startup broadcasts; otherwise put the startup handling in the GameMode subclass.

The Dodge and Evade lessons keep the player safe from staged damage. Failed
attempts clear the enemy's attack and timers, then replay the same lesson with
a fresh enemy. Evade succeeds only when a flurry trace reaches the player while
frontal evade is held.

## Tutorial timing and auto continuation

Open `/Game/Variant_Combat/Blueprints/BP_CombatGameMode`, choose **Class Defaults**,
and search for the property names below. They are in **FTUE | Timing** except
`BasicEncounterToShowcaseDelay`, which is in **Full Flow | Timing**. All values
are seconds and can be changed without editing C++. The values below are the
C++ defaults; a saved Blueprint override takes precedence.

| Transition or wait | Class Default property | C++ default |
| --- | --- | ---: |
| Welcome confirmation -> Light lesson | `WelcomeToLightDelay` | 0.7 |
| Successful Light hit -> Heavy lesson | `LightSuccessFeedbackDuration` | 0.7 |
| Successful Heavy hit -> Dodge lesson | `HeavySuccessFeedbackDuration` | 0.7 |
| FTUE Evade flurry windup hold before the burst | `EvadePromptDelay` | 0.7 |
| Dodge success -> Evade lesson | `DodgeSuccessFeedbackDuration` | 0.7 |
| Evade success -> tutorial completion and basic encounter | `EvadeSuccessFeedbackDuration` | 0.7 |
| Tutorial Complete message stays visible | `TutorialCompleteFeedbackDuration` | 1.25 |
| Basic enemy death -> showcase travel | `BasicEncounterToShowcaseDelay` | 1.0 |

Welcome waits for player confirmation; that input starts the first timer. The
other lesson transitions happen after the matching successful action. The basic
encounter starts when the Evade success timer completes; the Tutorial Complete
message duration controls its overlay only and does not delay enemy spawning.
The basic enemy must die before the showcase travel timer begins. Direct skip
and jump controls bypass the corresponding success timer.

`DodgeAttackStartDelay` (0 seconds) waits after the Dodge lesson appears before
the enemy starts its attack. `DodgePromptDelay` (0 seconds) holds the enemy's
attack pose before showing **Dodge Now**. `EvadePromptDelay` adds time to the
Evade lesson's existing flurry windup; the enemy draws its fist back at the
normal speed, holds the windup pose for the extra delay, then starts its burst.
It applies only to the FTUE Evade enemy. The ordinary flurry and post-showcase
reminder still use `BP_FlurryEnemy`'s `FlurryWindupDuration` without this extra
delay. These control timing *within* a lesson, not the gap between lessons.
After editing defaults, compile and save
`BP_CombatGameMode`, then replay from the main menu to check the pacing.

The Space dodge itself uses `BP_CombatCharacter`'s **Dodge | Timing** and
**Dodge | Movement** defaults. Its mesh now stays facing the opponent (screen
right) while the unchanged procedural movement carries it backward and returns
it to the fixed combat position. The `AM_Dash` montage, camera, capsule facing,
invulnerability, and movement distance are unchanged.

## Where to replace combat animations

Open each Blueprint, select **Class Defaults**, and search for the property
name. Compile and save after changing a value. The active player, basic enemy,
and flurry enemy meshes currently use `SKM_DKM_Full` with the Dark Knight male
skeleton (`SK_DKM_Full`); replacement sequences should use that skeleton or be
retargeted to it. The Blueprint's mesh component also selects
`ABP_Manny_Combat` as its **Anim Class** for the underlying idle/movement pose.

| Blueprint | Property and category | Current animation |
| --- | --- | --- |
| `BP_CombatCharacter` | `ComboAttackMontage` — Melee Attack / Combo | `/Game/Variant_Combat/Anims/AM_ComboAttack` |
| `BP_CombatCharacter` | `ChargedAttackMontage` — Melee Attack / Charged | `/Game/Variant_Combat/Anims/AM_ChargedAttack` |
| `BP_CombatCharacter` | `DodgeMontage` — Dodge | `/Game/Variant_Platforming/Anims/AM_Dash` |
| `BP_CombatCharacter` | `EvadeAnimations` — Evade | `/Game/Variant_Combat/Anims/MIxamo/Retargeted/Center_Block`, `Left_Block`, `Right_Block` |
| `BP_CombatCharacter` | `DeathAnimation` — Damage / Animation | `/Game/Dark_Knight/Dark_Knight_Male/Animations/Anim_DKM_Death` |
| `BP_CombatEnemy` and `BP_FlurryEnemy` | `ComboAttackMontage` — Melee Attack / Combo | `/Game/Variant_Combat/Anims/AM_ComboAttack` |
| `BP_CombatEnemy` and `BP_FlurryEnemy` | `ChargedAttackMontage` — Melee Attack / Charged | `/Game/Variant_Combat/Anims/AM_ChargedAttack` |
| `BP_FlurryEnemy` | `ExhaustedAnimation` — Flurry Enemy / Animation | `/Game/Characters/Mannequins/Anims/Rifle/HitReact/MM_HitReact_Front_Hvy_01` |
| `BP_CombatEnemy` and `BP_FlurryEnemy` | `DeathAnimation` — Damage / Animation | `/Game/Dark_Knight/Dark_Knight_Male/Animations/Anim_DKM_Death` |

`BP_FlurryEnemy` inherits the attack and death fields from `BP_CombatEnemy`;
you can override them separately. Its flurry windup poses the
`ChargedAttackMontage` (or the combo montage if no charged montage is set),
and its burst plays the `ComboAttackMontage`. The Dodge tutorial also uses the
enemy combo montage. `ComboSectionNames`, `ChargeLoopSection`, and
`ChargeAttackSection` must match the sections in replacement montages.
`EvadePlayRate`, `EvadeBlendTime`, and `FlurryPlayRate` tune playback without
replacing the assets. Player and enemy death animations play once in place of
death ragdolls; the player death flow waits for the clip before showing Game
Over or restarting a failed tutorial attempt.

## Demo controls

| Input | Action |
| --- | --- |
| `.` / gamepad D-pad Right | Skip the current Light, Heavy, Dodge, Evade, or basic encounter event |
| `1` / gamepad D-pad Up | Open the showcase immediately |
| `2` / gamepad D-pad Down | Jump to the post-showcase flurry encounter |
| Esc / gamepad Y / reminder X | Dismiss the one-time Evade reminder |

### Change keyboard, mouse, and gamepad controls

For combat, open `Content/Variant_Combat/Input/IMC_Combat` in the Content
Browser. Its **Mappings** list pairs each Input Action with keyboard, mouse,
and gamepad keys. Change a key there, or add another mapping for the same
action, then save the mapping context and test in PIE. `BP_CombatPlayerController`
loads `IMC_Combat` from its **Default Mapping Contexts** class default.
`BP_CombatCharacter` selects the Input Action assets in its **Class Defaults**;
`ACombatCharacter::SetupPlayerInputComponent` binds those actions to gameplay.
You normally change a key in the mapping context, without editing the action
or C++ binding. For an analog stick, keep the action's axis type and inspect
the mapping's Dead Zone, Scalar, Swizzle, and Negate modifiers before replacing
its key.

| Action in `IMC_Combat` | Current PC input | Current gamepad input | Input Action |
| --- | --- | --- | --- |
| Move | WASD or arrow keys | Left stick (`Gamepad Left 2D`) | `/Game/Input/Actions/IA_Move` |
| Look | Mouse movement via `IMC_MouseLook` | Right stick (`Gamepad Right 2D`) | `/Game/Input/Actions/IA_Look` for stick; `/Game/Input/Actions/IA_MouseLook` for mouse |
| Light attack | Left mouse button | Right shoulder | `/Game/Variant_Combat/Input/Actions/IA_ComboAttack` |
| Heavy attack | Right mouse button | Right trigger axis | `/Game/Variant_Combat/Input/Actions/IA_ChargedAttack` |
| Back dodge | Space | Face button Left | `/Game/Input/Actions/IA_Jump` |
| Hold Evade | F | Left shoulder | `/Game/Variant_Combat/Input/Actions/IA_Evade` |
| Toggle camera side | R | D-pad Down | `/Game/Variant_Combat/Input/Actions/IA_ToggleCameraSide` |

`IMC_MouseLook` is in `BP_CombatPlayerController`'s **Mobile Excluded Mapping
Contexts** and maps `Mouse 2D` to `IA_MouseLook`. The left and right sticks
are already in `IMC_Combat`; console gamepads use those mappings when the
controller supplies the corresponding Unreal gamepad keys. The D-pad Down
camera mapping is consumed by the Full Flow direct-jump shortcut there; use a
different camera key if you need both actions in that level.

The demo-only shortcuts in the table above are **hard-coded key checks**, not
entries in `IMC_Combat`. To change their PC or gamepad keys, edit
`ACombatPlayerController::InputKey` in
`Source/GSGR/Variant_Combat/CombatPlayerController.cpp`:

| Demo action | Current `EKeys` checks |
| --- | --- |
| Skip one event | `Period`, `Gamepad_DPad_Right` |
| Jump to showcase | `One`, `Gamepad_DPad_Up` |
| Jump to post-showcase flurry | `Two`, `Gamepad_DPad_Down` |
| Dismiss reminder | `Escape`, `Gamepad_FaceButton_Top` (Y/Triangle position) |

The reminder X is a UI button in `UCombatRunWidget`, not a keyboard binding.
Welcome accepts any deliberate digital keyboard/gamepad button after it is
pressed and released. For an easier asset-only remapping workflow in the
future, create Input Actions for these demo commands, map them in a context,
and bind them in the controller; the current code does not do that. The
on-screen Evade hint reads the active Input Action mappings, while some
reminder text and this guide name specific keys and should be updated if you
change them. After a C++ shortcut change, do a normal editor build and replay
the menu and jump paths.

### Optional main-menu buttons for direct destinations

The current menu is built in `UCombatRunWidget::BuildWidgetTree` in
`Source/GSGR/Variant_Combat/UI/CombatRunWidget.cpp`; it is not a separate Widget
Blueprint. `NativeOnInitialized` binds button clicks, and `ShowStartupMenu`
chooses which buttons are visible. The existing Play click calls
`ACombatPlayerController::HandlePlaySelected`, then
`ACombatGameMode::HandlePlaySelected`, which unpauses and opens
`Level_Full_Flow`. For two new C++ buttons, follow that same widget ->
controller -> GameMode path. If you replace the menu with a Widget Blueprint,
its button `OnClicked` events can use **Set Game Paused** (false) and **Open
Level (by Name)** with the same level and option strings below.

Pass the selected destination through an `OpenLevel` **Options** string, for
example `StartAtShowcase=1` or `StartAtFlurry=1`, while opening
`Level_Full_Flow`. The two menu handlers can use these calls after unpausing:

```cpp
UGameplayStatics::OpenLevel(this, TEXT("Level_Full_Flow"), true, TEXT("StartAtShowcase=1"));
UGameplayStatics::OpenLevel(this, TEXT("Level_Full_Flow"), true, TEXT("StartAtFlurry=1"));
```

Use one call per button. In the *new* `ACombatGameMode::InitializeRunFlow`, check
`UGameplayStatics::HasOption(OptionsString, TEXT("StartAtShowcase"))` or
`StartAtFlurry` in the `bFullFlowDemo` branch **before** its normal `BeginFTUE()`
path. Schedule `JumpToShowcase()` or `JumpToPostShowcase()` for the next tick
with `GetWorldTimerManager().SetTimerForNextTick`, then return from that branch.
For example, after the existing `FromShowcase` case:

```cpp
if (UGameplayStatics::HasOption(OptionsString, TEXT("StartAtShowcase")))
{
    GetWorldTimerManager().SetTimerForNextTick(this, &ACombatGameMode::JumpToShowcase);
    return;
}
if (UGameplayStatics::HasOption(OptionsString, TEXT("StartAtFlurry")))
{
    GetWorldTimerManager().SetTimerForNextTick(this, &ACombatGameMode::JumpToPostShowcase);
    return;
}
BeginFTUE();
```

Those are the same destination methods called by `1`/D-pad Up and `2`/D-pad
Down once Full Flow is running. Waiting a tick also lets the newly loaded
player and widget initialize before the flurry route resets health. Keep the
existing `FromShowcase=1` check ahead of the new start-option checks so the
showcase Return path still fires `OnShowcaseReturned`.

The menu's GameMode is a different instance from Full Flow's. Calling
`JumpToShowcase()` or `JumpToPostShowcase()` on the menu instance has no effect
because both require `bFullFlowDemo`. Sending a fake `1` or `2` key from the
menu is therefore not a substitute for a travel option. Opening the showcase
map directly would also bypass the Full Flow `OnShowcaseDeparted` callback.
With the travel-option route, `JumpToShowcase()` fires that callback; the
flurry shortcut stays in Full Flow and does not fire `OnShowcaseReturned`.
Because the example handles the option before `BeginFTUE()`, it does not fire
`OnFTUEStarted`. To match the literal in-level key press, first call
`BeginFTUE()`, then schedule the jump for the next tick instead.

The post-showcase flurry uses the existing random attack choice. Its first
flurry windup pauses the world and shows a left-side reminder for up to three
seconds of real time. The card shows a cropped screenshot of the flurry attack
alongside the Evade instructions, then slides out before gameplay resumes.
The source capture is `Art/UI/ReminderFlurrySource.png`; the in-game texture is
`/Game/Variant_Combat/UI/T_FlurryReminder`.

## Verification after C++ compile

Open the main menu and play the full route. Check each single-step skip,
both direct jumps, failed Dodge and Evade retries, showcase Return, health and
timer reset, three reminder dismissal paths, and multiple mixed respawns.
Compile affected Blueprints and inspect PIE logs for missing class references.
