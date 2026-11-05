local ADDON = ...
local L = NozdorActionLocale
local A = {}
NozdorAction = A
BINDING_HEADER_NOZDORACTION = "Nozdor Action"
BINDING_NAME_NOZDORACTION_TOGGLE = L["Toggle action camera"]
BINDING_NAME_NOZDORACTION_SETTINGS = L["Open settings"]
BINDING_NAME_NOZDORACTION_INTERACT = L["Interact under crosshair (NPC / mail / loot)"]
BINDING_NAME_NOZDORACTION_CURSOR = L["Free cursor"]

local defaults = {autoTarget = true, softTarget = true, range = 40, delay = 100, size = 20, opacity = 0.9,
    shoulder = 1.25, height = 0.65, distance = 6, cursorCenter = false,
    mountAdapt = true, mountDistance = 3, mountHeight = 1, leftShoulder = false, toggleUnbound = false,
    cursorKey = "ALT", cursorUnbound = false, keepTarget = false, cursorToggle = false, targetColor = true,
    style = 1, thickness = 3, stockCamera = false, mountStockCamera = false, mountCustom = false,
    mountLeftShoulder = false, mountShoulder = 1.8, mountCameraHeight = 1.65, mountCameraDistance = 10}
local shapeNames = {"Crosshair", "Plus", "Dot", "Circle", "Circle and dot", "Square", "Diamond", "Corners"}
local cursorKeys = {ALT = 18, LALT = 164, RALT = 165, SHIFT = 16, LSHIFT = 160, RSHIFT = 161,
    CTRL = 17, LCTRL = 162, RCTRL = 163, SPACE = 32, TAB = 9, ESCAPE = 27, ENTER = 13, BACKSPACE = 8,
    INSERT = 45, DELETE = 46, HOME = 36, END = 35, PAGEUP = 33, PAGEDOWN = 34,
    LEFT = 37, UP = 38, RIGHT = 39, DOWN = 40, CAPSLOCK = 20, NUMLOCK = 144, SCROLLLOCK = 145,
    PAUSE = 19, PRINTSCREEN = 44, BUTTON2 = 2, BUTTON3 = 4, BUTTON4 = 5, BUTTON5 = 6,
    NUMPADMULTIPLY = 106, NUMPADPLUS = 107, NUMPADMINUS = 109, NUMPADDECIMAL = 110, NUMPADDIVIDE = 111,
    ["-"] = 189, ["="] = 187, ["["] = 219, ["]"] = 221, ["\\"] = 220,
    [";"] = 186, ["'"] = 222, [","] = 188, ["."] = 190, ["/"] = 191, ["`"] = 192}
for code = 65, 90 do cursorKeys[string.char(code)] = code end
for i = 0, 9 do cursorKeys[tostring(i)] = 48 + i; cursorKeys["NUMPAD" .. i] = 96 + i end
for i = 1, 24 do cursorKeys["F" .. i] = 111 + i end
local enabled, elapsed, nativeActive = false, 0, false
local stateText, nativeVersion, cameraApplied = L["DLL not connected"], "", false
local softPicked = ""
local targetDiagnostic = L["No targeting diagnostic available"]
local settings, settingsContent, statusLabel, shapeLabel
local cursorHeld = false
local cursorBindingAssigned, cursorWarning = false, nil
local controls, keyEditors, refreshing = {}, {}, false
local rightShoulderButton, leftShoulderButton
local modifierBits = {SHIFT = 1, CTRL = 2, ALT = 4}
local modifierKeys = {[16] = true, [17] = true, [18] = true, [160] = true, [161] = true,
    [162] = true, [163] = true, [164] = true, [165] = true}

local function ParseCursorBinding(key)
    if type(key) ~= "string" or key == "" then return nil end
    local modifiers, seen = 0, {}
    while true do
        local modifier, rest = key:match("^(%u+)%-(.*)$")
        if not modifier then break end
        local bit = modifierBits[modifier]
        if not bit or seen[modifier] then return nil end
        modifiers = modifiers + bit; seen[modifier] = true; key = rest
    end
    local code = cursorKeys[key]
    if not code or (modifiers ~= 0 and modifierKeys[code]) then return nil end
    return code, modifiers
end

local function UpdateCursorBindingState()
    if KeyBindingFrame and KeyBindingFrame:IsVisible() then return end
    local first, second = GetBindingKey("NOZDORACTION_CURSOR")
    if first or second then NozdorActionDB.cursorUnbound = false
    elseif cursorBindingAssigned then NozdorActionDB.cursorUnbound = true end
    cursorBindingAssigned = not not (first or second)
end

