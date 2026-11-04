# Verification: 0.3.9, 2026-11-04

## 0.3.9 interaction without forced cursor release

- Cause: InteractUnderCrosshair unconditionally called ReleaseMouse after every request, and Tick repeated that release for 200 ms, even for no hit, expired requests and refused actions. That freed the cursor independently of whether any window opened.
- Consume the request and reset automatic-selection ownership directly while retaining capture. Keep the 200 ms selection cooldown without releasing the mouse. Actual UI, assigned free-cursor key, focus loss, spell targeting and mode-off continue to use the existing release gates.
- Added interaction_capture, which compiles the real native source and supplies isolated client-global memory in the test process's zero-filled image. ASLR is disabled for this fixture only. It executes no-hit, expired and invalid-UI interaction paths and asserts the one-shot request is consumed, selection ownership reset, and mouse/activity flags retained. No native input/interaction function is invoked by this fixture.
- Built 0.3.9 with warnings as errors; all nine CTest checks passed, including interaction_capture. Client identity and all 73 runtime guards are unchanged and verified.
- Native GUID interaction remains unchanged for NPCs, gameobjects and loot; no simulated right click or UI cursor movement was added. In-game checks still required: visible cursor in windowed mode, real NPC/mail/loot actions, instant auto-loot and releasing the cursor only for an actually opened panel.

## 0.3.8 mount camera return

- Cause: SetZoom schedules a native timed channel; it does not immediately update camera+0x1E8. The old bookkeeping read the field immediately after requesting mount zoom, recorded an applied delta of zero and therefore requested no zoom-in on dismount. This is established by local cursor-evidence.json (ZoomIn, ZoomOut and 0x6000E0) and the previous PrepareCamera calculation.
- Added a shared ProfileZoom state used by PrepareCamera: retain the baseline while a transition runs, measure the applied delta after the 400 ms channel plus 100 ms settling margin, then subtract that actual delta on return. Steady profiles issue no repeated commands. This retains wheel zoom, handles a smaller mounted profile and accounts for native zoom caps. Existing smooth shoulder/height return remains unchanged.
- Regression checks simulate delayed field updates, foot-to-mount-to-foot, repeated cycles with manual foot zoom, reversal during a transition, native distance clamping, a negative mounted offset and invalid camera values. Host tests cannot prove the real native channel timings or visual result; mount/dismount still needs in-game confirmation.
- Built 0.3.8 with warnings as errors; all eight CTest checks passed. The original client identity and 73 runtime guards were verified.

## 0.3.7 acquisition range

- Raised acquisition range from 5..60 to 5..300 yards in the addon slider, saved-setting validation, native boundary validation and localized errors. The default remains 40; existing saved choices are preserved.
- Core tests cover 200 and 300 yards, the lower boundary, rejection above 300 and non-finite values. Addon tests cover slider bounds, configuration propagation, loading a saved 300 and rejecting 301.
- Built 0.3.7 with warnings as errors; all eight CTest checks passed. Original client SHA-256 and 73 runtime guards verified by the build.
- This changes the acquisition filter only. Objects must still be present in the client and hit by native picking; spell and interaction ranges are unchanged. Actual picking at 200..300 yards has not been tested inside WoW.

## 0.3.6 dialog close, camera profiles and reticle collection

- A.Interact now checks for an open interaction panel before the general UI pause gate. It uses the stock close API where available and HideUIPanel otherwise, then returns without sending an interaction request. Covered panels include gossip/quest/vendor/trainer, mail/loot (including ElvUI loot), bank/auction/guild bank, item text and taxi. Mode-off, typing, options and the key-binding UI block this close path. Inventory bags are left alone.
- Added independent stock-framing switches on foot and mounted. The input/cursor/targeting session stays enabled; PrepareCamera restores a previously owned zoom once and bypasses the camera offset/preset zoom while stock framing is selected. Added an independent mount profile with distance 3..40, height -1..8 and signed shoulder -5..5. Profile priority is stock-mounted, then independent-mounted, then inherited foot settings with optional adaptive extras. Side/height changes are smoothed; zoom transitions use the native 0.4 s request. Existing collision tracing remains.
- Profile zoom tracking accepts signed deltas so a custom mount profile can be nearer as well as farther than the walking camera. It tracks actual native applied changes when bounds clip a request and avoids resetting wheel zoom on every frame. Native camera/caps/return behavior still needs manual in-game confirmation.
- Protocol 6 validates 29 arguments (command plus 28 values), adding two stock flags, custom-mount flag and three numeric mount-profile values to protocol 5. No new client function addresses were introduced; 73 guards remain. Addon and DLL must be updated together.
- Replaced four line textures with one white-alpha 512x512 TGA atlas: 8 shapes by 8 thicknesses. The stdlib generator self-checks all 64 distinct cells, growing coverage, correct centers/gaps, antialiasing, transparent borders and TGA bytes. Preview was visually inspected. Shapes: broken crosshair, plus, dot, circle, circle+dot, square, diamond and corners. Existing target/class tint and opacity apply to every shape. Reticle size now allows 8..64.
- All eight CTest checks passed with profile-priority/bounds cases, all 64 atlas coordinate choices, cycling/reset, invalid-setting migration, profile controls and a repeated-dialog-close regression with typing/mode-off protection. The native close-game-UI and texture loader are not exercised by host mocks.
- Installer/release include Reticles.tga in the prepared Interface/AddOns directory. The original EXE remains unchanged; backup of 0.3.5: backups/0.3.5-before-camera-reticles-20261104-184430. Actual game tests still needed: repeated NPC/mail/loot closure, profile/wheel/collision transitions, stock framing, atlas loading and visual thickness at different UI scales.

