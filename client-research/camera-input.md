# Camera and input: static evidence

Scope: read-only IDA analysis of Wow_12340.exe, image base VA 0x400000, MD5 45892bdedd0ad70aed4ccd22d9fb5984. All addresses below refer to this database.

Local WoW.exe MD5 is 9A21E982D20F6B323CC9B84E09F116DB. The final comparison covers 49 complete code ranges: 48 matched byte for byte. Only Target at 0x524BF0 differed: local 0x524DC1..0x524DCD contains 13 NOPs in place of calls named OsGetAsyncTimeMs and UpdateSpellCastBars in IDA. See local-comparison.json. This does not establish full runtime or dependency equivalence and does not validate every address in this report.

No EXE, IDA database, or game process was modified.

## Result

Code and callers confirm existing paths for persistent mouselook, cursor release, relative mouse input, player-facing synchronization, and camera updates. These entry points are sufficient to design a centered third-person camera prototype with cursor switching. This is not a ready executable patch: runtime ABI, lifecycle, thread assumptions, and patch sites still need verification.

A native lateral shoulder-offset control was not confirmed. Offset +0x13C is applied along up, not screen-right.

## Functions and observed ABI

The target is x86. IDA-generated types can be wrong; corrections below are based on assembly and callers.

| VA | Observed ABI | Role and evidence |
|---|---|---|
| 0x4F5960 | No args; pointer EAX; cdecl-compatible | GetActiveCamera returns [currentWorldFrame+0x7E20] or NULL; assembly confirmed. |
| 0x5F95D0 | No args; pointer EAX | GetActive returns global input pointer at 0xC24954. |
| 0x5FCC10 | No consumed args; ret; EAX=0 | Lua_MouselookStart clears input+0x18; SetControlBit(input,1,time at 0xB499A4); conditional UpdatePlayer; then 0x5FCB20. Data xref 0xAD1A1C, table dispatch. |
| 0x5FC890 | No consumed args; ret; EAX=0 | Lua_MouselookStop clears input+0x18; UnsetControlBit(input,1,time,0); conditional UpdatePlayer; then override-binding cleanup at 0x5604F0. Data xref 0xAD1A24. |
| 0x5FA170 | thiscall, ECX=input; mask,time on stack; ret 8 | SetControlBit changes input+4. Transition from neither mask 1 nor mask 2 to either calls 0x512D60; also resets accumulators and updates camera/movement state. |
| 0x5FA450 | thiscall, ECX=input; mask,time,flag; ret 0xC | UnsetControlBit clears flags; transition away from masks 1/2 calls 0x512DC0. Pending default action at input+0x18 can execute unless cleared. |
| 0x512D60 | No args | Capture: graphics device virtual +0x124 with 0, then EventSetMouseMode(1,0). |
| 0x512DC0 | No args | Release: restores world/UI/device state, then EventSetMouseMode(0,0). |
| 0x47D690 | cdecl(mode,flags); ret | EventSetMouseMode validates mode<2, gets event context under lock, forwards to 0x47FBB0. |
| 0x5FA890 | thiscall(input,force) | UpdateMouseMode evaluates +0x58/+0x5C and CVar; changes graphics device virtual +0x124 and calls 0x5F9670. Not a replacement for mouselook start/stop. |
| 0x512CD0 | cdecl(event*) | Physical mouse event dispatcher to 0x5FBA60 for type 1074069707. |
| 0x5FBA60 | thiscall(input,event*); ret 4 | Physical relative input: float dx/dy at event+0x24/+0x28; calls UpdateFreeLookFacing and conditional SyncFreeLookFacing. |
| 0x5FAB70 | thiscall(input,float dt) | Separate per-frame OnMouseMoveRel path for flags 0x6000000, input+0x64/+0x68, dt and 800x600 normalization. Direct caller 0x4FA33C. |
| 0x6020B0 | thiscall(camera,float dx,float dy,float* optionalOut); ret 0xC | UpdateFreeLookFacing applies sensitivity/inversion, yaw/pitch and smoothing. IDA int/int/float arguments are wrong: arg8 is loaded as pointer; caller 0x5FBB66 passes local float address. |
| 0x6023D0 | thiscall(camera); ret | SyncFreeLookFacing passes camera+0x11C to SetFacing(input,asyncTime,yaw); clamps combined pitch and invokes 0x5FBE70 with vehicle checks. |
| 0x5FB260 | thiscall(input,time,float yaw); ret 8 | SetFacing first checks CameraCanTurnPlayer; normal branch calls OnSetRawFacingLocal, with separate commentator/vehicle behavior. Direct caller 0x6025C5. |
| 0x5FA6B0 | thiscall(input); boolean-like EAX | CameraCanTurnPlayer checks mover/control state, camera/object GUID match, camera flags and input mask 0x2000001. IDA pointer return is wrong. |
| 0x5FA110 | thiscall(input,unit*); bool | Decides camera/player sync through unit restrictions, input mask 0x2000001 and unit virtual +0x138. Called by both mouse paths and SetControlBit. |
| 0x5FBBC0 | thiscall(input,time,updateMovement) | UpdatePlayer preserves native movement state and timing. Many Lua movement start/stop callers. |
| 0x607B00 | cdecl(callbackArg,camera); ret | Camera mode dispatcher. First argument unused in observed body. Attached branch calls 0x606F90 with ECX=camera and object/time on stack, then 0x5FE7B0. |
| 0x606F90 | thiscall(camera,object*,time); ret 8 | Attached-camera update: focus, distance, terrain/water/vehicle and collision-related work. Writes final camera world position +8/+0xC/+0x10 at 0x6075AB..0x6075E9. |
| 0x601D60 | thiscall(camera,outVec,focusVec,float distance,extraVec); ret 0x10 | Position = focus - forward*distance + up*scaledOffset + extra. Reads offset +0x13C and scale +0x2C0. Additional collision steps follow in caller. |
| 0x6061D0 | thiscall(camera,outVec,focusVec,float distance,inputVec); ret 0x10 | Position correction through Intersect and 0x6059E0. IDA incorrectly types inputVec as float; code dereferences it as pointer. |
| 0x607CB0 | thiscall(camera,rect*); ret 4 | Builds projection through 0x6BF370, view through 0x6BFE60; device virtual +0xA0/+0xA4 receive matrices. |
| 0x5FE880 | Same thiscall as 0x607CB0 | Thunk tail-jumps to 0x607CB0. IDA stdcall is wrong: caller has ECX=camera and stack &world+0x320. |
| 0x4FA040 | thiscall(world,float dt); ret 4 | OnLayerUpdate runs native hover pick first; then GetActive -> OnMouseMoveRel(dt) and world+0xB14=dt. Virtual-dispatch data xref 0x9F9A50. |
| 0x4FA5F0 | thiscall(world) | OnWorldUpdate calls camera update at 0x4FA7BC and matrix setup at 0x4FA812. Caller 0x4FB031. |
| 0x4FAF90 | Internal/custom ABI; IDA usercall EBX plus stack world* | RenderWorld sets ECX=world at 0x4FB02F, calls OnWorldUpdate at 0x4FB031, then Render. Inferred usercall is not a ready external C prototype. |
| 0x5FE4E0 | thiscall(camera,viewIndex) | CreateViewFromCamera saves distance/pitch/yaw through 0x5FE440 into view slots and CVar. It does NOT build rendering matrices. |

