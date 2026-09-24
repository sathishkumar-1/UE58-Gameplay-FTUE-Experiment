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
attack pose before showing **Dodge Now**. These control timing *within* Dodge,
not the gap between lessons. After editing defaults, compile and save
`BP_CombatGameMode`, then replay from the main menu to check the pacing.

The Space dodge itself uses `BP_CombatCharacter`'s **Dodge | Timing** and
**Dodge | Movement** defaults. Its mesh now stays facing the opponent (screen
right) while the unchanged procedural movement carries it backward and returns
it to the fixed combat position. The `AM_Dash` montage, camera, capsule facing,
invulnerability, and movement distance are unchanged.

## Demo controls

| Input | Action |
| --- | --- |
| `.` / gamepad D-pad Right | Skip the current Light, Heavy, Dodge, Evade, or basic encounter event |
| `1` / gamepad D-pad Up | Open the showcase immediately |
| `2` / gamepad D-pad Down | Jump to the post-showcase flurry encounter |
| Esc / gamepad Y / reminder X | Dismiss the one-time Evade reminder |

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