local function CursorBindings()
    UpdateCursorBindingState()
    local first, second = GetBindingKey("NOZDORACTION_CURSOR")
    if not first and not second and not NozdorActionDB.cursorUnbound then first = NozdorActionDB.cursorKey end
    local key, modifiers = ParseCursorBinding(first)
    local secondKey, secondModifiers = ParseCursorBinding(second)
    return key or 0, modifiers or 0, secondKey or 0, secondModifiers or 0
end
local worldReady = true
local driver = CreateFrame("Frame")
local crosshair = CreateFrame("Frame", "NozdorActionCrosshair", UIParent)
crosshair:SetFrameStrata("HIGH")
crosshair:SetPoint("CENTER", WorldFrame, "CENTER", 0, 0)
crosshair:EnableMouse(false)
crosshair:Hide()
local reticle = crosshair:CreateTexture(nil, "OVERLAY")
reticle:SetAllPoints(crosshair)
reticle:SetTexture("Interface\\AddOns\\NozdorAction\\Reticles")

local labels = {
    ["hover hook unavailable"] = L["Hover input is unavailable"],
    ["right mouse unavailable"] = L["RMB input is unavailable"],
    active = L["Active"], disabled = L["Off / paused by interface"], ["cursor key"] = L["Paused: free cursor"],
    focus = L["Paused: window not active"],
    interaction = L["Paused: interaction"],
    ["spell targeting"] = L["Paused: spell targeting"],
    ["no living player"] = L["Paused: no living player"],
    ["no world"] = L["Paused: world unavailable"],
    ["no UI"] = L["Paused: UI unavailable"],
    ["waiting for addon"] = L["Waiting for addon"],
    ["addon heartbeat expired"] = L["Paused: addon heartbeat expired"],
    ["picking unavailable"] = L["Camera active; native picking unavailable"],
}

local function TranslateDiagnostic(text)
    local reason, details = text:match("^Target probe: (.-)(; kind=.*)$")
    return reason and (L["Target probe: "] .. L[reason] .. details) or L[text]
end

local SyncCursorUI
local cursorUIWatched = setmetatable({}, {__mode = "k"})
local function Shown(frame)
    if frame and not cursorUIWatched[frame] and type(frame.HookScript) == "function" then
        cursorUIWatched[frame] = true
        local function Changed() if SyncCursorUI then SyncCursorUI() end end
        frame:HookScript("OnShow", Changed)
        frame:HookScript("OnHide", Changed)
    end
    return frame and frame:IsVisible()
end

local pauseFrames = {"GameMenuFrame", "InterfaceOptionsFrame", "VideoOptionsFrame", "AudioOptionsFrame",
    "SettingsPanel", "AuctionHouseFrame",
    "WorldMapFrame", "KeyBindingFrame", "GossipFrame", "QuestFrame", "MerchantFrame", "MailFrame", "LootFrame",
    "TaxiFrame", "BankFrame", "ClassTrainerFrame", "AuctionFrame", "TradeFrame", "ItemTextFrame",
    "GuildBankFrame", "KeyRingFrame", "ColorPickerFrame", "StackSplitFrame",
    "CharacterFrame", "SpellBookFrame", "PlayerTalentFrame", "TalentFrame", "QuestLogFrame",
    "FriendsFrame", "PVPParent", "LFDParentFrame", "LFGParentFrame", "AchievementFrame",
    "TradeSkillFrame", "CraftFrame", "InspectFrame", "ItemSocketingFrame", "MacroFrame",
    "HelpFrame", "CalendarFrame", "DressUpFrame"}
local panelSlots = {"left", "center", "right", "doublewide", "fullscreen"}

local function SettingsShown()
    return Shown(InterfaceOptionsFrame) or Shown(SettingsPanel)
end

local function CanUseCamera()
    if not enabled or not worldReady then return false end
    if not IsLoggedIn() or not UnitExists("player") or UnitIsDeadOrGhost("player") then return false end
    if UnitOnTaxi("player") or (UnitHasVehicleUI and UnitHasVehicleUI("player")) then return false end
    if UnitInVehicle and UnitInVehicle("player") then return false end
    if Shown(CinematicFrame) or Shown(MovieFrame) then return false end
    return true
end

local function CanRun(ignoreCursor)
    if not CanUseCamera() or Shown(settings) or GetCurrentKeyBoardFocus() or
        (not ignoreCursor and cursorHeld) or SpellIsTargeting() then return false end
    for _, name in ipairs(pauseFrames) do
        if Shown(_G[name]) then return false end
    end
    -- Query panels actually displayed by the manager, not registered secure helper frames.
    if type(GetUIPanel) == "function" then
        for _, slot in ipairs(panelSlots) do
            if Shown(GetUIPanel(slot)) then return false end
        end
    end
    for i = 1, NUM_CONTAINER_FRAMES or 13 do
        if Shown(_G["ContainerFrame" .. i]) then return false end
    end
    for i = 1, STATICPOPUP_NUMDIALOGS or 4 do
        if Shown(_G["StaticPopup" .. i]) then return false end
    end
    -- UIPanelWindows can contain always-visible secure helper frames (RefinedBlizzPlates).
    -- ElvUI bags and addon dialogs register here instead of using Blizzard panels.
    for _, name in ipairs(UISpecialFrames or {}) do
        if Shown(_G[name]) then return false end
    end
    return true
