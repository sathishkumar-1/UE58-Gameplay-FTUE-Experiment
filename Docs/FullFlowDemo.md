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

The Dodge and Evade lessons keep the player safe from staged damage. Failed
attempts clear the enemy's attack and timers, then replay the same lesson with
a fresh enemy. Evade succeeds only when a flurry trace reaches the player while
frontal evade is held.

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
