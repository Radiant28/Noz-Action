# Cursor return and zoom duration: focused static research

Scope: read-only IDA analysis of Wow_12340.exe, x86 VA base 0x400000, database MD5 45892bdedd0ad70aed4ccd22d9fb5984. No database, executable, process, or native.cpp changes. New function ranges in this report have not been compared with local WoW.exe by this research pass.

## Result

Native mouselook STOP restores the client's saved client-area cursor position. A native cursor getter and setter are available and maintain that saved state. Prefer these existing functions over a bare SetCursorPos call when implementing previous-position or crosshair-center return.

Native ZoomIn/ZoomOut accept an explicit duration in seconds as their fourth argument (counting camera as the first). A value of 0.4 requests nominal 400 ms travel; camera caps/collision and other native states still apply.

## Native cursor state and call chain

- 0xD4156C: low-level mouse mode; 1 means relative/captured mode, 0 means absolute mode.
- qword 0xD413F8: saved POINT in game-window CLIENT coordinates (LONG x then LONG y).
- 0xD413F4: native saved HWND, used with this POINT on restore; use the getter/setter, not direct writes. Address confirmed by IDA globals lookup for hWnd.
- qword 0xD413EC: screen-space physical recenter point for relative input; not the saved free cursor point.
- 0xB41828: event-layer mouse mode.
- 0xB4182C: requested mouse-mode flags.

START: Lua_MouselookStart 0x5FCC10 -> SetControlBit 0x5FA170 -> capture 0x512D60 -> EventSetMouseMode(1,0) 0x47D690 -> 0x47FBB0 -> 0x86A020 -> 0x869DB0.

STOP: Lua_MouselookStop 0x5FC890 -> UnsetControlBit 0x5FA450 -> release 0x512DC0 -> EventSetMouseMode(0,0) -> 0x47FBB0 -> 0x86A020 -> 0x8695B0.

0x86A020 writes mode 0 before calling 0x8695B0. That restore helper copies saved POINT, ClientToScreen(saved HWND), calls optional LogicalToPhysicalPoint via 0x868CE0, then uses the cursor-position wrapper 0x868C70.

0x868C70 resolves a user32 cursor-position function dynamically with SetCursorPos fallback. 0x868C10 similarly resolves GetPhysicalCursorPos with GetCursorPos fallback. Decompiler function-pointer return types are artifacts: for our callers only coordinate input and side effects matter.

Physical WM_MOUSEMOVE handling at 0x86A210:
- In relative mode it reads physical cursor position, subtracts 0xD413EC, emits relative InputEvent(12,...), then recenters via 0x869DB0.
- In absolute mode it reads GetCursorPos, ScreenToClient, emits InputEvent(10,...), then 0x869600 updates saved POINT/HWND.
- 0x869600 explicitly refuses to overwrite the saved free cursor while mode==1.

Thus a bare SetCursorPos after complete release should cause normal absolute mouse processing, but the saved native POINT remains stale until the corresponding absolute update/getter. The native setter updates that state synchronously and uses the same coordinate conversion path as native restore.

## Getter, setter and HWND accessor

### Getter 0x86A0D0

Observed ABI: cdecl(LONG* outX, LONG* outY), ret, caller stack cleanup. Treat its return as unused.

It gets the real game HWND through 0x86C6A0(0), calls GetCursorPos and ScreenToClient, and writes outputs when pointers are non-null. When mode!=1 it also updates saved HWND and saved POINT at 0xD413F8.

Call BEFORE MouselookStart while absolute mode is active to save the previous free cursor point in our own POINT. This also refreshes native restore state immediately before capture.

### Setter 0x86A130

Observed ABI: cdecl(POINT pointByValue), exactly 8 input stack bytes. Equivalent x86 parameter layout is cdecl(LONG x,LONG y), not POINT*. Returns with plain ret; caller cleans stack. Treat return as unused.

Assembly:
- [ebp+8] is x and [ebp+0xC] is y.
- If 0xD4156C !=1, it writes saved HWND and qword 0xD413F8.
- ClientToScreen(native game HWND,&point).
- 0x868CE0(saved HWND,&point): LogicalToPhysicalPoint when available, otherwise no-op.
- 0x868C70(screenX,screenY): OS cursor warp.