end

SyncCursorUI = function()
    if not NozdorActionDB or type(NozdorActionNative) ~= "function" then return end
    local focus = GetMouseFocus and GetMouseFocus()
    local overUI = focus and focus ~= WorldFrame and focus ~= UIParent or false
    pcall(NozdorActionNative, "cursorui", CanRun(true), overUI)
end

local interactionWindows = {
    {"GossipFrame", "CloseGossip"}, {"QuestFrame", "CloseQuest"}, {"MerchantFrame", "CloseMerchant"},
    {"ClassTrainerFrame", "CloseTrainer"}, {"MailFrame", "CloseMail"}, {"LootFrame", "CloseLoot"},
    {"ElvLootFrame", "CloseLoot"}, {"BankFrame", "CloseBankFrame"}, {"AuctionFrame", "CloseAuctionHouse"},
    {"GuildBankFrame", "CloseGuildBankFrame"}, {"ItemTextFrame", "CloseItemText"}, {"TaxiFrame", "CloseTaxiMap"},
    {"AuctionHouseFrame", "CloseAuctionHouse"},
}

local function CloseInteractionWindow()
    if not CanUseCamera() or GetCurrentKeyBoardFocus() or Shown(settings) or Shown(GameMenuFrame) or
        SettingsShown() or Shown(KeyBindingFrame) then return false end
    for _, entry in ipairs(interactionWindows) do
        local frame = _G[entry[1]]
        if Shown(frame) then
            local close = _G[entry[2]]
            if entry[1] == "AuctionHouseFrame" then HideUIPanel(frame)
            elseif type(close) == "function" then close() else HideUIPanel(frame) end
            if entry[1] == "MailFrame" and type(CloseAllBags) == "function" then CloseAllBags() end
            return true
        end
    end
    return false
end

local function CrosshairCenter()
    local x, y = WorldFrame:GetCenter()
    local scale = WorldFrame:GetEffectiveScale() / UIParent:GetEffectiveScale()
    if not x or not y then return 0.5, 0.5 end
    return math.max(0, math.min(1, x * scale / UIParent:GetWidth())),
        math.max(0, math.min(1, y * scale / UIParent:GetHeight()))
end

local function UpdateReticle()
    local db = NozdorActionDB or defaults
    local size = db.size
    crosshair:SetWidth(size)
    crosshair:SetHeight(size)
    crosshair:SetAlpha(db.opacity)
    reticle:SetTexCoord((db.thickness - 1) / 8, db.thickness / 8, (db.style - 1) / 8, db.style / 8)
    if shapeLabel then shapeLabel:SetText(L[shapeNames[db.style]]) end
    local r, g, b = 1, 1, 1
    local unit = "target"
    if db.softTarget and nativeActive and db.autoTarget then
        local hover = UnitGUID("mouseover")
        unit = hover and softPicked ~= "" and string.lower(hover) == string.lower(softPicked) and "mouseover" or nil
    end
    if db.targetColor and unit and UnitExists(unit) then
        local classColor
        if UnitIsPlayer(unit) then
            local _, class = UnitClass(unit)
            classColor = (CUSTOM_CLASS_COLORS and CUSTOM_CLASS_COLORS[class]) or (RAID_CLASS_COLORS and RAID_CLASS_COLORS[class])
        end
        if UnitIsDeadOrGhost(unit) then r, g, b = 0.55, 0.55, 0.55
        elseif classColor then r, g, b = classColor.r, classColor.g, classColor.b
        else
            local reaction = UnitReaction(unit, "player")
            if reaction and reaction <= 3 then r, g, b = 1, 0.2, 0.2
            elseif reaction == 4 then r, g, b = 1, 0.85, 0.1
            elseif (reaction and reaction >= 5) or UnitIsFriend("player", unit) then r, g, b = 0.25, 1, 0.35
            elseif UnitCanAttack("player", unit) then r, g, b = 1, 0.2, 0.2 end
        end
    end
    reticle:SetVertexColor(r, g, b)
    if (nativeActive and CanRun()) or Shown(settings) then crosshair:Show()
    else crosshair:Hide() end
    if statusLabel then
        local target = UnitName("target") or L["none"]
        statusLabel:SetText(string.format(L["DLL: %s\nCamera: %s\nMode: %s   %s   Target: %s"],
            nativeVersion ~= "" and nativeVersion or L["not connected"], cameraApplied and L["SHOULDER"] or L["stock / off"],
            enabled and L["ON"] or L["OFF"], stateText, target))
    end
    if rightShoulderButton then
        rightShoulderButton:SetText(L["Right shoulder"] .. (NozdorActionDB.leftShoulder and "" or " [X]"))
        leftShoulderButton:SetText(L["Left shoulder"] .. (NozdorActionDB.leftShoulder and " [X]" or ""))
    end
