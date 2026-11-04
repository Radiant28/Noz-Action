# One-key interaction under the reticle — verified native path

## Result

A single native entry already performs the normal right-click/default object interaction:

```cpp
using InteractGuid = int (__cdecl*)(std::uint64_t);
constexpr std::uintptr_t kInteractGuid = 0x005277B0;
```

Its ABI is two stack DWORDs (GUID low, GUID high), cdecl caller cleanup, integer return EAX. It resolves a general object, calls native Target(GUID), then invokes virtual OnRightClick on that resolved object. This covers friendly NPC gossip/vendors, mailbox gameobjects and dead creature loot through their existing native handlers. No custom packets or direct invocation of vendor/mail/loot routines are needed.

This is static evidence for the existing opened IDA database, not runtime observation. The current prototype remains unmodified.

## Entry and caller proof

E01: `sub_5277B0 0x5277B0`, size 0x73:
- 0x5277B3 obtains active-player GUID and 0x5277C6 resolves mask 0x10.
- 0x5277D9/0x5277DF read GUID low/high from stack arguments.
- 0x5277EC pushes mask 1 (TYPEMASK_OBJECT); 0x5277F0 resolves the supplied GUID.
- 0x527803..0x527805 push high/low and call CGGameUI::Target.
- 0x52780A..0x527817 load [object vtable+0xB0], put object in ECX and call with no stack arguments.
- function returns via plain RET; returns zero for missing object, one after dispatch. It also returns one when no local player exists, so return one is NOT confirmation of successful interaction.

E02: `Lua_InteractUnit 0x527F00` calls this exact function at 0x527F9C..0x527FA3: PUSH EDX, PUSH EAX, CALL, ADD ESP,8. It does so after checking world/reload global 0xD4139C, and supports mouseover by reading g_trackedObject. The prototype should supply its own exact pick GUID; it need not modify mouseover global or call Lua.

E03: Native `CGWorldFrame::PerformDefaultAction 0x4F7880` calls this function at 0x4F78EA; the normal object-click branch at 0x4F79A7 calls adapter 0x527870, which uses 0x5277B0 for a non-left click. Therefore the entry is the shared native default interaction, not an unrelated Lua-only helper.

## Friendly NPC path and built-in distance

E04: Unit vtable pointer [0xA34E40] = 0x731260, corresponding to byte offset +0xB0 from unit vtable base 0xA34D90. Player vtable cross-reference to the same method is 0xA32778.

`0x5277B0 -> virtual +0xB0 -> CGUnit_C::OnRightClick 0x731260`.

Assembly confirms the unit handler's object is ECX; IDB's fastcall rendering is an imprecise naming convention for this no-stack-argument member call. It resolves the local player, checks living/possessed state and other native control conditions. At 0x7314CD it pushes clicked object, ECX=player, and calls `CGPlayer_C::CanInteract 0x729530`; if true, it calls `CGPlayer_C::HandleOnRightClick 0x6DDBB0` with ECX=player, stack target and native auto-interaction flag.

CanInteract ABI: bool(AL) thiscall(player ECX, candidate stack), RET 4. It checks NPC interaction flags, reaction in both directions, and additional restrictions. It is NOT a distance predicate. It is suitable to prefilter living NPCs so a common interaction key does not fall through to native hostile autoattack.

HandleOnRightClick obtains both WORLD positions through vtable+0x2C and compares squared distance to (target unit descriptor float+0xF0 + 4)^2. Outside that distance it displays native GERR_TOO_FAR_TO_INTERACT, or may set native tracking/click-to-move when CanAutoInteract is enabled. It dispatches:
- gossip: 0x6DDD67 -> CGGameUI::OpenGossipWindow;
- quest dialog: 0x6DDD9E;
- vendor: 0x6DDE13 -> CGPlayer_C::ShopFromMerchant;
- other standard NPC services and mailbox NPC flags through their native branches.

No map/player-to-NPC collision ray was observed in this interaction/range path. Do not claim built-in player line of sight is enforced by the wrapper. Server acceptance remains authoritative.

## Loot corpse path

E05: In unit OnRightClick at 0x7312F2, dead target plus allowed native flags/mover state enters corpse path. At 0x731323..0x73132B it calls:

```cpp
using IsLootable = bool (__attribute__((thiscall))*)(void* corpse, std::uint32_t clientTime);
constexpr auto kIsLootable = 0x00717B60;
```

Use the native time DWORD at 0xCD76AC, exactly as the stock caller does, NOT GetTickCount(). Return convention is integer EAX 0/1 in assembly; a bool return reads the correct low byte. Predicate requires:
- signed health ([unit+0xD0]+0x48) <= 0;
- unit+0xAC0 timestamp either zero or not ahead of passed native client time;
- byte at descriptor+0x124 has bit 1 (mask 0x01), the native loot flag.

When true, the handler performs standing checks and invokes `0x6DAD90` with ECX=player, clicked corpse and auto-interact flag on stack. It handles native mount/form restrictions and standard loot request. Its prerequisite helper `0x6D5A60` checks distance when native auto-interaction is disabled. Range helper `0x4F5F40` returns max(5, player bounding radius + target bounding radius + 1.3333334), using descriptor pointers as its arguments (not unit object pointers). It also checks movement/control/ongoing loot state. If native auto-interact is enabled it may start native movement instead of immediately looting.

For the requested feature, keep dead-unit eligibility separate from the existing enemy acquisition filter, which currently rejects all dead units. Do not use CanAttack as a requirement for loot.

## Mailbox gameobject path

E06: TYPEMASK_GAMEOBJECT=0x20 is proved by caller 0x70BE4F (PUSH 20h) into ClntObjMgrObjectPtr.

