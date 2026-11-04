# Screen geometry, picking, collision and native UI

Evidence: `geometry-evidence.json`, entries indexed by virtual address. All observations are from the open IDA database, not a running game. Function names below are descriptive interpretations unless already named in the database. No IDB changes were made.

## Confirmed screen-to-world picking chain

`CGWorldFrame::OnLayerUpdate 0x4FA040` gets cursor coordinates, converts them with `0x47BFF0` and invokes `0x4F9DA0` at instruction `0x4FA13A`. The caller allocates and zeroes a 48-byte result region (GUID and ten floats).

`0x4F9DA0` is a thiscall function, ECX=worldframe, stack=(float x, float y, int mode, result*), `ret 0x10`. It:

1. Requires bit 0 of input-control field +0x58. This gate must not be assumed satisfied merely because mouselook is active.
2. Saves the graphics device projection/view matrices and installs camera matrices through `0x5FE880`.
3. Obtains mode-dependent pick flags from `0x4F7650`.
4. Calls `0x4F6450` to build the world ray, then `0x4F9930` to pick against geometry and models.
5. Updates worldframe+0x2D0/+0x2D4 (last picked GUID), and restores the device matrices.

Return classifications observed in callers: 0=no hit, 1=terrain-like hit, 2=model/object hit processed through OnLayerTrackObject, 3=world/gameobject hit. Always resolve and check the returned GUID and actual object type; a nonzero classification is not proof of an attackable unit.

Result layout observed in writes: +0 GUID low, +4 GUID high, +8/+12/+16 hit position, +20 distance, +24/+28/+32 ray start, +36/+40/+44 ray end. Some gameobject hit coordinates are transformed by an inverse world matrix, so do not treat every hit-position result as an unconditional world position.

### Ray construction

`0x4F6450`: thiscall(worldframe,float x,float y,Vec3* start,Vec3* end), `ret 0x10`. It normalizes x using world+0x324/+0x32C and y using +0x320/+0x328, rejects coordinates outside [0,1], calls `0x4BF0F0`, then adds camera position from camera+8/+0xC/+0x10 to both endpoints.

`0x4BF0F0`: cdecl(float normalizedX,float normalizedY,Vec3* start,Vec3* end). Checks pointers and [0,1] coordinates, derives frustum corners from device matrices and interpolates near/far endpoints. Therefore the ray follows camera pitch as well as yaw.

The midpoint of the active worldframe viewport is the appropriate prototype center. The public caller's UI coordinate conversion and worldframe rectangle must be preserved; do not pass physical display pixels or assume that the world viewport always fills the window.

### Combined ray pick

`0x4F9930`: thiscall(worldframe,Vec3* start,Vec3* end,uint32 flags,result*), `ret 0x10`.

- Intersects scene geometry through `0x77F310` -> `0x7A3B70`, initializes fraction to 1, and shortens the ray to the first scene hit.
- For flags & 0x7C, calls `0x4F9550` for rendered-model selection along the shortened ray.
- For flags & 3, performs another scene trace and clips the result again.
- Writes hit GUID, position and distance, and returns the classification above.

`0x4F9550`: thiscall(worldframe,Vec3* start,Vec3* end,uint32 flags,float* hitDistance), returns GUID in EDX:EAX and pops 0x10. The IDA decompiler's float type for its start pointer is wrong. It reads the worldframe's model selection/cache lists, resolves objects, filters through `0x4F7530`, and intersects models through `0x81DF10`. It modifies model-selection/cache state. It is NOT a stateless standalone ray function and must only run on the established game/render thread with initialized world/model state.

`0x4F7530` filters type masks (units, players, gameobjects, corpses). `0x4F7350` adds local-player exclusion, optional faction/attack/assist filters and optional alive/dead filters. Default normal-world picking flags from `0x4F7650` include 0x5C, not an enemy-only mode. Hostility/alive validation remains necessary after picking.

`0x4F7650` is stdcall(mode), ret 4. Its flags depend on player state and pending spell targeting. It is unsuitable as an unconditional fixed enemy-only filter. A database function incorrectly named `__uncaught_exception` at `0x7FD620` actually checks whether `spellCast` (global 0xD3F4E4) is nonzero; it is not a C++ exception-state test.