end

local function Sync()
    if not NozdorActionDB then return end
    local native = NozdorActionNative
    if type(native) ~= "function" then
        nativeActive, nativeVersion, cameraApplied = false, "", false
        stateText = L["DLL not connected. Start with NozdorActionLauncher.exe."]
        UpdateReticle()
        return
    end
    local ok, protocol, version, active, picked, reason, frames, shifted, diagnostic, held = pcall(native, "status")
    if not ok or protocol ~= 9 then
        nativeActive, cameraApplied = false, false
        stateText = L["Update DLL and addon to compatible versions"]
        UpdateReticle()
        return
    end
    nativeVersion, nativeActive = tostring(version), active
    softPicked = type(picked) == "string" and picked or ""
    cursorHeld = not not held
    targetDiagnostic = TranslateDiagnostic(diagnostic or L["Update DLL to 0.2.1 for targeting diagnostics"])
    cameraApplied = shifted
    local centerX, centerY = CrosshairCenter()
    local cursorKey, cursorModifiers, secondKey, secondModifiers = CursorBindings()
    local sent, accepted, errorText = pcall(native, "configure", CanRun(),
        NozdorActionDB.autoTarget, NozdorActionDB.range, NozdorActionDB.delay, CanUseCamera(),
        NozdorActionDB.leftShoulder and -NozdorActionDB.shoulder or NozdorActionDB.shoulder,
        NozdorActionDB.height, NozdorActionDB.distance,
        NozdorActionDB.cursorCenter, centerX, centerY, not not IsMounted(),
        NozdorActionDB.mountAdapt, NozdorActionDB.mountDistance, NozdorActionDB.mountHeight,
        cursorKey, cursorModifiers, secondKey, secondModifiers, NozdorActionDB.keepTarget,
        NozdorActionDB.cursorToggle, CanRun(true), NozdorActionDB.stockCamera, NozdorActionDB.mountStockCamera,
        NozdorActionDB.mountCustom, NozdorActionDB.mountLeftShoulder and -NozdorActionDB.mountShoulder or NozdorActionDB.mountShoulder,
        NozdorActionDB.mountCameraHeight, NozdorActionDB.mountCameraDistance, NozdorActionDB.softTarget)
    SyncCursorUI()
    stateText = labels[reason] or tostring(reason)
    if not sent or not accepted then
        stateText = L["Configuration rejected: "] .. L[tostring(errorText or accepted)]
    end
    UpdateReticle()
end

function A.Toggle()
    enabled = not enabled
    Sync()
end

function A.Cursor()
    Sync()
end

function A.Interact()
    Sync()
    if CloseInteractionWindow() then Sync(); return end
    if not CanRun() or type(NozdorActionNative) ~= "function" then return end
    local ok, accepted, reason = pcall(NozdorActionNative, "interact")
    if not ok or not accepted then
        DEFAULT_CHAT_FRAME:AddMessage("Nozdor Action: " .. L[tostring(reason or accepted)])
    end
end

WorldFrame:HookScript("OnMouseUp", function(self, button)
    if button ~= "LeftButton" or not NozdorActionDB or not CanRun(true) or
        type(NozdorActionNative) ~= "function" then return end
    local focus = GetMouseFocus and GetMouseFocus()
    if focus and focus ~= WorldFrame and focus ~= UIParent then return end
    local first, _, second = CursorBindings()
    if first == 2 or second == 2 then pcall(NozdorActionNative, "interactcursor") end
end)

local function RefreshBindings()
    if not NozdorActionDB then return end
    UpdateCursorBindingState()
    local first, second = GetBindingKey("NOZDORACTION_CURSOR")
    local bad = first and not ParseCursorBinding(first) and first or second and not ParseCursorBinding(second) and second
    if bad and bad ~= cursorWarning then
        DEFAULT_CHAT_FRAME:AddMessage(string.format(L["Nozdor Action: %s is not a held cursor key. Use a keyboard key or BUTTON2-5."], bad))
    end
    cursorWarning = bad
    if InCombatLockdown() then return end
    ClearOverrideBindings(driver)
    if not NozdorActionDB.toggleUnbound and not GetBindingKey("NOZDORACTION_TOGGLE") and GetBindingAction("F6") == "" then
        SetOverrideBindingClick(driver, false, "F6", "NozdorActionToggleButton", "LeftButton")
    end
    for action, editor in pairs(keyEditors) do
        local key, secondary = GetBindingKey(action)
        if action == "NOZDORACTION_CURSOR" and not key and not NozdorActionDB.cursorUnbound then key = NozdorActionDB.cursorKey end
        editor:SetText(key or "")
        editor.secondary:SetText(string.format(L["Secondary: %s"], secondary or L["none"]))
    end