## Partial fields

Offsets are bytes, not a complete C++ layout.

| Object | Offset | Observed use |
|---|---|---|
| WorldFrame | +0x7E20 | CGCamera* |
| WorldFrame | +0x320 | Rect passed to camera render setup |
| WorldFrame | +0xB14 | Frame delta float |
| InputControl | +0x4 | Mask 1: mouselook/turn-or-action; mask 2: camera/select; combined masks have movement semantics |
| InputControl | +0x8,+0xC | Accumulated absolute mouse displacement magnitudes |
| InputControl | +0x18 | Pending default action 1/2, cleared by MouselookStart/Stop |
| InputControl | +0x44,+0x48 | Special facing state and yaw |
| InputControl | +0x58,+0x5C,+0x60 | Mouse-mode/force/relative-input flags; incomplete bit map |
| InputControl | +0x64,+0x68 | Float relative rates in per-frame input |
| CGCamera | +0x8,+0xC,+0x10 | Final world position |
| CGCamera | +0x38,+0x3C | Clip values copied from globals before update |
| CGCamera | +0x44 | Rectangle height / width ratio in 0x607CB0 |
| CGCamera | +0x88,+0x90 | Primary/fallback attached GUIDs, 64-bit each |
| CGCamera | +0x98,+0x9C | Camera flags; incomplete bit map |
| CGCamera | +0x118 | Current distance-like value in focus-forward*distance |
| CGCamera | +0x11C | Yaw, passed to SetFacing; normalized by 0x5FE5F0 |
| CGCamera | +0x120 | Pitch |
| CGCamera | +0x130 | Additional pitch component |
| CGCamera | +0x13C | Offset along up, not confirmed shoulder offset |
| CGCamera | +0x1E8,+0x230,+0x260 | Saved/desired distance,pitch,yaw; state/smoothing must be respected |
| CGCamera | +0x2C0 | Up-offset scale in 0x601D60 |

