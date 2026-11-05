# Nozdor Action 0.3.13

- [x] Soft hover without hard-target changes; LMB selects only, native mouseover/class colors, addon setting and protocol 9.
- [ ] Confirm soft hover, click selection and UI/Cell behavior inside WoW.

Scope: extend the working 0.2.2 client-only DLL and addon. Keep source, build output and release packaging under client-prototype. Preserve the original EXE and existing SavedVariables.

- [x] Verify native cursor and interaction ABIs against IDA and the supported EXE; add six verified ranges to the 73 runtime guards.
- [x] Add one assignable interaction action for the object under the crosshair: friendly NPC, mailbox/game object or lootable corpse. Dispatch once on key release through the native default action.
- [x] Add Alt cursor return choices: previous free position or crosshair center, using the native cursor path.
- [x] Replace fixed Alt with a saved assignable Cursor (hold) key in the standard options; migrate defaults to ALT, update protocol/validation and test remapping, held-state gates and reset.
- [x] Add smooth mount distance/lift adjustment and reversal on dismount.
- [x] Remove Esc mode disabling. Keep temporary interface pauses and allow enabled-state changes only through the assigned toggle action.
- [x] Add explicit right/left shoulder selection and migrate signed legacy settings.
- [x] Expose every setting and both key assignments in the standard Interface > AddOns > Nozdor Action panel; /nfa opens that same panel.
- [x] Build the x86 DLL and launcher; pass all six automated checks, including new cursor/mount and addon regressions.
- [x] Extend interface pauses to standard bags, registered panels and addon dialogs including ElvUI bags; check hidden parents, missing frames and multiple open bags in addon regressions.
- [x] Fix false pauses from visible UIPanelWindows helper frames used by RefinedBlizzPlates; retain explicit user-facing panels and add a regression check.
- [x] Expand selection to all normally selectable units/NPCs without automatically dispatching interaction; verify gameobject limitation in IDA and update settings/documentation.
- [x] Add Russian options/messages/binding labels with English fallback; pass the complete addon fixture in enUS, ruRU and deDE.
- [x] Package the addon directly under Interface/AddOns, add a root startup CMD, and verify extracted paths and relocated startup with --check.
- [x] Share cursor bindings between standard WoW Key Bindings and addon options, including two keyboard/mouse bindings and modifiers, with explicit unassignment and legacy migration.
- [x] Add a saved optional keep-target checkbox and exercise target retention/replacement in core and addon checks.
- [x] Add an optional persistent free-cursor toggle on key press, preserving hold mode by default; verify repeats, pauses and resets.
- [x] Add target/class reticle colors with a settings checkbox, class-table fallback and tests for players/NPCs/death/no target.
- [x] Close an open interaction dialog on the next Interact release without reopening/dispatching another request.
- [x] Add independent on-foot/mounted stock framing and a custom mount distance/height/shoulder profile, retaining input and reticle behavior.
- [x] Add eight reticle shapes and eight thicknesses in one generated/checked atlas, expose controls and include the asset in installation/release.
- [ ] Validate the new 0.3.0 behavior inside WoW using the manual checks in VERIFICATION.md.

The user confirmed 0.2.2 in-game. Automated checks and static evidence support 0.3.0; they do not replace the pending in-game verification.