end

local function Button(parent, text, x, y, width, callback)
    local button = CreateFrame("Button", nil, parent, "UIPanelButtonTemplate")
    button:SetWidth(width); button:SetHeight(25)
    button:SetPoint("TOPLEFT", parent, "TOPLEFT", x, y)
    button:SetText(text)
    button:SetScript("OnClick", callback)
    return button
end

local function Slider(name, text, low, high, step, key, y, x)
    local slider = CreateFrame("Slider", name, settingsContent, "OptionsSliderTemplate")
    slider:SetWidth(220); slider:SetHeight(16)
    slider:SetPoint("TOPLEFT", settingsContent, "TOPLEFT", x or 20, y)
    slider:SetMinMaxValues(low, high); slider:SetValueStep(step)
    _G[name .. "Low"]:SetText(tostring(low))
    _G[name .. "High"]:SetText(tostring(high))
    local function SetValue(value)
        value = math.floor(value / step + 0.5) * step
        NozdorActionDB[key] = value
        _G[name .. "Text"]:SetText(text .. ": " .. string.format(step < 1 and "%.2f" or "%d", value))
        if not refreshing then Sync() end
    end
    slider:SetValue(NozdorActionDB[key])
    SetValue(NozdorActionDB[key])
    slider:SetScript("OnValueChanged", function(self, value) SetValue(value) end)
    controls[key] = slider
end