## 0.3.5 cursor bindings/toggle, target retention and reticle colors

- Registered NOZDORACTION_CURSOR / Free cursor in Bindings.xml with runOnUp. Its body only synchronizes configuration on down/up; repeated downs do not toggle the chosen action-camera mode. Standard WoW bindings and the inline editor now share this action and preserve the secondary key. BUTTON3/4/5 and modifier combinations are mapped to native physical polling. Unsupported inputs, including mouse wheel impulses, are reported instead of silently substituting ALT.
- Legacy standalone modifier cursor settings remain available in the inline editor. Assigning regular WoW bindings supersedes them; explicit unassignment disables the fallback and persists in SavedVariables. Selecting a standalone modifier explicitly clears regular cursor bindings. Resetting addon parameters preserves WoW binding assignments.
- Protocol 5 validates 23 arguments (command plus 22 values): the previous camera/mount fields, two key/modifier pairs, retention and cursor-toggle booleans, and a separate cursor-input permission. Key zero disables that binding and requires zero modifiers; nonzero virtual keys and integer modifier masks 0..7 are validated. Native polling requires the exact SHIFT/CTRL/ALT mask for regular bindings. Status still returns nine values; its last boolean now reports free-cursor state for either hold or toggle mode. DLL/addon must be updated together.
- Keep target until another is selected defaults false. Selection.Step keeps owned targets on a miss when enabled, while normal delay/next-target switching remains. Turning it off restores the existing 250 ms lost-target clearing; manual target ownership protections remain.
- Toggle cursor on press defaults false. Native CursorMode uses rising edges, keeps free cursor after release, ignores repeats and ignores presses while input is blocked by UI/chat/focus/heartbeat. UI pauses preserve the latch and update the observed key state to avoid a false toggle on return. Turning the action-camera mode off, changing the cursor-mode checkbox, disabling all cursor bindings or losing the active camera session resets it. Only the checkbox is saved across sessions, not the temporary latched state.
- Optional reticle colors default on. Actual current target determines color, including retained/manual targets: player class color from CUSTOM_CLASS_COLORS with RAID_CLASS_COLORS fallback; hostile/neutral/friendly NPC red/yellow/green; dead targets gray; no target white. Disabling the option makes it white. Addon fixtures check faction colors, death/no target, reaction fallbacks, class table colors, custom class overrides and missing-class fallback on all four reticle bars.
- Core checks cover retained targets, delayed replacement, reverting to normal clearing, manual targets, binding validation, mouse modifiers/release and the cursor latch across repeats, release, pauses and reset. Addon fixtures in all three locales exercise external UPDATE_BINDINGS changes, cancellation of temporary menu edits, primary/secondary mouse bindings, combined modifiers, malformed input, explicit unassignment, legacy fallback, reset, all new checkboxes and repeated execution of the real cursor XML body without mode changes or interactions.
- New native entry points were not introduced; all 73 guards remain. Actual standard WoW binding UI, mouse hardware, modifier transitions and retained-target behavior still require in-game confirmation. Backup: backups/0.3.4-before-bindings-20261104-180657.

## 0.3.4 Russian localization and copy-ready distribution

