# Procedural Characters

The plugin owns registration, centrally scheduled state gathering, thread-safe snapshots, significance ranking, hard population caps, priority boosts, tier hysteresis, global query caps, debug display, and lightweight native skeletal-control nodes. Standard zombies do not require Control Rig.

## Setup

1. Hell Run's `ABaseCharacter` creates `UProceduralCharacterComponent` automatically. Component Tick is disabled.
2. Optionally create a `UProceduralCharacterProfile` Primary Data Asset and assign semantic limbs and per-tier settings.
3. Set intent or importance through `SetLookTarget`, `SetAimDirection`, `SetAttackTarget`, `SetGameplayThreat`, and `RequestPriorityBoost`.
4. Existing Hell Run AnimBPs can cache `GetHellRunAnimDataFromAnimInstance` and use its nested `Procedural` field. Other consumers can call `GetProceduralStateFromAnimInstance` during AnimBP Update Animation.

## Animation graph

Convert the authored local pose to component space, then insert the native nodes in this order:

```text
Authored locomotion/action pose
  -> Local To Component
  -> Procedural Movement Lean
  -> Procedural Look Chain
  -> Procedural Impulse
  -> Component To Local
  -> Output Pose
```

Wire the cached `FProceduralCharacterState` to each node's `Procedural State` pin. Configure semantic bones per archetype rather than assuming names:

- Movement Lean: pelvis/root plus an optional lower-spine bone.
- Look Chain: root-to-tip spine/neck/head sequence. P2 evaluates no more than the final two valid bones.
- Impulse: pelvis/spine/chest entries with profile-appropriate weights.

All three nodes check the feature mask and maximum tier before bone evaluation. P3/P4 therefore do not pay for procedural pose work. Missing bones simply make the node invalid for that mesh LOD.

`/Game/Blueprints/Characters/Enemies/Melee/Zombie/ABP_Zombie` is wired with this chain for the UE4 mannequin hierarchy. The editor commandlet can inspect another AnimBP without saving, or apply the same guarded wiring with `-Apply`:

```text
UnrealEditor-Cmd.exe Hell_Run.uproject -run=ProceduralAnimGraph -Asset=/Game/Path/ABP_Name
UnrealEditor-Cmd.exe Hell_Run.uproject -run=ProceduralAnimGraph -Asset=/Game/Path/ABP_Name -Apply
```

The apply mode compiles before saving and refuses to save on a Blueprint compile error. Always inspect the selected skeleton and review archetype-specific bone assignments before broad rollout.

The subsystem is created automatically for Game and PIE worlds. Runtime caps can be changed with `pc.Anim.HeroCap`, `pc.Anim.CombatCap`, `pc.Anim.NearbyCap`, and `pc.Anim.BackgroundCap`. A value of `-1` uses Project Settings > Plugins > Procedural Animation Budget.

## First-person viewmodel

`APlayerCharacter` owns a `UProceduralViewmodelComponent` that updates only for the locally controlled player's rendered arms and weapon hierarchy. It is deliberately outside the zombie significance and population budgets. The camera-local additive layer supplies movement bob, acceleration inertia, look lag, breathing, airborne response, landing and recoil impulses, ADS damping, and a 30 Hz wall-avoidance probe.

Tune the inherited `ProceduralViewmodelComponent` on `Base_PlayableCharacter`. At runtime `APlayerCharacter` assigns the existing `SK_Mannequin_Arms` mesh and `FirstPerson_AnimBP`, then parents the arms beneath the same invisible driver used by active weapon meshes. The arms retain a stable driver-relative pose so each weapon's first-person location and rotation offsets move the hands and weapon together. C++ feeds the demo AnimBP's movement, airborne, and armed variables because its original event graph is hard-cast to the sample first-person character. Firing plays the existing arms-slot shoot montage.

## Debugging

- `pc.Anim.Debug 1`
- `pc.Anim.ShowTiers 1`
- `pc.Anim.ShowSignificance 1`
- `pc.Anim.ForceTier 0..4` (`-1` restores automatic assignment)
- `pc.Anim.FreezeBudget 1`
- `pc.Anim.DumpStats`

## Profiling checklist

- Spawn 500 actors and confirm `pc.Anim.DumpStats` matches the configured caps.
- Capture Unreal Insights while actors enter and leave view; inspect the subsystem tick and confirm ranking occurs at 10 Hz by default.
- Confirm P4 actors have a zero procedural update rate.
- Change caps under load and verify the next assignment pass respects them.
- Validate local players, bosses, attack targets, and temporary boosts rank ahead of ordinary crowd actors.

## Current boundary

No hand IK, Control Rig, general character environment contacts, or Animation Sharing are included. The current arms use a generic rifle grip pose across weapon archetypes; weapon-specific arm offsets/IK remain future polish. The player viewmodel has one local-only, rate-limited wall probe; the crowd query API otherwise only reserves centrally capped slots for later contact modules.