Caller evidence: 0x47FD8B in native cursor clipping and 0x8711FC in native input handling.

Call AFTER STOP and only after confirming native mode==0. Do not warp while relative mode remains active: the next event would be interpreted as camera displacement relative to recenter point.

### HWND accessor 0x86C6A0

Observed ABI: HWND cdecl(int selector).
- selector 0 returns the game HWND from 0xD41620.
- selector 1 returns GetActiveWindow.
- selector 2 returns GetForegroundWindow.

Use selector 0 for GetClientRect and previous-point conversion. Keep the existing foreground/process check before a user-requested Alt warp. Never use the entire desktop or top-level outer window rectangle as the crosshair viewport.

0x86A1A0 is also a client-rectangle getter, but caches the result. Ordinary GetClientRect(native HWND) is simpler for current dimensions.

## Minimal proposed sequence

1. When entering plugin-owned capture, while foreground and native mode==0, call getter 0x86A0D0 into our saved POINT, then native MouselookStart.
2. On the transition to Alt-held, invoke native MouselookStop only for plugin-owned capture.
3. Check low-level mode is now 0. Clearing mask 1 alone does not guarantee this: native mask 2 (camera/select, commonly another mouse button) can keep capture active. Defer the one-time return while captured; do not force native state bits.
4. Previous option: choose the saved client POINT, clamped to the current client rectangle if resize made it out of bounds.
5. Crosshair option: get actual normalized crosshair center from the addon's WorldFrame/UIParent geometry, map to current client pixels. If the addon uses bottom-left origin, convert y using (1-normalizedY)*clientHeight. Do not apply an additional UI scale after normalizing numerator and denominator in the same UI coordinate system.
6. Clamp chosen pixel to [0,width-1] and [0,height-1]; call native setter 0x86A130 once while mode==0 and foreground.
7. While Alt remains held, let the cursor move freely; do not repeat the warp each frame.
8. When Alt is released and capture conditions permit, refresh previous POINT through the getter before starting a new capture.

For a saved POINT after a window move, preserving client coordinates retains the same UI-relative location. If product intent is absolute desktop position instead, store/convert screen coordinates deliberately; do not mix the two.

No internal InputEvent injection is required by the observed native setter path. OS mouse-event processing updates hover/UI as it does for native cursor clipping. Immediate hover before queued mouse events is a runtime verification item.

## Zoom duration evidence

0x5FF950 ZoomIn and 0x5FFA60 ZoomOut:
- thiscall, ECX=camera, float amount, DWORD timeMs, float durationSeconds on stack; ret 0xC.
- durationSeconds>0: native durationMs = integer(durationSeconds*1000).
- speed multiplier = trunc(amount*1000)/(1000*durationSeconds*cameraDistanceMoveSpeed).
- durationSeconds<=0: native duration derives from amount/cameraDistanceMoveSpeed.
- 0x5FE580 records channel start/end times and speed multiplier.
- 0x6000E0 advances zoom using elapsed milliseconds, clamps desired distance, and updates current distance when permitted by flags.

Changing duration to 0.4 therefore uses the existing native timed zoom path. Send requests on actual mount/dismount or target-distance changes, not every frame, because repeated calls may extend/stack channels. Existing Lua CameraZoomIn passes 0.0, so explicit duration is an additional native capability rather than a new custom interpolation loop.

Native camera already accounts for attached model/vehicle/terrain heights. No new model-scale getter is necessary for the parent's chosen IsMounted() + configured extra distance/lift feature. Extra lift still needs smooth interpolation in the existing collision-preserving shoulder path.

## Required runtime checks

- Compare new code ranges/getter/setter and accessed globals with local client/signatures before calling them.
- Verify Alt return with another mouse button held; do not warp until true absolute mode.
- Test rapid Alt toggles, window resize/move, borderless/fullscreen, non-default UI scale, non-100% desktop DPI, and multi-monitor placement.
- Keep client-space versus physical screen-space handling inside native setter to preserve the existing DPI conversion path.
- Foreground loss must not trigger a new crosshair-center warp.
- No runtime test or implementation was performed in this pass.

Raw structured decompilation, assembly and callers are saved in cursor-evidence.json.
