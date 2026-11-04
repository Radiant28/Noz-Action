# Objects and target selection — static IDA evidence

Target: opened IDA database `Wow_12340.exe`, image base `0x00400000`, MD5 supplied by coordinator `45892bdedd0ad70aed4ccd22d9fb5984`. Addresses below are virtual addresses in THIS database. Workspace `WoW.exe` has a different MD5 `9A21E982D20F6B323CC9B84E09F116DB`; these addresses must not be patched into that file without mapping and verification.

Method: read-only IDA MCP decompilation, assembly and cross-references. No IDB mutation, EXE patch, process attachment or game execution.

## Confirmed building blocks

| Address | Purpose | ABI / evidence |
|---|---|---|
| 0x004D4B30 | EnumVisibleObjects | cdecl int(callback, context). Callback is cdecl int(uint64 GUID, context), three DWORD stack slots. Calls callback at 0x4D4B76, caller removes 0xC. Callback zero stops enumeration; nonzero continues. Return 1 = completed, 0 = callback stopped. |
| 0x004D4DB0 | ClntObjMgrObjectPtr | cdecl pointer(uint64 GUID, uint32 mask, const char* file, uint32 line), as observed in callers pushing five DWORDs and removing 0x14. IDA pseudocode omits trailing diagnostics arguments; implementation does not read them in this build. Returns null for missing manager, zero GUID, missing object or mask mismatch. |
| 0x004D3790 | ClntObjMgrGetActivePlayer | cdecl uint64(), return EDX:EAX in x86 asm. Returns GUID from object manager +0xC0, zero if no manager. |
| 0x006E6EF0 | World position getter, misleading IDB name CGUnit_C_SetPosition | thiscall Vec3*(unit ECX, Vec3* output stack), ret 4. Unit vtable byte slot +0x2C. Calls CMovement::GetPosition at 0x4F4460. This function READS position; it does not set it. |
| 0x004F4460 | CMovement::GetPosition | Applies transport matrix when movement +0x8 contains nonzero transport GUID; otherwise copies input XYZ. Thus world-position wrapper above is preferable to raw coordinate reads. |
| 0x006E6F10 | Raw movement-position getter, IDB CGUnit_C::GetPosition | thiscall Vec3*(unit ECX, output stack), ret 4. Unit vtable byte slot +0x30. Copies XYZ from [unit+0xD8]+0x10/+0x14/+0x18 without transport transform. |
| 0x006E6F60 | Raw facing getter | thiscall; copies movement facing (decompilation retained in raw evidence). |
| 0x00729740 | CGUnit_C::CanAttack | thiscall bool(AL)(source ECX, target stack), ret 4. Confirmed unambiguously by Lua_UnitCanAttack 0x60D730 at 0x60D783..0x60D78B. Evaluates unit flags, immunity categories, faction/PvP/duel/controlling-player conditions. Does not replace dead and visibility filtering. |
| 0x00518E40 | Existing target-type filter | custom register ABI: EAX=mode, EDX=candidate, ECX=source. Returns int EAX. Mode 1 attacks, mode 2 attackable players, mode 3 assistable units alive, mode 4 assistable players alive, modes 5/6 party membership variants. Verified callers 0x52447D..0x524486 and 0x52520A..0x525211. |
| 0x00716490 | Unit dead predicate | thiscall bool(AL)(unit ECX). Returns signed health <= 0 from [unit+0xD0]+0x48. Unit vtable byte slot +0x128, pointer at 0xA34EB8. |
| 0x0071A380 | Unit stand-state getter | thiscall int(unit ECX), byte at [unit+0xD0]+0x110. Unit vtable byte slot +0x138, pointer at 0xA34EC8. Existing attack selector excludes value 7; naming of that value was not independently established here. |
| 0x00524BF0 | CGGameUI::Target | cdecl void(uint64 GUID), callsite 0x5252A0..0x5252A7 confirms stack low/high and caller removes 8. GUID zero performs clear-target path. Nonzero updates previous/current target, highlights, spell usability, EVENT_PLAYER_TARGET_CHANGED and sends the standard target-selection message. |
| 0x005241B0 | Internal cleanup of old target | cdecl void(uint64 oldOrZero, int notify). Resets current target QWORD to zero. When notify=1 sends zero target through standard client path and fires event. Prefer public native Target(0), not direct global writes. |
| 0x00518DC0 | Native notification of new target | Creates datastore containing value 317 followed by QWORD GUID and sends it. No custom packet logic is needed in the modification. |
| 0x00525FC0 | Lua_ClearTarget | Confirms same clear path and Lua true/nil result based on previous target presence. Do not call this Lua adapter as if it were a native no-argument clear function. |
| 0x00524FC0 | Existing Tab-style nearest selector | cdecl void(reverse, mode). Name TargetNearestEnemy in IDB, but actually serves enemy/friend/party modes. Wrapper Lua_TargetNearestEnemy 0x525AD0 calls it with mode=1. |

