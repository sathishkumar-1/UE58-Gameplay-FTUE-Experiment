# Current work / restart handoff

Updated: 2026-09-22, after the normal build/restart and animated Evade PIE tests.

## Current status

Animated Evade is implemented and its keyboard gameplay checks passed. No new
C++ or imported-animation changes were needed in this validation session.
Editor is open on Lvl_Combat; PIE is stopped and injected keys are released.
All temporary enemy changes were made only to UEDPIE objects and discarded.
LogCombatCharacter verbosity was restored to Log.

The previous Live Coding LNK2001/LNK1120 failure is resolved. The missing
DEFINE_LOG_CATEGORY(LogCombatCharacter) definition was included in the user's
normal GSGREditor Win64 Development build before restarting the editor.
Verified the build log includes linking UnrealEditor-GSGR.dll and Result:
Succeeded. DLL timestamp: 2026-09-22 23:32:08 local.
Build evidence: C:/Users/vicky/AppData/Local/UnrealBuildTool/
Log-backup-2023.11.04-05.08.55.txt (despite the old filename, its contents and
modification time are from today's build). The following Log.txt invocation
also succeeded with the target already current.

## User intent and implementation

- Hold F / gamepad Left Shoulder to chain random center/left/right evade motions.
- Preserve LMB light attack, RMB heavy attack, and Space's separate back dodge.
- Frontal enemy hits cause no damage/knockback while held. Release and punch the
  red flurry enemy during exhaustion for a one-hit kill.
- Use the user's existing retargeted animations; preserve their imported assets.
- CombatCharacterEvade.cpp replaces CombatCharacterBlock.cpp. Evade naming is
  used throughout character, game mode, input, and HUD. CoreRedirects preserve
  old reflected property/function references.
- IA_Block was renamed through Unreal to IA_Evade. Its old file deletion and
  BP_CombatCharacter / IMC_Combat changes are intentional.
- EvadeAnimations holds three hard references; EvadePlayRate=1.0 and
  EvadeBlendTime=0.12. Dynamic montages use the existing attack slot and choose
  randomly without adjacent repeats. Tick handles chaining; no delayed timer
  can restart animation after release. Starting cancels attacks/charge; stopping
  blends out only the owned evade montage. Death/controller/end-play clear it.

## Asset defaults verified after restart

BP_CombatCharacter.EvadeAction = IA_Evade. EvadeAnimations contains:

- /Game/Variant_Combat/Anims/MIxamo/Retargeted/Center_Block.Center_Block (1.6 s)
- /Game/Variant_Combat/Anims/MIxamo/Retargeted/Left_Block.Left_Block (1.5 s)
- /Game/Variant_Combat/Anims/MIxamo/Retargeted/Right_Block.Right_Block (1.7 s)

All reference SK_Mannequin, compatible with SKM_Quinn_Simple / ABP_Manny_Combat.
No defaults repair was needed. Source clips outside Retargeted use Y_Bot.
Root motion and force root lock remain false; retargeting assets are untouched.
IMC_Combat.DefaultKeyMappings confirms F and Gamepad_LeftShoulder -> IA_Evade,
LMB/RMB -> light/heavy, and Space -> IA_Jump (stationary back dodge).
UE 5.8's old Mappings field reads empty; inspect DefaultKeyMappings instead.

## Validation completed in this session

- BP_CombatCharacter, BP_CombatGameMode, BP_FlurryEnemy, and ABP_Manny_Combat
  compile with warnings_as_errors=true.
- Actual Windows keyboard/mouse input in floating PIE on Lvl_Combat.
- 101 logged evade animation starts: Center 39, Left 31, Right 31; zero adjacent
  repeats across the recorded sample, including release/repress tests.
- Continuous F maintained 5/5 HP through repeated natural frontal normal attacks
  and flurries. EVADING and EVADED! HUD states were captured.
- LMB, RMB, and Space while F was held did not break Evade; enemy stayed at 3 HP.
- At 18:11:45.633 UTC enemy exhaustion, helper released F and clicked LMB once:
  enemy HP changed 3 -> 0; player stayed 5/5 and bIsEvading became false.
- F interrupted a heavy charge and a light attack; screenshots and reflected
  state showed Evade active afterward. These isolated checks used a temporarily
  disabled/repositioned enemy only in PIE.
- Release stopped animation chaining for over 20 seconds and returned to idle.
  Repress resumed. Space after release visibly performed the separate back dodge.
- Player transform before/after stayed (1400,1750,302.15), yaw 0; sampled poses
  remained grounded. Screenshots show usable retargeted poses, but are not a
  frame-by-frame assessment of transition/foot-sliding polish.
- A rear-positioned enemy in disposable PIE damaged/killed the player while F
  remained down: HP 0, bIsEvading=false, Game Over shown. This confirms frontal-
  only protection and death cleanup. Releasing F and clicking Restart yielded
  HP 5 and bIsEvading=false. Earlier unguarded run also reached Game Over.
- No missing Evade input/property/function/animation reference errors observed.

Evidence: Saved/Logs/GSGR.log and Saved/Screenshots/Evade*.png.
Saved/EvadePlaytest.ps1 is an ignored local UI test helper, not shipping code.
It checks the GSGR Preview window identity/focus before sending input. Invocation:
powershell -NoProfile -ExecutionPolicy Bypass -File Saved/EvadePlaytest.ps1 ...
Actions include Capture, Down/Up/Tap, Click, Counter, ChargeEvade, LightEvade,
and Dodge. Counter watches a NEW exhaustion log line, releases F and clicks once.

## Remaining limits / next work

- User can now assess the feel and visual polish of the three motions in editor.
  Further tuning should follow concrete feedback; don't redo their retargeting.
- Physical gamepad testing and packaging were not performed.
- Fresh-profile FTUE was not replayed after the animated update. Permission
  paths were reviewed; previous guard-version FTUE results are in FlurryEnemy.md.
  Local profile remains FTUE-complete. FTUE.Reset exists but persists a profile
  reset; do not run it casually just to show the tutorial.
- Startup reports a missing unrelated SM_DoorFrame_Edge asset at
  /Game/LevelPrototyping/Interactable/Door/Assets/Meshes/SM_DoorFrame_Edge.
  Not repaired in this Evade task. No Evade-specific load errors were found.
- Some editor tool operations during PIE emit GetCurrentLevel errors even when
  the explicit PIE-object property/transform update succeeds. Also an attempted
  EvadeMontage property read warned because that transient field isn't exposed
  to this tool. These are tooling diagnostics, not gameplay failure evidence.

## Git / preservation

Branch main; baseline before the animated Evade commit:
0e4f90d5fddb86ecf09a019a72a55f248d7a648a
Add red flurry enemy with hold-to-block counter combat.
The user requested committing and pushing the validated animated Evade work.
This handoff is included with the implementation, input assets, and all 12
Mixamo source/retarget assets. Those assets' dependency lists reference this
Mixamo folder, existing mannequin assets, and engine/plugin assets. Check
git log and remote status for the resulting commit and publication state.
Preserve the intended IA_Block deletion. Unrelated imported folders remain
outside the Evade commit and untouched:
Content/FreeAnimationLibrary,
Content/__ExternalActors__/LocomotionAnimPack,
Content/__ExternalObjects__/LocomotionAnimPack.

Git/LFS can fail in sandbox with couldn't create signal pipe / Win32 error 5;
rerun Git inspection with escalation instead of changing Git/LFS configuration.
Do not routinely delete Binaries/Intermediate or user imports.

## Normal build command (only if further C++ changes require it)

With editor closed:
& 'D:\Games\Epic Games\UE_5.8\Engine\Build\BatchFiles\Build.bat' GSGREditor Win64 Development 'D:\Unreal Stuffs\Projects\GS\GSGR\GSGR.uproject' -WaitMutex -NoHotReloadFromIDE

UE is 5.8.1. Constructor defaults/redirects should be checked after a normal
build/reopen, not assumed updated in existing Live Coding instances.
