-- Host mocks check addon state and protocol; they do not emulate WoW rendering or secure UI.
local frames, environment, categories = {}, {}, {}
local noop = function() end
function GetLocale() return TEST_LOCALE end
function GetMouseFocus() return environment.mouseUI and InterfaceOptionsFrame or WorldFrame end
local function Region(name, parent)
    local object = {name = name, parent = parent, scripts = {}, events = {}, shown = true, textures = {}}
    function object:GetName() return self.name end
    function object:GetWidth() return rawget(self, "width") or 1280 end
    function object:GetHeight() return rawget(self, "height") or 720 end
    function object:GetEffectiveScale() return rawget(self, "scale") or 1 end
    function object:GetCenter() return rawget(self, "centerX") or 640, rawget(self, "centerY") or 360 end
    function object:SetWidth(value) self.width = value end
    function object:SetHeight(value) self.height = value end
    function object:SetScript(event, callback) self.scripts[event] = callback end
    function object:HookScript(event, callback)
        local original = self.scripts[event]
        self.scripts[event] = function(...)
            if original then original(...) end
            callback(...)
        end
    end
    function object:RegisterEvent(event) self.events[event] = true end
    function object:SetText(text) self.text = text end
    function object:SetVertexColor(r, g, b) self.color = {r, g, b} end
    function object:SetTexture(path) self.texture = path end
    function object:SetTexCoord(...) self.texCoord = {...} end
    function object:GetText() return rawget(self, "text") or "" end
    function object:SetChecked(value) self.checked = value end
    function object:GetChecked() return self.checked end
    function object:SetScrollChild(child) self.scrollChild = child end
    function object:SetMinMaxValues(low, high) self.minimum, self.maximum = low, high end
    function object:SetValue(value)
        self.value = value
        if self.scripts.OnValueChanged then self.scripts.OnValueChanged(self, value) end
    end
    function object:IsShown() return self.shown end
    function object:IsVisible()
        local parent = rawget(self, "parent")
        return self.shown and (not parent or parent:IsVisible())
    end
    function object:Show()
        if self.shown then return end
        self.shown = true
        if self.scripts.OnShow then self.scripts.OnShow(self) end
    end
    function object:Hide()
        if not self.shown then return end
        self.shown = false
        if self.scripts.OnHide then self.scripts.OnHide(self) end
    end
    function object:CreateTexture()
        local texture = Region(nil, self)
        self.textures[#self.textures + 1] = texture
        return texture
    end
    function object:CreateFontString() return Region(nil, self) end
    return setmetatable(object, {__index = function() return noop end})
end
function CreateFrame(kind, name, parent, template)
    local object = Region(name, parent)
    frames[#frames + 1] = object
    if name then
        _G[name] = object
        for _, suffix in ipairs({"Text", "Low", "High"}) do _G[name .. suffix] = Region(nil, object) end
    end
    return object
end
UIParent = Region()
WorldFrame = Region(nil, UIParent)
WorldFrame.centerX, WorldFrame.centerY, WorldFrame.scale = 400, 200, 0.8
GameMenuFrame, WorldMapFrame = Region(nil, UIParent), Region(nil, UIParent)
InterfaceOptionsFrame = Region(nil, UIParent)
InterfaceOptionsFramePanelContainer = Region(nil, InterfaceOptionsFrame)
KeyBindingFrame = Region(nil, UIParent)
GameMenuFrame:Hide(); WorldMapFrame:Hide(); InterfaceOptionsFrame:Hide(); KeyBindingFrame:Hide()
UISpecialFrames, SlashCmdList, DEFAULT_CHAT_FRAME = {}, {}, {AddMessage = noop}
function InterfaceOptions_AddCategory(panel) categories[#categories + 1] = panel end
local openedCategory
function InterfaceOptionsFrame_OpenToCategory(panel)
    openedCategory = panel
    InterfaceOptionsFrame:Show()
    panel:Show()
end
function HideUIPanel(panel) panel:Hide() end
function ShowUIPanel(panel) panel:Show() end
environment.panels = {}
function GetUIPanel(slot) return environment.panels[slot] end
local closedBags = 0
function CloseAllBags()
    closedBags = closedBags + 1
    for i = 1, NUM_CONTAINER_FRAMES or 13 do
        if _G["ContainerFrame" .. i] then _G["ContainerFrame" .. i]:Hide() end
    end
    if ElvUI_ContainerFrame then ElvUI_ContainerFrame:Hide() end
end
function LoadAddOn() error("Binding UI already mocked") end
function GetCurrentKeyBoardFocus() return environment.focus end
function IsAltKeyDown() return environment.alt end
function IsLoggedIn() return not environment.loggedOut end
function UnitExists(unit)
    if unit == "mouseover" then return not environment.noHover and not environment.noTarget end
    return unit ~= "target" or not environment.noTarget
end
function UnitIsDeadOrGhost(unit)
    if unit == "target" or unit == "mouseover" then return environment.targetDead end
    return environment.dead
end
environment.reaction = 5
function UnitReaction() return environment.reaction end
function UnitIsFriend() return environment.targetFriendly end
function UnitCanAttack() return environment.targetAttackable end
function UnitIsPlayer() return environment.targetPlayer end
function UnitClass() return "Test class", environment.targetClass end
RAID_CLASS_COLORS = {
    PRIEST = {r = 1, g = 1, b = 1}, PALADIN = {r = 0.96, g = 0.55, b = 0.73},
    DRUID = {r = 1, g = 0.49, b = 0.04}, DEATHKNIGHT = {r = 0.77, g = 0.12, b = 0.23},
}
function UnitOnTaxi() return environment.taxi end
function UnitHasVehicleUI() return environment.vehicle end
function UnitInVehicle() return environment.vehicle end
function SpellIsTargeting() return environment.spell end
function IsMounted() return environment.mounted end
function UnitGUID(unit)
    if unit == "mouseover" and environment.noHover then return nil end
    return environment.hoverGuid or "0x000000000000000B"
end
function UnitName() return "Test target" end
environment.f6 = "EXISTING_BINDING"
environment.bindings = {NOZDORACTION_TOGGLE = {}, NOZDORACTION_INTERACT = {}, NOZDORACTION_CURSOR = {}}
function GetBindingAction(key)
    for action, keys in pairs(environment.bindings) do
        for _, bound in ipairs(keys) do if bound == key then return action end end
    end
    return key == "F6" and environment.f6 or ""
end
function GetBindingKey(action) return table.unpack(environment.bindings[action]) end
function SetBinding(key, action)
    if key == "BUTTON1" or key == "BUTTON2" or key:match("%-BUTTON2$") then return false end
    if not key:match("^[A-Z0-9%-]+$") then return false end
    for _, keys in pairs(environment.bindings) do
        for i = #keys, 1, -1 do if keys[i] == key then table.remove(keys, i) end end
    end
    if key == "F6" then environment.f6 = "" end
    if action then table.insert(environment.bindings[action], key) end
    return true
end
local savedBindings, saves = {}, 0
function GetCurrentBindingSet() return 2 end
function SaveBindings(set)
    assert(set == 2); saves = saves + 1
    savedBindings = {}
    for action, keys in pairs(environment.bindings) do savedBindings[action] = {table.unpack(keys)} end
end
function InCombatLockdown() return environment.combat end
local overrides, cleared = {}, 0
function ClearOverrideBindings(owner) overrides = {}; cleared = cleared + 1 end
function SetOverrideBindingClick(owner, priority, key, button, mouseButton)
    overrides[key] = button
end
NozdorActionDB = {range = 999, delay = 0/0, size = "invalid", opacity = -2, shoulder = -1.7, cursorKey = "INVALID",
    style = 1.5, thickness = 99, mountCameraHeight = 0/0}
assert(loadfile((ADDON_FILE:gsub("NozdorAction%.lua$", "Localization.lua"))))()
local L = NozdorActionLocale
assert(L["Unknown fallback label"] == "Unknown fallback label")
assert(loadfile(ADDON_FILE))("NozdorAction")
local driver
for _, frame in ipairs(frames) do if frame.events.ADDON_LOADED then driver = frame end end
assert(driver)
local function Event(event) driver.scripts.OnEvent(driver, event, "NozdorAction") end
local function Update()
    driver.scripts.OnUpdate(driver, 0.11)
    driver.scripts.OnUpdate(driver, 0.11)
end
local function Button(text)
    text = L[text]
    for _, frame in ipairs(frames) do
        if rawget(frame, "text") == text then return frame end
    end
    error("Missing button: " .. text)
end
local function Click(button) button.scripts.OnClick(button) end
local function Check(check, value) check:SetChecked(value); Click(check) end
local function Near(actual, wanted) assert(math.abs(actual - wanted) < 0.0001, tostring(actual) .. " ~= " .. wanted) end
Event("ADDON_LOADED")
assert(NozdorActionDB.range == 40 and NozdorActionDB.delay == 100)
assert(NozdorActionDB.size == 20 and NozdorActionDB.opacity == 0.9)
assert(NozdorActionDB.style == 1 and NozdorActionDB.thickness == 3)
Near(NozdorActionDB.mountCameraHeight, 1.65)
assert(NozdorActionDB.cursorKey == "ALT" and NozdorActionCursorKey:GetText() == "ALT", "Migrate missing/invalid cursor key to ALT")
assert(NozdorActionDB.leftShoulder, "Migrate signed shoulder side"); Near(NozdorActionDB.shoulder, 1.7)
assert(not next(overrides), "Preserve existing F6")
assert(#categories == 1 and categories[1] == NozdorActionSettings and categories[1].name == "Nozdor Action")
assert(NozdorActionSettingsScroll.scrollChild == NozdorActionSettingsContent)
assert(#UISpecialFrames == 0, "Settings belong to standard options, not a floating dialog")
assert(not NozdorActionCrosshair:IsShown())
local native = {active = false, reason = "disabled", config = {}, interactions = 0,
    diagnostic = "Target probe: terrain under crosshair; kind=1", protocol = 9}
function NozdorActionNative(command, ...)
    if command == "cursorui" then
        assert(select("#", ...) == 2)
        native.cursorAllowed, native.overUI = ...
        assert(type(native.cursorAllowed) == "boolean" and type(native.overUI) == "boolean")
        return true
    elseif command == "status" then
        return native.protocol, "0.3.13", native.active, environment.pickedGuid or "0x000000000000000B",
            native.reason, 10, native.config[5], native.diagnostic, environment.cursorHeld
    elseif command == "interactcursor" then
        assert(select("#", ...) == 0)
        if not environment.cursorHeld or not native.cursorAllowed or not native.config[5] then return false end
        native.cursorInteractions = (native.cursorInteractions or 0) + 1
        return true
    elseif command == "interact" then
        assert(select("#", ...) == 0, "One shared interaction action")
        native.interactions = native.interactions + 1
        return true
    end
    assert(command == "configure")
    assert(select("#", ...) == 29, "Protocol 9 requires 29 configure values")
    native.config = {...}
    native.active = native.config[1]
    native.reason = native.active and "active" or "disabled"
    return true
end
Update()
assert(not native.config[1] and not native.config[5], "Starts OFF")
NozdorAction.OpenSettings()
assert(openedCategory == categories[1] and InterfaceOptionsFrame:IsVisible())
Click(Button("Reset camera settings")); Update()
assert(not native.config[1] and not native.config[5], "Reset camera must not enable mode")
InterfaceOptionsFrame:Hide(); Update()
NozdorAction.Toggle(); Update()
assert(native.config[1] and native.config[5] and NozdorActionCrosshair:IsShown())
Near(native.config[6], 1.25); Near(native.config[7], 0.65); Near(native.config[8], 6)
assert(native.config[9] == false and native.config[12] == false and native.config[13] == true)
Near(native.config[10], 0.25); Near(native.config[11], 2/9)
Near(native.config[14], 3); Near(native.config[15], 1)
assert(native.config[16] == 18, "Default native cursor key is ALT")
assert(native.config[17] == 0 and native.config[18] == 0 and native.config[19] == 0 and native.config[20] == false)
assert(not native.config[21] and native.config[22], "Cursor hold is default; input can switch its state")
assert(not native.config[23] and not native.config[24] and not native.config[25])
Near(native.config[26], 1.8); Near(native.config[27], 1.65); Near(native.config[28], 10)
assert(native.config[29] and NozdorActionDB.softTarget, "New and migrated settings default to soft targeting")
local function ReticleColor(r, g, b)
    Update()
    assert(#NozdorActionCrosshair.textures == 1, "One atlas texture covers every reticle shape")
    for _, texture in ipairs(NozdorActionCrosshair.textures) do
        Near(texture.color[1], r); Near(texture.color[2], g); Near(texture.color[3], b)
    end
end
assert(NozdorActionCrosshair.textures[1].texture == "Interface\\AddOns\\NozdorAction\\Reticles")
ReticleColor(0.25, 1, 0.35)
environment.pickedGuid = "0x000000000000000C"; ReticleColor(1, 1, 1)
environment.pickedGuid = nil; environment.noHover = true; ReticleColor(1, 1, 1)
environment.noHover = nil
environment.hoverGuid = "0x000000000000000b"; ReticleColor(0.25, 1, 0.35)
environment.hoverGuid = nil
environment.reaction = 2; ReticleColor(1, 0.2, 0.2)
environment.reaction = 4; ReticleColor(1, 0.85, 0.1)
environment.reaction = 8; ReticleColor(0.25, 1, 0.35)
environment.targetDead = true; ReticleColor(0.55, 0.55, 0.55)
environment.targetDead = nil; environment.noTarget = true; ReticleColor(1, 1, 1)
environment.noTarget = nil; environment.reaction = nil; environment.targetFriendly = true
ReticleColor(0.25, 1, 0.35)
environment.targetFriendly = nil; environment.targetAttackable = true; ReticleColor(1, 0.2, 0.2)
environment.targetAttackable = nil; ReticleColor(1, 1, 1)
environment.reaction = 5
environment.targetPlayer = true
for class, color in pairs(RAID_CLASS_COLORS) do
    environment.targetClass = class
    ReticleColor(color.r, color.g, color.b)
end
environment.targetClass = "PRIEST"
CUSTOM_CLASS_COLORS = {PRIEST = {r = 0.4, g = 0.6, b = 0.8}}
ReticleColor(0.4, 0.6, 0.8)
environment.targetClass = "DRUID"; ReticleColor(1, 0.49, 0.04)
environment.targetDead = true; ReticleColor(0.55, 0.55, 0.55)
environment.targetDead = nil; environment.targetClass = nil; ReticleColor(0.25, 1, 0.35)
CUSTOM_CLASS_COLORS = nil; environment.targetPlayer = nil
assert(NozdorActionSettings:IsShown() and not NozdorActionSettings:IsVisible(),
    "Hidden options parent must not pause camera because category IsShown remains true")

-- Menus pause input without changing chosen mode, including an Esc-closed menu.
for _, name in ipairs({"GameMenuFrame", "WorldMapFrame", "InterfaceOptionsFrame", "KeyBindingFrame",
    "GossipFrame", "QuestFrame", "MerchantFrame", "MailFrame", "LootFrame", "TaxiFrame",
    "BankFrame", "ClassTrainerFrame", "AuctionFrame", "TradeFrame", "ItemTextFrame",
    "VideoOptionsFrame", "AudioOptionsFrame", "GuildBankFrame", "KeyRingFrame", "ColorPickerFrame", "StackSplitFrame",
    "SettingsPanel", "AuctionHouseFrame"}) do
    if not _G[name] then _G[name] = Region(nil, UIParent); _G[name]:Hide() end
    _G[name]:Show(); Update()
    assert(not native.config[1] and native.config[5], name .. " pauses input only")
    assert(not native.cursorAllowed, name .. " blocks RMB toggles")
    local count = native.interactions; NozdorAction.Interact()
    assert(native.interactions == count, name .. " blocks interact")
    _G[name]:Hide(); Update()
    assert(native.config[1], name .. " resumes without toggle")
    assert(native.cursorAllowed, name .. " restores permission, not a cursor toggle")
end
-- Repeated interaction closes the window before the paused-input gate can reject it.
local closedGossip = 0
function CloseGossip() closedGossip = closedGossip + 1; GossipFrame:Hide() end
local beforeClose = native.interactions
GossipFrame:Show(); environment.focus = true
NozdorAction.Interact()
assert(GossipFrame:IsVisible() and closedGossip == 0, "Typing cannot close a dialog")
environment.focus = nil; NozdorAction.Interact()
assert(not GossipFrame:IsVisible() and closedGossip == 1 and native.interactions == beforeClose)
NozdorAction.Interact()
assert(native.interactions == beforeClose + 1, "A later press can interact after the dialog has closed")
GossipFrame:Show(); NozdorAction.Toggle(); NozdorAction.Interact()
assert(GossipFrame:IsVisible() and closedGossip == 1, "OFF leaves dialogs alone")
GossipFrame:Hide(); NozdorAction.Toggle(); Update()
-- Only repeat-interaction on mail closes bags, including the ElvUI container hook.
ContainerFrame1, ContainerFrame2 = Region(nil, UIParent), Region(nil, UIParent)
ElvUI_ContainerFrame = Region(nil, UIParent)
ContainerFrame1:Hide(); ContainerFrame2:Hide(); ElvUI_ContainerFrame:Hide()
function CloseMail() MailFrame:Hide() end
local bagsBeforeMail, interactionsBeforeMail = closedBags, native.interactions
GossipFrame:Show(); ContainerFrame1:Show(); NozdorAction.Interact()
assert(ContainerFrame1:IsVisible() and closedBags == bagsBeforeMail, "Other dialogs leave bags alone")
MailFrame:Show(); ContainerFrame2:Show(); ElvUI_ContainerFrame:Show(); Update()
NozdorAction.Interact(); Update()
assert(not MailFrame:IsVisible() and not ContainerFrame1:IsVisible() and not ContainerFrame2:IsVisible())
assert(not ElvUI_ContainerFrame:IsVisible() and closedBags == bagsBeforeMail + 1)
assert(native.interactions == interactionsBeforeMail and native.cursorAllowed, "Mail closes bags without a new interaction")
-- Scan visible bags and registered panels dynamically, including load-on-demand addon windows.
local function PausesAndResumes(name)
    _G[name] = Region(nil, UIParent)
    Update()
    assert(not native.config[1] and native.config[5] and not NozdorActionCrosshair:IsShown(), name)
    local count = native.interactions; NozdorAction.Interact()
    assert(native.interactions == count, name .. " blocks interaction")
    _G[name]:Hide(); Update()
    assert(native.config[1], name .. " resumes automatically")
end
NUM_CONTAINER_FRAMES = 13
for i = 1, NUM_CONTAINER_FRAMES do PausesAndResumes("ContainerFrame" .. i) end
for i = 1, 4 do PausesAndResumes("StaticPopup" .. i) end
UIPanelWindows = {CharacterFrame = {}, TradeSkillFrame = {}, MissingLazyPanel = {}, SetWorldFrameID3SecureHandler = {}}
SetWorldFrameID3SecureHandler = Region(nil, UIParent)
Update()
assert(native.config[1] and NozdorActionCrosshair:IsShown(), "Registered secure helper is not an open window")
-- The actual panel manager identifies replacement windows with arbitrary names.
for _, slot in ipairs({"left", "center", "right", "doublewide", "fullscreen"}) do
    local customPanel = Region("NozdorCustomPanel", UIParent)
    customPanel:Hide(); environment.panels[slot] = customPanel; Update()
    customPanel:Show()
    assert(not native.cursorAllowed, "Managed panel show immediately pauses capture: " .. slot)
    Update()
    assert(not native.config[1] and native.config[5] and not NozdorActionCrosshair:IsShown())
    customPanel:Hide(); Update()
    assert(native.config[1] and native.cursorAllowed)
    environment.panels[slot] = nil
end
-- The custom auction's OnHide performs its own C_AuctionHouse close operation.
local closedAuctions = 0
AuctionHouseFrame:SetScript("OnHide", function() closedAuctions = closedAuctions + 1 end)
AuctionHouseFrame:Show(); Update()
assert(not native.config[1] and not native.cursorAllowed)
local beforeAuction = native.interactions
NozdorAction.Interact()
assert(closedAuctions == 1 and native.interactions == beforeAuction and native.cursorAllowed)
MailFrame:Show(); SettingsPanel:Show(); NozdorAction.Interact()
assert(MailFrame:IsVisible(), "New settings window protects the dialog behind it")
SettingsPanel:Hide(); MailFrame:Hide(); Update()
PausesAndResumes("CharacterFrame")
PausesAndResumes("TradeSkillFrame")
table.insert(UISpecialFrames, "ElvUI_ContainerFrame")
table.insert(UISpecialFrames, "MissingLazyDialog")
PausesAndResumes("ElvUI_ContainerFrame")
local hiddenParent = Region(nil, UIParent); hiddenParent:Hide()
_G.ElvUI_ContainerFrame = Region(nil, hiddenParent)
Update()
assert(native.config[1], "A dialog with a hidden parent must not pause input")
ContainerFrame1:Show(); ContainerFrame13:Show(); Update()
assert(not native.config[1])
ContainerFrame1:Hide(); Update()
assert(not native.config[1], "Closing one bag must not resume while another is open")
ContainerFrame13:Hide(); Update()
assert(native.config[1])
local interactionsBeforeHover = native.interactions
for i = 1, 20 do Update() end
assert(native.config[2] and native.interactions == interactionsBeforeHover,
    "Automatic target selection must never dispatch interaction")
native.reason = "escape"; native.active = false; Update()
assert(native.config[1], "An old native Escape reason cannot switch off chosen mode")
for _, gate in ipairs({"focus", "cursorHeld", "dead", "taxi", "vehicle", "spell", "loggedOut"}) do
    environment[gate] = true; Update()
    assert(not native.config[1], gate)
    local count = native.interactions; NozdorAction.Interact()
    assert(native.interactions == count, gate .. " blocks interact")
    environment[gate] = nil; Update()
    assert(native.config[1], gate .. " resumes")
end
Event("PLAYER_LEAVING_WORLD")
assert(not native.config[1] and not native.config[5])
Event("PLAYER_ENTERING_WORLD"); Update()
assert(native.config[1] and native.config[5], "World transition preserves chosen ON mode")

-- Only the assigned toggle action changes mode; refresh clears obsolete F6 overrides.
environment.f6 = ""; Event("UPDATE_BINDINGS")
assert(overrides.F6 == "NozdorActionToggleButton")
environment.bindings.NOZDORACTION_TOGGLE = {"G", "CTRL-G"}; Event("UPDATE_BINDINGS")
assert(not next(overrides) and cleared >= 3, "Custom toggle must remove old F6 override")
NozdorActionToggleKey:SetText("ctrl-f"); Click(Button("Apply Toggle"))
assert(GetBindingAction("CTRL-F") == "NOZDORACTION_TOGGLE" and GetBindingAction("G") == "")
assert(GetBindingAction("CTRL-G") == "NOZDORACTION_TOGGLE", "Keep secondary binding")
assert(savedBindings.NOZDORACTION_TOGGLE[1] == "CTRL-F" and savedBindings.NOZDORACTION_TOGGLE[2] == "CTRL-G" and saves == 1)
assert(NozdorActionToggleKey:GetText() == "CTRL-F", "Editor must show the new primary, not the preserved secondary")
assert(not next(overrides), "Inline assignment clears fallback override")
NozdorActionInteractKey:SetText("BUTTON4"); Click(Button("Apply Interact"))
assert(GetBindingAction("BUTTON4") == "NOZDORACTION_INTERACT" and saves == 2)
local oldSaves = saves
environment.combat = true; NozdorActionToggleKey:SetText("F7"); Click(Button("Apply Toggle"))
local oldCleared = cleared
Event("UPDATE_BINDINGS")
assert(cleared == oldCleared, "No protected override mutation during combat")
environment.combat = nil
Event("PLAYER_REGEN_ENABLED")
assert(saves == oldSaves and GetBindingAction("F7") == "", "No combat key mutation")
NozdorActionInteractKey:SetText("NOT A KEY"); Click(Button("Apply Interact"))
assert(saves == oldSaves and GetBindingAction("BUTTON4") == "NOZDORACTION_INTERACT")
NozdorActionToggleKey:SetText(""); Click(Button("Apply Toggle"))
assert(not GetBindingKey("NOZDORACTION_TOGGLE") and not next(overrides), "Explicit unassign also suppresses F6 fallback")
NozdorActionToggleKey:SetText("G"); Click(Button("Apply Toggle"))
assert(savedBindings.NOZDORACTION_TOGGLE[1] == "G" and not NozdorActionDB.toggleUnbound)
SlashCmdList.NOZDORACTION("off"); SlashCmdList.NOZDORACTION("toggle"); Update()
assert(native.config[1], "Slash commands cannot switch mode")
NozdorAction.Toggle(); Update()
Event("PLAYER_LEAVING_WORLD"); Event("PLAYER_ENTERING_WORLD"); Update()
assert(not native.config[1], "World transition preserves chosen OFF mode")
NozdorAction.Toggle(); Update()

-- Real registered controls all reach the protocol; both shoulders are explicit choices.
SlashCmdList.NOZDORACTION("")
assert(openedCategory == NozdorActionSettings and InterfaceOptionsFrame:IsVisible())
Check(NozdorActionAutoTarget, false)
assert(native.config[2] == false)
assert(NozdorActionRange.minimum == 5 and NozdorActionRange.maximum == 300)
for _, range in ipairs({200, 300}) do
    NozdorActionRange:SetValue(range)
    assert(NozdorActionDB.range == range and native.config[3] == range, "Long range reaches DLL configuration")
end
NozdorActionRange:SetValue(55); NozdorActionDelay:SetValue(250)
NozdorActionCameraDistance:SetValue(8.5); NozdorActionShoulder:SetValue(1.8)
NozdorActionHeight:SetValue(1.2); NozdorActionMountDistance:SetValue(4.5)
NozdorActionMountHeight:SetValue(1.5)
Check(NozdorActionCursorCenter, true); Check(NozdorActionMountAdapt, false)
NozdorActionCursorKey:SetText(" f8 "); Click(Button("Apply Cursor"))
assert(NozdorActionDB.cursorKey == "F8" and native.config[16] == 119 and NozdorActionCursorKey:GetText() == "F8")
NozdorActionCursorKey:SetText("MOUSEWHEELUP"); Click(Button("Apply Cursor"))
assert(NozdorActionDB.cursorKey == "F8" and native.config[16] == 119, "Reject wheel input without changing the key")
NozdorActionCursorKey:SetText(""); Click(Button("Apply Cursor"))
assert(NozdorActionDB.cursorUnbound and native.config[16] == 0, "Empty cursor field disables bindings and fallback")
NozdorActionCursorKey:SetText("F8"); Click(Button("Apply Cursor"))
InterfaceOptionsFrame:Hide(); Update()
environment.alt = true; Update()
assert(native.config[1], "ALT is no longer a fixed pause key after remapping")
environment.alt = nil; environment.cursorHeld = true; Update()
assert(not native.config[1] and native.config[5] and not NozdorActionCrosshair:IsShown(), "Assigned cursor key pauses input, not camera")
assert(native.config[22], "Free cursor must still accept the next toggle key")
local heldInteractions = native.interactions; NozdorAction.Interact()
assert(native.interactions == heldInteractions, "Cursor key blocks interactions")
environment.cursorHeld = nil; Update()
assert(native.config[1], "Releasing cursor key resumes without toggling mode")
NozdorAction.OpenSettings()
Check(NozdorActionKeepTarget, true)
assert(NozdorActionDB.keepTarget and native.config[20])
Check(NozdorActionKeepTarget, false)
assert(not native.config[20])
Check(NozdorActionCursorToggle, true)
assert(NozdorActionDB.cursorToggle and native.config[21] and not native.config[22], "Menu blocks cursor toggles without disabling camera")
Check(NozdorActionCursorToggle, false)
assert(not native.config[21])
Check(NozdorActionTargetColor, false); ReticleColor(1, 1, 1)
Check(NozdorActionTargetColor, true); ReticleColor(0.25, 1, 0.35)
for style = 1, 8 do
    assert(NozdorActionDB.style == style)
    for thickness = 1, 8 do
        NozdorActionThickness:SetValue(thickness)
        local uv = NozdorActionCrosshair.textures[1].texCoord
        Near(uv[1], (thickness - 1) / 8); Near(uv[2], thickness / 8)
        Near(uv[3], (style - 1) / 8); Near(uv[4], style / 8)
    end
    Click(Button("Next shape"))
end
assert(NozdorActionDB.style == 1)
Click(Button("Previous shape")); assert(NozdorActionDB.style == 8)
Click(Button("Next shape")); NozdorActionThickness:SetValue(3)
Check(NozdorActionStockCamera, true)
Check(NozdorActionMountStockCamera, true)
Check(NozdorActionMountCustom, true)
Check(NozdorActionMountLeftShoulder, true)
NozdorActionMountCameraDistance:SetValue(14)
NozdorActionMountShoulder:SetValue(2.5)
NozdorActionMountCameraHeight:SetValue(2.25)
assert(native.config[23] and native.config[24] and native.config[25] and native.config[5])
Near(native.config[26], -2.5); Near(native.config[27], 2.25); Near(native.config[28], 14)
NozdorActionCursorKey:SetText("button4"); Click(Button("Apply Cursor"))
assert(NozdorActionDB.cursorKey == "BUTTON4" and native.config[16] == 5)
NozdorActionCursorKey:SetText("rshift"); Click(Button("Apply Cursor"))
assert(NozdorActionDB.cursorKey == "RSHIFT" and native.config[16] == 161)
Click(Button("Left shoulder"))
Near(native.config[6], -1.8)
Click(Button("Right shoulder"))
Near(native.config[6], 1.8)
assert(native.config[3] == 55 and native.config[4] == 250)
Near(native.config[7], 1.2); Near(native.config[8], 8.5)
assert(native.config[9] and not native.config[13])
Near(native.config[14], 4.5); Near(native.config[15], 1.5)
environment.mounted = true; Update(); assert(native.config[12])
Check(NozdorActionMountAdapt, true); assert(native.config[13])
environment.mounted = nil
NozdorActionSize:SetValue(30); NozdorActionOpacity:SetValue(0.5)
assert(NozdorActionDB.size == 30 and NozdorActionDB.opacity == 0.5)
assert(NozdorActionCrosshair:IsShown(), "Settings provide reticle preview")
NozdorActionSettings.default()
assert(NozdorActionDB.size == 20 and NozdorActionDB.opacity == 0.9 and native.config[2])
assert(not native.config[9] and native.config[13] and not NozdorActionDB.leftShoulder)
assert(not native.config[20], "Reset disables target retention")
assert(not native.config[21], "Reset restores cursor hold mode")
assert(NozdorActionDB.targetColor, "Reset restores target colors")
assert(NozdorActionDB.style == 1 and NozdorActionDB.thickness == 3)
assert(not native.config[23] and not native.config[24] and not native.config[25])
assert(NozdorActionDB.cursorKey == "ALT" and native.config[16] == 18 and NozdorActionCursorKey:GetText() == "ALT")
Near(native.config[6], 1.25); Near(native.config[14], 3); Near(native.config[15], 1)
InterfaceOptionsFrame:Hide(); Update()
assert(native.config[1], "Reset defaults preserves ON mode")
NozdorAction.Toggle()
NozdorAction.OpenSettings(); Click(Button("Reset all settings"))
InterfaceOptionsFrame:Hide(); Update()
assert(not native.config[1] and not native.config[5], "Reset all preserves OFF mode")
NozdorAction.Toggle(); Update()

-- Standard WoW binding changes and the inline cursor editor share one action.
KeyBindingFrame:Show()
SetBinding("BUTTON5", "NOZDORACTION_CURSOR"); Event("UPDATE_BINDINGS"); Update()
assert(not native.config[22], "Binding assignment cannot toggle the cursor")
SetBinding("BUTTON5"); Event("UPDATE_BINDINGS")
KeyBindingFrame:Hide(); Update()
assert(not NozdorActionDB.cursorUnbound and native.config[16] == 18,
    "Cancelling a temporary standard binding must preserve the original ALT fallback")
SetBinding("BUTTON3", "NOZDORACTION_CURSOR")
SetBinding("SHIFT-BUTTON5", "NOZDORACTION_CURSOR")
Event("UPDATE_BINDINGS"); Update()
assert(native.config[16] == 4 and native.config[17] == 0)
assert(native.config[18] == 6 and native.config[19] == 1, "Second mouse binding carries SHIFT")
assert(NozdorActionCursorKey:GetText() == "BUTTON3")
NozdorAction.OpenSettings()
NozdorActionCursorKey:SetText("ctrl-button4"); Click(Button("Apply Cursor"))
assert(savedBindings.NOZDORACTION_CURSOR[1] == "CTRL-BUTTON4" and savedBindings.NOZDORACTION_CURSOR[2] == "SHIFT-BUTTON5")
assert(native.config[16] == 5 and native.config[17] == 2 and native.config[18] == 6 and native.config[19] == 1)
NozdorActionCursorKey:SetText("CTRL-CTRL-F8"); Click(Button("Apply Cursor"))
assert(GetBindingKey("NOZDORACTION_CURSOR") == "CTRL-BUTTON4", "Malformed combination does not change bindings")
NozdorActionCursorKey:SetText("CTRL-SHIFT-BUTTON4"); Click(Button("Apply Cursor"))
assert(native.config[17] == 3, "Combined modifier mask reaches the DLL")
SetBinding("CTRL-SHIFT-BUTTON4"); SetBinding("SHIFT-BUTTON5")
Event("UPDATE_BINDINGS"); Update()
assert(NozdorActionDB.cursorUnbound and native.config[16] == 0 and native.config[18] == 0,
    "Unassigning in standard WoW UI must not reactivate legacy ALT")
assert(NozdorActionCursorKey:GetText() == "")
Event("UPDATE_BINDINGS"); Update()
assert(native.config[16] == 0, "Unassignment survives another refresh")
NozdorActionCursorKey:SetText("ALT"); Click(Button("Apply Cursor"))
assert(not NozdorActionDB.cursorUnbound and native.config[16] == 18 and not GetBindingKey("NOZDORACTION_CURSOR"))
InterfaceOptionsFrame:Hide(); Update()

-- RMB uses the native path because the stock binding API reserves BUTTON2.
NozdorAction.OpenSettings()
Check(NozdorActionCursorToggle, false)
Click(Button("Use RMB to toggle cursor"))
assert(NozdorActionDB.cursorKey == "BUTTON2" and NozdorActionDB.cursorToggle)
assert(native.config[16] == 2 and native.config[17] == 0 and native.config[21])
assert(not GetBindingKey("NOZDORACTION_CURSOR") and not native.cursorAllowed)
environment.mouseUI = true; Update()
assert(native.overUI and not native.cursorAllowed, "RMB remains a UI click in open settings")
environment.mouseUI = nil; InterfaceOptionsFrame:Hide(); Update()
assert(not native.overUI and native.cursorAllowed)
ContainerFrame1:Show()
assert(not native.cursorAllowed, "Showing a watched bag blocks RMB immediately")
driver.scripts.OnUpdate(driver, 0.001)
assert(not native.cursorAllowed, "An open bag blocks capture without waiting for the 100 ms configure tick")
ContainerFrame1:Hide(); driver.scripts.OnUpdate(driver, 0.001)
assert(native.cursorAllowed)
environment.cursorHeld = true; Update()
local cursorClicks = native.cursorInteractions or 0
WorldFrame.scripts.OnMouseUp(WorldFrame, "LeftButton")
assert((native.cursorInteractions or 0) == cursorClicks, "Soft mode LMB never dispatches an interaction")
Check(NozdorActionSoftTarget, false)
assert(not native.config[29], "Soft mode is available in addon settings")
WorldFrame.scripts.OnMouseUp(WorldFrame, "LeftButton")
assert(native.cursorInteractions == cursorClicks + 1, "Free cursor LMB interacts at the cursor, not reticle")
WorldFrame.scripts.OnMouseUp(WorldFrame, "RightButton")
assert(native.cursorInteractions == cursorClicks + 1)
environment.mouseUI = true
WorldFrame.scripts.OnMouseUp(WorldFrame, "LeftButton")
assert(native.cursorInteractions == cursorClicks + 1, "Cell/UI clicks never become world interaction")
environment.mouseUI = nil; environment.focus = true
WorldFrame.scripts.OnMouseUp(WorldFrame, "LeftButton")
assert(native.cursorInteractions == cursorClicks + 1, "Typing blocks pointer interaction")
environment.focus = nil; ContainerFrame1:Show(); Update()
WorldFrame.scripts.OnMouseUp(WorldFrame, "LeftButton")
assert(native.cursorInteractions == cursorClicks + 1, "Bags block pointer interaction behind the panel")
ContainerFrame1:Hide(); environment.cursorHeld = nil; Update()
WorldFrame.scripts.OnMouseUp(WorldFrame, "LeftButton")
assert(native.cursorInteractions == cursorClicks + 1, "Mouse-look LMB does not dispatch pointer interaction")
Check(NozdorActionSoftTarget, true)
NozdorAction.OpenSettings()
NozdorActionCursorKey:SetText("CTRL-BUTTON2"); Click(Button("Apply Cursor"))
assert(native.config[16] == 2 and native.config[17] == 2)
assert(not GetBindingKey("NOZDORACTION_CURSOR"))
NozdorActionCursorKey:SetText("ALT"); Click(Button("Apply Cursor"))
Check(NozdorActionCursorToggle, false)
InterfaceOptionsFrame:Hide(); Update()

-- Shared XML binding dispatches once on release, never on repeated key-down.
local xmlFile = assert(io.open(ADDON_FILE:gsub("NozdorAction%.lua$", "Bindings.xml"), "rb"))
local xml = xmlFile:read("*a"); xmlFile:close()
local cursorAttributes, cursorBody = xml:match('<Binding ([^>]-name="NOZDORACTION_CURSOR"[^>]*)>(.-)</Binding>')
assert(cursorAttributes and cursorAttributes:find('runOnUp="true"', 1, true))
local invokeCursor = assert(load(cursorBody))
local beforeCursor = native.interactions
keystate = "down"; environment.cursorHeld = true
for i = 1, 20 do invokeCursor() end
assert(not native.config[1] and native.config[5], "Repeated cursor binding downs only pause, never toggle mode")
keystate = "up"; environment.cursorHeld = nil; invokeCursor()
assert(native.config[1] and native.interactions == beforeCursor, "Cursor release resumes without interaction")
local _, interactBindings = xml:gsub('name="NOZDORACTION_INTERACT"', "")
assert(interactBindings == 1, "Exactly one shared interaction binding")
local attributes, binding = xml:match('<Binding ([^>]-name="NOZDORACTION_INTERACT"[^>]*)>(.-)</Binding>')
assert(attributes and attributes:find('runOnUp="true"', 1, true))
local invoke = assert(load(binding))
local before = native.interactions
keystate = "down"; for i = 1, 20 do invoke() end
assert(native.interactions == before)
keystate = "up"; invoke()
assert(native.interactions == before + 1)
Update(); Update()
assert(native.interactions == before + 1, "No per-frame interaction")
environment.focus = true; invoke(); environment.focus = nil
environment.cursorHeld = true; invoke(); environment.cursorHeld = nil
assert(native.interactions == before + 1, "No interaction in chat or while cursor key is held")
NozdorAction.Toggle(); invoke()
assert(native.interactions == before + 1, "OFF blocks interaction")
NozdorAction.Toggle()

local settingsBinding = assert(xml:match('<Binding name="NOZDORACTION_SETTINGS">(.-)</Binding>'))
assert(load(settingsBinding))()
assert(InterfaceOptionsFrame:IsVisible() and openedCategory == NozdorActionSettings)
Click(Button("Assign keys")); Update()
assert(KeyBindingFrame:IsVisible() and not InterfaceOptionsFrame:IsVisible() and not native.config[1])
KeyBindingFrame:Hide(); Update()
assert(native.config[1], "Leaving key binding UI resumes selected mode")

local messages = {}
DEFAULT_CHAT_FRAME.AddMessage = function(self, message) messages[#messages + 1] = message end
environment.focus = true; Update()
SlashCmdList.NOZDORACTION("status")
assert(not native.config[1] and messages[1]:find("0.3.13", 1, true))
if TEST_LOCALE == "ruRU" then
    assert(BINDING_NAME_NOZDORACTION_SETTINGS == "Открыть настройки")
    assert(BINDING_NAME_NOZDORACTION_CURSOR == "Свободный курсор")
    assert(NozdorActionKeepTargetText:GetText() == "Сохранять цель до следующей")
    assert(NozdorActionCursorToggleText:GetText() == "Курсор по нажатию")
    assert(NozdorActionTargetColorText:GetText() == "Цвет прицела по цели")
    assert(NozdorActionStockCameraText:GetText() == "Штатная камера: пешком")
    assert(Button("Apply Cursor"):GetText() == "Задать курсор")
    assert(NozdorActionRangeText:GetText():find("Дальность выбора", 1, true))
    assert(messages[2]:find("Проверка прицела: поверхность под прицелом", 1, true))
    assert(L["Mode is off or paused"] == "Режим выключен или приостановлен")
else
    assert(BINDING_NAME_NOZDORACTION_SETTINGS == "Open settings")
    assert(messages[2]:find(native.diagnostic, 1, true))
end
environment.focus = nil; Update()
NozdorActionDB.range = 300; Event("ADDON_LOADED")
assert(NozdorActionDB.range == 300 and native.config[3] == 300, "Preserve saved 300-yard range on load")
NozdorActionDB.range = 301; Event("ADDON_LOADED")
assert(NozdorActionDB.range == 40 and native.config[3] == 40, "Reject saved range beyond limit")
native.protocol = 8; Update()
assert(not NozdorActionCrosshair:IsShown(), "Reject old native protocol")
native.protocol = 9; Update()
NozdorActionNative = nil; Update()
assert(not NozdorActionCrosshair:IsShown(), "Missing DLL hides reticle")
print("PASS: protocol9; camera profiles; atlas reticles; dialog close; cursor bindings; target retention; UI gates")