## World-to-screen alternative for aim assistance

`0x4F6D20`: thiscall(worldframe,Vec3* world,Vec3* screen,int* optionalClipFlags), ret 0xC. Assembly confirms ECX=this and three stack arguments. It subtracts camera world position, transforms by matrix at worldframe+0x340, checks the near plane, divides by homogeneous w, maps into the worldframe's UI coordinates and reports whether the result is within its bounds. Output z retains pre-division depth; do not treat it as a normalized [0,1] depth.

Callers include `0x518F40` and `0x5252D0`, which project unit positions for directional selection, as well as the native pick chain. This is corroborated behavior, not a conclusion based only on a function name.

Frame-order correction (0.2.2): current-frame matrix readiness does not imply model-picking readiness. OnWorldUpdate clears both model lists at 0x4FAA5A/0x4FAA68. The rendering path later inserts records into world+0x29C via 0x4F8D10; pick 0x4F9550 reads its head at +0x2A4. Therefore the previously suggested 0x4FB036 point is unsuitable for this native picker. The prototype now picks BEFORE OnWorldUpdate, consuming last rendered camera/model state like native OnLayerUpdate. WorldToScreen's matrix at +0x340 is still refreshed at 0x4FAB37..0x4FAB67; that fact alone must not be used to schedule model picking. See ../client-prototype/target-pick-order-evidence.json. In-game success of 0.2.2 remains unverified.

This supplies the primitive for a small aim-assist radius: enumerate client-known units, filter/revalidate, get transport-aware world position, choose/derive a sensible body aim point, project it, score screen distance from the reticle, check occlusion and range, and keep a stable GUID. An actor's ground-origin point alone can produce poor aim, especially for large models; chest/bounds selection still needs implementation and in-game calibration.

## Collision trace primitive

`0x77F310` is a tail-jump wrapper to `0x7A3B70`, not an additional collision implementation.

`0x7A3B70`: cdecl, boolean result in AL, six stack arguments: (Vec3* start, Vec3* end, Vec3* optionalHit, float* fraction, uint32 flags, optional auxiliary output). Assembly uses plain ret; callers remove 0x18. Initialize fraction (native pick code uses 1.0). Hit position is start + fraction*(end-start). The optional sixth argument has not been fully typed; use null in the prototype like the native callers.

Native ray pick uses scene masks 0x01000124 / 0x01020124 and then 0x00100151. Other callers use different masks; bit meanings have not been fully recovered. The function also writes shared collision state, including last-hit GUID at 0xCD7768, so it must not be called concurrently from a worker.

The camera-to-object pick chain clips against scene geometry. This does not prove that the player can cast at the target: an over-the-shoulder camera can see past an obstacle which still blocks the character. A separate player-to-aim-point trace can provide a client-side approximation, while the server remains authoritative. Thin geometry, doors, dynamic objects, water, transport geometry and spell-specific LOS cannot be certified from this static inspection.

## UI without an external DLL

Observed endpoints:

- `0x819210` FrameScript::Execute: cdecl(scriptText,chunkName,executionContext). Calls string length, Lua loadbuffer and protected Lua call, and restores execution/taint bookkeeping. The exact third argument must follow the intended native callsite, not an invented security context.
- `0x81BB20` Lua_CreateFrame: validates Lua arguments and creates named/parented native UI frames.
- `0x4A2010` Lua_CreateTexture_class3: creates a native texture region and returns its Lua object.

Thus an EXE-embedded initialization script is a plausible way to draw the reticle using the game's own UI, without an external DLL or a custom Direct3D renderer. A small addon is the simpler alternative. These are Lua C adapters taking a Lua-state stack, not direct C++ frame constructors. UI initialization/reload handling and the reticle script have not been implemented or exercised.

## Conclusion

The open binary contains the necessary static geometry primitives for a client-side crosshair prototype: camera-derived screen ray, world/model picking, world-to-screen projection and scene tracing. Together with the confirmed camera/input and object/target primitives, a new renderer, network protocol or combat implementation is unnecessary.

Not established: runtime ABI validation, exact spell LOS parity, model body aim calibration, safe final patch/trampoline, input/UI lifecycle, or a completed embedded reticle. These limits must survive into implementation and testing.