## Relevant fields and globals

- TLS `TlsIndex` -> per-thread slot -> +0x8 = object manager. Enumeration dereferences manager without null guard. Run on an established client/game thread in-world, not an arbitrary worker or loading screen.
- Object manager +0xAC = first visible-list node; next link displacement is read from manager+0xA4; node GUID at +0x30/+0x34. Prefer EnumVisibleObjects over hardcoding this linked-list walk.
- Resolved object +0x8 = descriptor pointer. GUID = descriptor+0x0/+0x4; descriptor type bitmask = +0x8. TYPEMASK_OBJECT=1, TYPEMASK_UNIT=8, TYPEMASK_PLAYER=0x10, confirmed by assembly immediate arguments and flag tests.
- Unit +0xD0 = unit descriptor pointer used by predicates. Health at descriptor+0x48; flags consulted by CanAttack at +0xD4; dead-selector flag at +0x124 bit 0x20.
- Unit +0xD8 = movement pointer. Raw XYZ at movement+0x10/+0x14/+0x18. World getter handles transport conversion and should be used for crosshair calculations.
- `CGGameUI::m_lockedTarget` at 0x00BD07B0 = current target GUID QWORD. GUID zero means no target. Treat as GUID, not object pointer.
- 0x00BD07B8 QWORD = previous target as updated by Target and ClearTarget.
- `g_lastEnemyTarget` at 0x00BD07C0 QWORD and 0x00BD07C8 QWORD assist-target history. Initial file value at lastEnemyTarget is all ones; do not treat that history sentinel as current-target absence.
- Tab candidate list: count 0x00BD0C00, backing pointer 0x00BD0C04, element size 16 = GUID (8), squared distance (4), front-cone preference (4).
- Tab state: selected index 0xBD08C8, rebuild timestamp 0xBD08CC, last use timestamp 0xBD08D0, filter mode 0xBD08D4.

## Existing selector chain and what can be reused

`Lua_TargetNearestEnemy 0x525AD0 -> TargetNearestEnemy 0x524FC0 -> EnumVisibleObjects 0x4D4B30(callback 0x524440, source) -> filter 0x518E40 -> sort comparator 0x5131D0 -> revalidate candidate -> CGGameUI::Target 0x524BF0`.

The native source is possessed unit when applicable, otherwise active player. Callback resolves GUID as TYPEMASK_UNIT, rejects source itself, and applies mode filter. In attack mode the filter requires CanAttack(source,candidate), dead virtual predicate false, descriptor+0x124 bit0x20 clear, stand-state virtual return !=7.

Callback gets source and candidate WORLD position through vtable+0x2C, computes squared 3D distance, computes horizontal angle relative to source facing through vtable+0x34, and tags targets within approximately +/-0.52359879 radians (30 degrees). Allowed squared range is 1681 in that cone (41 units), 100 outside (10 units). It rejects some creature-type flags and player commentator state. It also requires function 0x77F0B0(candidate+0xB8), which checks nonnull pointer and byte +0x25 equals zero; exact semantic name of this model/display state is not proven.

Comparator 0x5131D0 prioritizes cone-tag=1 then smaller squared distance. Selector cycles through this list and caches candidates with 1000/3000 ms rebuild/expiry conditions. Therefore calling this whole function every frame would cycle targets and use character horizontal facing, not exact camera crosshair direction. Reuse enumeration, positions, CanAttack/dead/filter and final Target. Add separate scoring around camera reticle and separate hold/hysteresis state.

## Sufficiency and limits

For candidate enumeration, identity, world coordinates, alive/attackable filtering and standard target assignment, this static evidence provides a complete useful primitive set in the opened binary. There is no need to reimplement factions, craft packets or write current target memory directly.

This alone does not establish crosshair projection, map/world line-of-sight, eye/chest aim points or object lifetime-safe frame hook. Those belong to camera/collision/update research. `EnumVisibleObjects` means client-known objects, not a guarantee of unobstructed visibility through terrain. The display-state helper in the native Tab callback is not a demonstrated LOS test. Target pointer must be resolved/revalidated near use, and every Target call must happen on the correct client thread. Lost target GUID, stale pointer, map changes and controlled-unit transitions still require runtime validation.

Not checked: runtime behavior, local EXE equivalence, server/anticheat response, all CanAttack branch edge cases. No executable code or patches were produced.