local function CreateSettings()
    settings = CreateFrame("Frame", "NozdorActionSettings", InterfaceOptionsFramePanelContainer or UIParent)
    settings.name = "Nozdor Action"
    settings:Hide()
    local title = settings:CreateFontString(nil, "OVERLAY", "GameFontNormalLarge")
    title:SetPoint("TOPLEFT", 16, -16); title:SetText("Nozdor Action 0.3.15")
    local scroll = CreateFrame("ScrollFrame", "NozdorActionSettingsScroll", settings, "UIPanelScrollFrameTemplate")
    scroll:SetPoint("TOPLEFT", settings, "TOPLEFT", 0, -45)
    scroll:SetPoint("BOTTOMRIGHT", settings, "BOTTOMRIGHT", -30, 8)
    settingsContent = CreateFrame("Frame", "NozdorActionSettingsContent", scroll)
    settingsContent:SetWidth(550); settingsContent:SetHeight(1450)
    scroll:SetScrollChild(settingsContent)
    Button(settingsContent, L["Assign keys"], 20, -5, 170, function()
        if not KeyBindingFrame then LoadAddOn("Blizzard_BindingUI") end
        if KeyBindingFrame then HideUIPanel(InterfaceOptionsFrame); ShowUIPanel(KeyBindingFrame) end
    end)
    local function Check(name, text, key, y, x)
        local check = CreateFrame("CheckButton", name, settingsContent, "UICheckButtonTemplate")
        check:SetPoint("TOPLEFT", settingsContent, "TOPLEFT", x or 15, y)
        _G[name .. "Text"]:SetText(text)
        check:SetChecked(NozdorActionDB[key])
        check:SetScript("OnClick", function(self) NozdorActionDB[key] = not not self:GetChecked(); Sync() end)
        controls[key] = check
    end
    Check("NozdorActionAutoTarget", L["Select units under crosshair (all NPCs)"], "autoTarget", -45)
    Check("NozdorActionKeepTarget", L["Keep target until another is selected"], "keepTarget", -80)
    Check("NozdorActionTargetColor", L["Color crosshair by target"], "targetColor", -80, 295)
    rightShoulderButton = Button(settingsContent, L["Right shoulder"], 285, -45, 120, function()
        NozdorActionDB.leftShoulder = false; Sync()
    end)
    leftShoulderButton = Button(settingsContent, L["Left shoulder"], 415, -45, 120, function()
        NozdorActionDB.leftShoulder = true; Sync()
    end)
    Slider("NozdorActionRange", L["Acquisition range (yards)"], 5, 300, 1, "range", -125)
    Slider("NozdorActionDelay", L["Switch confirmation (ms)"], 0, 500, 25, "delay", -180)
    Slider("NozdorActionSize", L["Crosshair size"], 8, 64, 1, "size", -235)
    Slider("NozdorActionOpacity", L["Crosshair opacity"], 0.2, 1, 0.05, "opacity", -290)
    Slider("NozdorActionCameraDistance", L["Camera distance"], 3, 25, 0.25, "distance", -125, 300)
    Slider("NozdorActionShoulder", L["Shoulder offset"], 0, 3, 0.05, "shoulder", -180, 300)
    Slider("NozdorActionHeight", L["Camera lift"], -1, 3, 0.05, "height", -235, 300)
    Slider("NozdorActionMountDistance", L["Mounted: extra distance"], 0, 15, 0.25, "mountDistance", -290, 300)
    Slider("NozdorActionMountHeight", L["Mounted: extra lift"], 0, 5, 0.05, "mountHeight", -345, 300)
    Check("NozdorActionCursorCenter", L["Free cursor starts at crosshair center"], "cursorCenter", -335)
    Check("NozdorActionMountAdapt", L["Adapt camera when mounted"], "mountAdapt", -375)
    Check("NozdorActionCursorToggle", L["Toggle cursor on press"], "cursorToggle", -390, 295)
    local function RefreshControls()
        refreshing = true
        for key, control in pairs(controls) do
            if type(defaults[key]) == "boolean" then control:SetChecked(NozdorActionDB[key])
            else control:SetValue(NozdorActionDB[key]) end
        end
        refreshing = false
        RefreshBindings()
        Sync()
    end
    Button(settingsContent, L["Reset camera settings"], 300, -425, 220, function()
        NozdorActionDB.leftShoulder = false
        for _, key in ipairs({"distance", "shoulder", "height", "mountAdapt", "mountDistance", "mountHeight",
            "stockCamera", "mountStockCamera", "mountCustom", "mountLeftShoulder", "mountShoulder", "mountCameraHeight", "mountCameraDistance"}) do
            NozdorActionDB[key] = defaults[key]
        end
        RefreshControls()
    end)
    settings.default = function()
        for key, value in pairs(defaults) do NozdorActionDB[key] = value end
        RefreshControls()
    end
    Button(settingsContent, L["Reset all settings"], 20, -420, 220, settings.default)
    statusLabel = settingsContent:CreateFontString(nil, "OVERLAY", "GameFontHighlightSmall")
    statusLabel:SetPoint("TOPLEFT", 20, -475); statusLabel:SetWidth(510)
    statusLabel:SetJustifyH("LEFT")
    local help = settingsContent:CreateFontString(nil, "OVERLAY", "GameFontNormalSmall")
    help:SetPoint("TOPLEFT", 20, -535); help:SetWidth(510); help:SetJustifyH("LEFT")
    help:SetText(L["Cursor key: hold or toggle (see option). Esc: closes windows.\nMode changes only with its assigned key; menus temporarily pause input.\nKeys: F6, CTRL-F, BUTTON4; Apply replaces that key's action.\nEmpty Apply clears action keys; secondary keys are otherwise kept."])
    local function KeyEditor(name, text, applyText, action, y)
        local label = settingsContent:CreateFontString(nil, "OVERLAY", "GameFontNormal")
        label:SetPoint("TOPLEFT", 20, y); label:SetText(text)
        local editor = CreateFrame("EditBox", name, settingsContent, "InputBoxTemplate")
        editor:SetWidth(150); editor:SetHeight(25); editor:SetAutoFocus(false)
        editor:SetPoint("TOPLEFT", settingsContent, "TOPLEFT", 185, y + 5)
        editor:SetScript("OnEscapePressed", function(self) self:ClearFocus() end)
        editor.secondary = settingsContent:CreateFontString(nil, "OVERLAY", "GameFontHighlightSmall")
        editor.secondary:SetPoint("TOPLEFT", 185, y - 25)
        keyEditors[action] = editor
        Button(settingsContent, applyText, 350, y + 5, 170, function()
            if InCombatLockdown() then
                DEFAULT_CHAT_FRAME:AddMessage(L["Nozdor Action: assign keys outside combat."]); return
            end
            local key = string.upper((editor:GetText() or ""):match("^%s*(.-)%s*$"))
            local isCursor = action == "NOZDORACTION_CURSOR"
            local code, modifiers = ParseCursorBinding(key)
            if isCursor and key ~= "" and not code then
                DEFAULT_CHAT_FRAME:AddMessage(L["Nozdor Action: invalid cursor key. Use F8, CTRL-F8, BUTTON2, BUTTON3, BUTTON4 or BUTTON5."]); return
            end
            local legacyModifier = isCursor and ((modifierKeys[code] and modifiers == 0) or code == 2)
            local old = {GetBindingKey(action)}
            if key ~= "" and not legacyModifier and not SetBinding(key, action) then
                DEFAULT_CHAT_FRAME:AddMessage(L["Nozdor Action: invalid key. Examples: F6, CTRL-F, BUTTON4."]); return
            end
            local current = {GetBindingKey(action)}
            for _, previous in ipairs(current) do SetBinding(previous) end
            if key ~= "" and not legacyModifier then
                SetBinding(key, action)
                if old[2] and old[2] ~= key then SetBinding(old[2], action) end
            end
            if action == "NOZDORACTION_TOGGLE" then NozdorActionDB.toggleUnbound = key == "" end
            if isCursor then
                NozdorActionDB.cursorUnbound = key == ""
                if key ~= "" then NozdorActionDB.cursorKey = key end
                cursorBindingAssigned = key ~= "" and not legacyModifier
            end
            SaveBindings(GetCurrentBindingSet())
            editor:ClearFocus()
            RefreshBindings()
            Sync()
        end)
    end
    KeyEditor("NozdorActionToggleKey", L["Toggle"], L["Apply Toggle"], "NOZDORACTION_TOGGLE", -620)
    KeyEditor("NozdorActionInteractKey", L["Interact"], L["Apply Interact"], "NOZDORACTION_INTERACT", -690)
    KeyEditor("NozdorActionCursorKey", L["Cursor key"], L["Apply Cursor"], "NOZDORACTION_CURSOR", -760)
    local cursorHelp = settingsContent:CreateFontString(nil, "OVERLAY", "GameFontHighlightSmall")
    cursorHelp:SetPoint("TOPLEFT", 20, -825); cursorHelp:SetWidth(510); cursorHelp:SetJustifyH("LEFT")
    cursorHelp:SetText(L["RMB: BUTTON2 here, or use the button below to enable toggle.\nCtrl/Shift/Alt keep cursor. Mouse look: LMB selects.\nFree RMB cursor: LMB interacts; UI keeps normal clicks."])
    Button(settingsContent, L["Use RMB to toggle cursor"], 20, -867, 280, function()
        if InCombatLockdown() then
            DEFAULT_CHAT_FRAME:AddMessage(L["Nozdor Action: assign keys outside combat."]); return
        end
        local old = {GetBindingKey("NOZDORACTION_CURSOR")}
        for _, key in ipairs(old) do SetBinding(key) end
        NozdorActionDB.cursorKey = "BUTTON2"
        NozdorActionDB.cursorUnbound = false
        NozdorActionDB.cursorToggle = true
        cursorBindingAssigned = false
        SaveBindings(GetCurrentBindingSet())
        RefreshControls()
    end)
    local extraTitle = settingsContent:CreateFontString(nil, "OVERLAY", "GameFontNormalLarge")
    extraTitle:SetPoint("TOPLEFT", 20, -900); extraTitle:SetText(L["Reticle and mounted camera"])
    Button(settingsContent, L["Previous shape"], 20, -935, 150, function()
        NozdorActionDB.style = (NozdorActionDB.style - 2) % #shapeNames + 1; Sync()
    end)
    shapeLabel = settingsContent:CreateFontString(nil, "OVERLAY", "GameFontNormal")
    shapeLabel:SetPoint("TOPLEFT", 185, -940); shapeLabel:SetWidth(150)
    Button(settingsContent, L["Next shape"], 350, -935, 170, function()
        NozdorActionDB.style = NozdorActionDB.style % #shapeNames + 1; Sync()
    end)
    Slider("NozdorActionThickness", L["Crosshair thickness"], 1, 8, 1, "thickness", -1000)
    Check("NozdorActionStockCamera", L["Stock camera: on foot"], "stockCamera", -1040)
    Check("NozdorActionMountStockCamera", L["Stock camera: mounted"], "mountStockCamera", -1040, 295)
    Check("NozdorActionMountCustom", L["Independent mounted camera"], "mountCustom", -1090)
    Check("NozdorActionMountLeftShoulder", L["Mounted: left shoulder"], "mountLeftShoulder", -1090, 295)
    Slider("NozdorActionMountCameraDistance", L["Mounted camera distance"], 3, 40, 0.25, "mountCameraDistance", -1160)
    Slider("NozdorActionMountShoulder", L["Mounted shoulder offset"], 0, 5, 0.05, "mountShoulder", -1160, 300)
    Slider("NozdorActionMountCameraHeight", L["Mounted camera height"], -1, 8, 0.05, "mountCameraHeight", -1215)
    local profileHelp = settingsContent:CreateFontString(nil, "OVERLAY", "GameFontHighlightSmall")
    profileHelp:SetPoint("TOPLEFT", 20, -1270); profileHelp:SetWidth(510); profileHelp:SetJustifyH("LEFT")
    profileHelp:SetText(L["Stock camera keeps the reticle and targeting, without shoulder offsets or preset zoom.\nMounted priority: stock camera, then independent profile, then adaptive extras.\nThe independent profile uses its own distance, height and shoulder; walls still clip it.\nInteract again to close an open NPC, mail or loot window."])
    Check("NozdorActionSoftTarget", L["Soft target: select with LMB"], "softTarget", -1350)
    local softHelp = settingsContent:CreateFontString(nil, "OVERLAY", "GameFontHighlightSmall")
    softHelp:SetPoint("TOPLEFT", 20, -1390); softHelp:SetWidth(510); softHelp:SetJustifyH("LEFT")
    softHelp:SetText(L["Mouse look: hover previews, LMB selects. Use Interact key to talk or loot.\nFree RMB cursor: LMB interacts at the cursor. Selected target stays until another click."])
    settings.refresh, settings.okay, settings.cancel = RefreshControls, Sync, Sync
    settings:SetScript("OnShow", RefreshControls)
    settings:SetScript("OnHide", Sync)
    InterfaceOptions_AddCategory(settings)