Camera vtable +4 returns forward and +0xC returns up. Evidence: 0x607CB0 -> 0x6BFE60 uses those vectors to build a look-at basis with cross products. Full vtable and structure remain unreconstructed; the IDA CGCamera type is smaller than offsets actually accessed.

## Candidate aim update order

1. OnLayerUpdate 0x4FA040 performs hover pick, then mouse input.
2. RenderWorld 0x4FAF90 invokes OnWorldUpdate 0x4FA5F0.
3. Call 0x4FA7BC -> 0x607B00(0,camera) updates camera position/state.
4. Call 0x4FA812 -> 0x5FE880/0x607CB0(camera,&world+0x320) sets device matrices.
5. Late follow-up: at 0x4FAB37..0x4FAB67 the worldframe+0x340 cached matrix used by WorldToScreen is compared/refreshed; model selection lists are reset/enumerated earlier in this late part of OnWorldUpdate.
6. Correction from 0.2.2: 0x4FB036 is after model-list clearing and before Render repopulates it, so it is unsuitable for native model picking. The same CALL hook at 0x4FB031 now runs Tick BEFORE OnWorldUpdate, using the previous rendered camera/list, then prepares the shoulder camera and calls the original update. Matrix readiness and model-list readiness are distinct. Evidence: ../client-prototype/target-pick-order-evidence.json.

This is ordering evidence, not an approved JMP site. Instruction boundaries, trampoline relocation, register/flags/FPU preservation and reentrancy need verification before patching. Geometry-only aim with independent projection could instead run after 0x607B00.

## Remaining gates and limits

- local-comparison.json covers selected code ranges only, not full image/import/dependency/runtime equivalence.
- Runtime ABI, game-thread context and update ordering are untested.
- Check login/logout, world loading, focus loss, modal UI, cinematics, death/ghost and vehicles.
- A new cursor-release key is not registered yet. Existing start/stop endpoints have override-binding side effects.
- Direct input-bit writes would bypass native mouse mode, UI/device changes, pending actions and camera handling.
- Preserve CameraCanTurnPlayer and native movement restrictions.
- Lateral shoulder placement must retain camera collision handling; post-collision position writes could place it in walls.
- No functional code, executable patch, game execution or complete patch-site validation was performed.

Raw decompilation/callers/assembly: camera-input-evidence.json. Separate local byte comparison: local-comparison.json.