- Added Localization.lua before the main addon in the TOC and to the development installer's backup/copy list. ruRU translates options, controls, binding labels, help, status, chat messages, known native API errors and the reason in target diagnostics. Other locales fall back to the existing English keys. Technical DLL logs and numeric diagnostic field names remain unchanged.
- All eight CTest checks passed: original core/client/loader checks plus the complete addon fixture in enUS, ruRU and fallback deDE. Russian cases assert actual control text, binding labels, native-error translation and translated diagnostic reason, while exercising the same behavior as English. Loader smoke also checks Russian/English message selection. Graphical launcher errors follow the Windows UI language; console diagnostics stay English.
- Release layout puts the four addon files directly in Interface/AddOns/NozdorAction. The DLL/launcher retain client-prototype/bin so their existing game-path lookup works. Start-NozdorAction.cmd in the game root launches that binary and supports --check. The archive has no WoW.exe, accounts or SavedVariables.
- tools/package_release.py validates translation coverage/format placeholders, checks the ZIP CRC and exact manifest/content, extracts the files, adds a test copy of the supported EXE outside the ZIP, and runs both the relocated launcher and root CMD with --check. These checks do not start the game.
- Russian layout, fonts and text wrapping still require visual confirmation inside WoW. No gameplay or protocol change was introduced beyond the 0.3.4 version label.

## 0.3.3 assignable cursor key

- Added Cursor (hold) / Apply Cursor to the standard addon options. A single named keyboard key or mouse button 3-5 maps to a Windows virtual key and is saved in SavedVariables. ALT is the migration/default/reset value. Unknown, empty and combined keys are rejected without changing the previous assignment; existing WoW actions are not rebound or suppressed.
- Protocol 4 requires 17 configure arguments (command plus 16 values). The final numeric virtual key must be a finite integer in 1..254. Status returns a ninth boolean reporting the configured key's current held state. Both addon and DLL must be updated together.
- Native Tick polls the configured key instead of fixed VK_MENU. The key releases owned mouselook and returns the cursor through the existing native path; releasing it resumes capture. Foreground and heartbeat checks remain. Interaction also checks the key directly before queueing a request, and the addon uses native held state rather than IsAltKeyDown.
- All six CTest checks passed. New core cases cover valid virtual keys and zero, negative, out-of-range, fractional, NaN and infinite values. Addon cases cover fallback ALT, normalized F8 assignment, rejection without mutation, BUTTON4/RSHIFT, the old Alt no longer pausing after reassignment, held-state pause and release, interaction blocking, reset and rejection of protocol 3.
- No new client-native addresses or code guards were required. Actual GetAsyncKeyState/mouselook/cursor behavior for the assigned key still needs in-game confirmation; host mocks do not simulate keyboard hardware or secure UI. Backup: backups/0.3.2-before-cursor-key-20261104-174016.

## 0.3.2 NPC selection and pause regression

- Removed attack-only eligibility from both candidate acquisition and final revalidation: CanAttack, dead, flags2/stand-state attack filters no longer exclude friendly/neutral/dead units. Unit resolution, self exclusion, valid positions, range, switching delay and native Target selectability checks remain. Renamed PickEnemy to PickUnit and updated the setting label. Automatic selection still calls Target, while the default interaction dispatcher is reached only for a queued Interact-key request.
- Rechecked Target 0x524BF0 and picking filter 0x4F7350 in IDA. Gameobject vtable 0xA34640 +0xA8 points to 0x8E5250, which returns false; native Target does not make ordinary targets of mailboxes/gameobjects. The unit slot delegates through 0x6E6EC0. See npc-selection-evidence.json. No new native entry points or guards were required.
- User reported missing reticle and mouselook in 0.3.1 even after Toggle. Installed RefinedBlizzPlates/Functions.lua registers visible secure helper frames in UIPanelWindows. Reproduced the false pause in the host fixture using such a frame; removed the broad registry scan and explicitly listed the standard user-facing panels instead. UISpecialFrames, standard bags and confirmation dialogs remain covered.
- Rebuilt the DLL with warnings as errors and passed all six CTest checks. Addon regression includes a visible registered secure helper that must not pause, open bags/panels that must pause, and repeated updates that must not dispatch interaction. These do not execute the native NPC filter inside WoW.
- New behavior and the user's runtime regression still require in-game confirmation after restarting through the launcher with DLL/addon 0.3.2. The original EXE is unchanged. Backup: backups/0.3.1-before-npc-fix-20261104-172818.

## Addon 0.3.1 follow-up

- Added temporary capture/selection/interaction pauses for all standard container frames, confirmation popups, visible UIPanelWindows and UISpecialFrames entries, plus guild bank, key ring, color picker and stack splitting. Hidden parents and missing load-on-demand frames do not pause. Closing one bag does not resume capture while another remains visible.
- Installed ElvUI source registers ElvUI_ContainerFrame and ElvUI_BankContainerFrame in UISpecialFrames (Modules/Bags/Bags.lua), so its bag replacement is covered without hooks into that addon.
- The addon regression test passed with these cases. DLL and protocol are unchanged from 0.3.0; the six-check native build below is still applicable. Actual window/cursor behavior inside WoW remains untested, including custom addon windows that do not register with either panel list.