end

function A.OpenSettings()
    if not settings then CreateSettings() end
    InterfaceOptionsFrame_OpenToCategory(settings)
    Sync()
end

local function LoadSettings()
    if type(NozdorActionDB) ~= "table" then NozdorActionDB = {} end
    for key, value in pairs(defaults) do
        if type(NozdorActionDB[key]) ~= type(value) then NozdorActionDB[key] = value end
    end
    if not ParseCursorBinding(NozdorActionDB.cursorKey) then NozdorActionDB.cursorKey = defaults.cursorKey end
    if NozdorActionDB.shoulder < 0 then
        NozdorActionDB.shoulder = -NozdorActionDB.shoulder
        NozdorActionDB.leftShoulder = true
    end
    local limits = {range = {5, 300}, delay = {0, 500}, size = {8, 64}, opacity = {0.2, 1},
        shoulder = {0, 3}, height = {-1, 3}, distance = {3, 25}, mountDistance = {0, 15}, mountHeight = {0, 5},
        style = {1, 8}, thickness = {1, 8}, mountShoulder = {0, 5}, mountCameraHeight = {-1, 8}, mountCameraDistance = {3, 40}}
    for key, bounds in pairs(limits) do
        local value = NozdorActionDB[key]
        if value ~= value or value < bounds[1] or value > bounds[2] then NozdorActionDB[key] = defaults[key] end
    end
    for _, key in ipairs({"style", "thickness"}) do
        if NozdorActionDB[key] ~= math.floor(NozdorActionDB[key]) then NozdorActionDB[key] = defaults[key] end
    end
    local toggle = CreateFrame("Button", "NozdorActionToggleButton", UIParent)
    toggle:SetScript("OnClick", A.Toggle)
    CreateSettings()
    RefreshBindings()
    SLASH_NOZDORACTION1 = "/nfa"
    SlashCmdList.NOZDORACTION = function(command)
        command = string.lower(command or "")
        if command == "toggle" or command == "off" then
            DEFAULT_CHAT_FRAME:AddMessage(L["Nozdor Action: use the assigned Toggle key to change mode."])
        elseif command == "status" then
            Sync()
            DEFAULT_CHAT_FRAME:AddMessage("Nozdor Action " .. nativeVersion .. ": " .. stateText)
            DEFAULT_CHAT_FRAME:AddMessage(L["Last active sample: "] .. targetDiagnostic)
        else A.OpenSettings() end
    end
    DEFAULT_CHAT_FRAME:AddMessage(L["Nozdor Action: /nfa for settings. Mode starts OFF; F6 toggles if unassigned."])
    Sync()
end

driver:RegisterEvent("ADDON_LOADED")
driver:RegisterEvent("PLAYER_LEAVING_WORLD")
driver:RegisterEvent("PLAYER_LOGOUT")
driver:RegisterEvent("PLAYER_ENTERING_WORLD")
driver:RegisterEvent("UPDATE_BINDINGS")
driver:RegisterEvent("PLAYER_REGEN_ENABLED")
driver:SetScript("OnEvent", function(self, event, name)
    if event == "ADDON_LOADED" and name == ADDON then LoadSettings()
    elseif event == "PLAYER_LEAVING_WORLD" or event == "PLAYER_LOGOUT" then
        worldReady = false; Sync()
    elseif event == "PLAYER_ENTERING_WORLD" then worldReady = true; Sync()
    elseif event == "UPDATE_BINDINGS" or event == "PLAYER_REGEN_ENABLED" then RefreshBindings()
    end
end)
driver:SetScript("OnUpdate", function(self, dt)
    SyncCursorUI()
    elapsed = elapsed + dt
    if elapsed < 0.1 then return end
    elapsed = 0
    Sync()
end)