GO vtable base 0xA34640 +0xB0 = 0xA346F0 -> `CGGameObject_C::OnRightClick 0x711140`.
It calls behavior component [GO+0x1A0] vtable+0x18 before proceeding, closes existing loot, then delegates to `0x70F680`.
That invokes component vtable+0x1C validation; on success invokes component vtable+0x24 behavior and sends native gameobject report-use notification. On validation failure it displays normal native errors or may initiate CTM for too-far cases.

Mailbox subtype is descriptor byte+0x2D = 19. Constructor 0x714250 case 0x13 calls 0x712750, which calls base constructor 0x7124B0 then sets component vtable 0xA34140. The vtable entries are:
- +0x18 (0xA34158) -> 0x7112A0: availability/flags checks;
- +0x1C (0xA3415C) -> 0x711470: player/control/form restrictions and range validation;
- +0x20 (0xA34160) -> 0x70FA70: range test;
- +0x24 (0xA34164) -> 0x70C9D0: calls CGPlayer_C::TalkToMailbox 0x56DA60 with owner object's GUID.

Mailbox base interaction radius is 5; `0x70FA70` measures against object display/model bounds expanded by interaction distance, accounting for object size/scale/transformation. Do not replace this with a fixed center-origin distance check. No terrain/WMOs line trace is demonstrated here: these are availability and proximity checks. Exact screen picking rejects camera-to-object occlusion through its stock picking path, but that is a different line from player-to-object LoS.

## Smallest implementation contract

1. Keep existing game-thread/in-world/living-player/focus/spell-targeting/reentrancy guards. Also require *0xD4139C == 0 before calling this lower-level wrapper, matching Lua_InteractUnit.
2. On ONE binding press, queue one request, consume once on existing game-thread Tick BEFORE OnWorldUpdate clears the pick models. Do not invoke every frame while the key is held.
3. Perform a fresh exact-center native pick, accept only kind==2 and nonzero GUID. Keep this raw hit separate from PickEnemy/RejectTarget: hostile-only acquisition currently strips friendly units, dead units and GO GUIDs. Kind 3 is a world/terrain object hit, not a generally usable object-manager GUID; do not treat it as mailbox.
4. Resolve that GUID afresh with mask1. Allow NPC/creature units only when CanInteract(player,target) for living units, or native IsLootable(corpse,*(DWORD*)0xCD76AC) for dead units. For strict NPC/mail/loot scope, reject TYPEMASK_PLAYER 0x10 and self, and allow GO mask0x20 only when subtype19. Broader doors/chests/default actions can use the same wrapper if later desired; they are not required now.
5. Call int __cdecl(GUID) at 0x5277B0 once. It already sets target and dispatches correct native action. Do not call separate Target, vendor, mailbox or loot packet functions.
6. Because native wrapper changes current target, skip auto-target selection for that Tick and account for g_selection.owned/actual target on following ticks. Preserve a newly opened NPC/loot UI from immediate re-selection if the chosen product behavior requires it. The wrapper's integer result means dispatched/resolved, not UI-open or server success.
7. Preserve native CanAutoInteract/CTM settings; stock right-click can walk toward far interactables when this is enabled. If the requirement is explicitly 'never move', gate actions by verified range before native dispatch or make a separate deliberate product decision. Do not silently change the global CVar.

Fresh picking supplies camera occlusion behavior already used by the working prototype. Strict player-to-object LoS is unresolved here; use existing verified collision trace only after selecting correct start/end points if that extra guard is needed. Do not advertise that the common native wrapper performs LOS.

## New code-guard ranges

Direct entry/filter guards needed by the smallest implementation:
- 0x5277B0, 0x73 bytes (115): common interaction.
- 0x729530, 0xE2 bytes (226): living NPC CanInteract.
- 0x717B60, 0x38 bytes (56): corpse loot predicate.

Existing guards already cover resolve, player GUID, dead predicate, native Target and screen pick. For validation of newly relied-upon chained semantics, the evidence JSON also records full function sizes for:
- 0x731260, 0x397 (919 bytes): CGUnit_C::OnRightClick.
- 0x6ddbb0, 0x580 (1408 bytes): CGPlayer_C::HandleOnRightClick.
- 0x6dad90, 0x1ce (462 bytes): sub_6DAD90.
- 0x6d5a60, 0x176 (374 bytes): sub_6D5A60.
- 0x4f5f40, 0x37 (55 bytes): sub_4F5F40.
- 0x721f50, 0x39 (57 bytes): CGUnit_C::CanAutoInteract.
- 0x711140, 0x59 (89 bytes): CGGameObject_C::OnRightClick.
- 0x70f680, 0x12f (303 bytes): CGGameObject_C::sub_70F680.
- 0x7112a0, 0x1c3 (451 bytes): sub_7112A0.
- 0x711470, 0x249 (585 bytes): sub_711470.
- 0x70fa70, 0x254 (596 bytes): sub_70FA70.
- 0x70c9d0, 0x18 (24 bytes): sub_70C9D0.
- 0x56da60, 0x36 (54 bytes): CGPlayer_C::TalkToMailbox.

Hashes must be generated from the local verified executable using the project's existing guard-generation method. These are sizes, not claimed local EXE byte hashes. Newly relying on virtual dispatch also means validating class/type eligibility, not casting every raw pointer to a unit.

## Scope and unverified items

Only this document and interaction-evidence.json were written. No native.cpp changes, IDB edits, game launch or live calls. Static analysis supports a usable common entry and all three requested routing paths. Runtime behavior for this server, actual UI opening, out-of-range movement, player LoS and edge cases while UI is open remain untested. Original historical opened-database MD5 differed from the first local EXE; use current project identity/guard workflow before implementation.