## Automated checks completed

- Built the x86 DLL and launcher with installed GCC 16.2.0, CMake and Ninja, warnings as errors and assertions enabled. All nine CTest tests passed: core; native interaction capture retention; addon in English, Russian and fallback locales; exact-client acceptance; wrong-client rejection; production DLL foreign-host rejection; suspended-child DLL loading. Output: build/Testing/Temporary/LastTest.log.
- Core checks exercise camera basis/sign, collision clipping, invalid settings, target selection, an executed reversible CALL patch, cursor coordinate conversion and resize clamping, and mount lift convergence at different frame rates.
- Addon checks exercise protocol 6 configuration, standard addon settings registration, right/left shoulder migration, cursor/mount settings, temporary UI/cursor-key pauses, mode preservation across world transitions, and mode changes only through the assigned toggle action. They also check inline key assignment, secondary bindings, explicit unassignment, combat restrictions, and one interaction on key release rather than repeated requests while held.
- The Lua fixture uses x86 Lua 5.4 with mocked WoW APIs. It does not execute WoW Lua 5.1, secure UI or the renderer.
- Protocol 6 validates all 29 configure arguments, including finite numeric values and supported ranges, before applying settings. Interaction is queued onto the game thread and expires if not handled promptly.

## Static evidence and client identity

- Runtime guards cover 73 code ranges. Six new ranges for cursor and interaction were compared byte-for-byte between IDA evidence and the supported on-disk EXE by tools/generate_signatures.py. See feature-ranges.json, cursor-evidence.json and interaction-evidence.json. FNV guards detect accidental mismatches; they are not adversarial integrity protection.
- Cursor getter 0x86A0D0, setter 0x86A130 and window getter 0x86C6A0 follow the native client-coordinate/capture path. A pending Alt cursor return waits until native relative capture actually ends, including capture held by another mouse button. See cursor-research.md.
- Interaction dispatcher 0x5277B0 is called with one GUID after native picking and object validation. Living NPC eligibility uses 0x729530; corpse loot eligibility uses 0x717B60. The native default action retains its own range/state checks. Dispatch does not prove server acceptance. See interaction-research.md.
- Existing camera collision/focus ABI and downstream matrix reuse are documented in camera-shoulder-evidence.json. Mount zoom uses native zoom methods; additional lift is smoothed before the existing collision handling.
- The original WoW.exe remains unchanged. Supported SHA-256: c33f54116161b0baf3d6f3b58276688336e96c50a80e921f10a8d5a6c38a4a60.

## Previous in-game result

The user confirmed that 0.2.2 works. That version fixed picking order: Tick now consumes the previous rendered camera/model lists before OnWorldUpdate clears them. The cause and evidence are in target-pick-order-evidence.json. This ordering remains in 0.3.0 and can introduce one frame of picking latency.

The working 0.2.2 binaries and source snapshot were saved under backups/0.2.2-before-features-20261104-165433 before these changes. The development installer backs up existing addon files and preserves SavedVariables; end-user ZIP installation copies the prepared folders directly.

## In-game checks still required for 0.3.0

The agent has not started WoW or exercised these features in-game. Host checks do not replace these manual checks:

- Assign Toggle and Interact inside Interface > AddOns > Nozdor Action; verify one action on release for a friendly NPC, mailbox and lootable corpse, with no repeated requests from holding the key. Check native range behavior and existing auto-interact movement settings.
- Assign F8 or SHIFT to Cursor; verify Alt no longer releases capture, holding the chosen key releases it, and letting go resumes. Check both cursor-position choices, repeated transitions, window resize/DPI, focus loss and holding the cursor key while another mouse button holds capture. Check that normal WoW actions on the chosen key match the documented coexistence behavior.
- Mount and dismount: verify smooth distance/lift changes and return, manual wheel zoom, zoom limits, walls/ceilings and different mounts.
- Verify right/left shoulder framing and acquisition after changing sides.
- Hover friendly, neutral, hostile and dead creatures: verify ordinary targeting without opening conversation/loot or initiating an interaction. Then use Interact deliberately. Confirm normal capture/reticle with RefinedBlizzPlates active and all windows closed.
- Verify Esc/menu/chat/settings only pause input, closing them resumes it, resets do not enable the mode, and only the assigned Toggle changes its enabled state.
- Check coexistence with existing addons, frame time, death, vehicles and world transitions.

A refusal in bin/NozdorAction.log immediately after CTest is expected from the foreign-host rejection test. The next real DLL load replaces that log.
