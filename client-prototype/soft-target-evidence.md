# Soft targeting: local static evidence, 2026-11-05

Exact input: WoW.exe SHA-256 c33f54116161b0baf3d6f3b58276688336e96c50a80e921f10a8d5a6c38a4a60.
After the user enabled IDA MCP, read-only decompilation of 0x4F5980 and 0x51F790 confirmed the world hover setter ABI and g_trackedObject / EVENT_UPDATE_MOUSEOVER_UNIT path. Raw results: soft-target-ida-evidence.json.
Method: installed MinGW objdump, read-only disassembly of the original EXE; historical OnLayerUpdate pseudocode in client-research/geometry-evidence.json.

- OnLayerUpdate passes a PickResult pointer to thiscall 0x4F8190 at 0x4FA302. That function uses more than a GUID, so the prototype does not call it with a GUID-only buffer.
- Instead reuse 0x4F5980: ECX=world, GUID low/high on stack, ret 8. It compares world+0x2C8/QWORD, creates a 16-byte pair (new, old), calls cdecl 0x51FB60 and updates world+0x2C8. 0x51FB60 forwards four DWORDs to cdecl 0x51F790.
- 0x51F790 resolves the active player, removes the old model highlight, writes new mouseover GUID to 0xBD07A0/QWORD and updates model hover/tooltip and event 0x142. Its switch tables extend through 0x51FA4C. It does not assign the locked target at 0xBD07B0 or call CGGameUI::Target.
- The new wrapper checks world hover versus global mouseover, because UI unit frames can update mouseover independently. A guarded CALL replacement at 0x4F59C0 intercepts only the world-hover notification adapter: while soft mouse look owns input it keeps the sampled hover and suppresses duplicate notifications, so stock mouse look cannot clear/re-highlight it every frame. Original pairs are forwarded unchanged outside this mode. Pausing releases only the owned global hover, even if the native world cache is already zero, and then resets the world cache; a different UI mouseover is preserved. As of 0.3.15 this CALL is installed on demand by the sole client UI thread only for active soft mouse look; OFF/free cursor/pause restore the original bytes. DLL startup leaves that CALL untouched. ToggleCall tracks successful enable/disable transitions and rejects unexpected bytes.
- LMB is consumed only in active, foreground, unpaused soft-target mouse look, before stock mouse handling. Its GUID snapshot is re-resolved and range/selectability checked on the game thread, then passed to the existing guarded CGGameUI::Target at 0x524BF0. No InteractGuid dispatch is part of LMB selection.
- The three new code ranges are recorded in feature-ranges.json and guarded at load. Native calls, actual highlight visibility, event ordering, Cell and custom client windows require in-game confirmation; static evidence is not runtime verification.

## Visual hover and game-object preview, 0.3.16

Local objdump of the exact original EXE identifies visual setter 0x743C70 (0xD7 bytes) and clearer 0x743BC0 (0xAB bytes), both thiscall(object,int) with ret4. They respectively OR / AND the bit selected by (argument+24) in object+0xBC; argument 1 is the mouseover channel already used in recorded 0x51F790. The setter updates the model highlight color and the clearer preserves other highlight bits. These two exact byte ranges are added to feature-ranges.json and runtime guards. Fresh IDA requests timed out and are not used as new evidence.

The preview accepts re-resolved kind-3 game objects in addition to eligible kind-2 units. LMB hard selection still receives only unit GUIDs. Explicit highlight refresh operates on resolved objects and owns only the native hover channel; no model or target memory is written directly. Actual outline rendering requires runtime confirmation.
