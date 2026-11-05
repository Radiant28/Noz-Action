local file = assert(io.open(ADDON_FILE, "rb"))
local source = file:read("*a"):gsub("\r\n", "\n"); file:close()
assert(load(source, "RefinedBlizzPlates syntax"))
local classSource = assert(source:match("(local function UpdateClassColor%(Virtual%).-)\nlocal function GetAggroStatus"))
local tickSource = assert(source:match("(local function PlatesSecUpdate%(%).-)\nlocal function RefreshGroupVisuals"))
local colors = {PALADIN = {r = 0.96, g = 0.55, b = 0.73}, PRIEST = {r = 1, g = 1, b = 1}}
local env = setmetatable({
    unpack = table.unpack, RAID_CLASS_COLORS = colors, NAMEPLATE_CLASS_COLORS = {},
    ClassByFriendName = {PartyMember = "PRIEST"},
    RBP = {dbp = {healthBar_friendClassColor = false, healthBar_friendColor = {0, 1, 0},
        nameText_classColorFriends = true, nameText_classColorEnemies = true, nameText_color = {1, 1, 1}}},
    NozdorAction = {GetNameplateClass = function(plate) return plate.class end},
}, {__index = _G})
local update = assert(load(classSource .. "\nreturn UpdateClassColor", "RBP installed class resolver", "t", env))()
local plate = {class = "PALADIN"}
local virtual = {RealPlate = plate, RBP_classKey = "FRIENDLY_PLAYER", RBP_nameString = "OutsideGroup",
    RBP_nameText = {SetTextColor = function() end}, RBP_isBarlessPlate = true,
    RBP_reaction = 5, RBP_healthBar = {GetStatusBarColor = function() return 0, 0, 1 end}}
update(virtual)
assert(virtual.RBP_classColor == colors.PALADIN, "Ungrouped friendly player must use exact native class")
virtual.RBP_nameString = "PartyMember" -- A same-named member cannot override the actual plate class.
update(virtual); assert(virtual.RBP_classColor == colors.PALADIN)
plate.class = nil; update(virtual)
assert(virtual.RBP_classColor == colors.PRIEST, "Keep group fallback without native identity")
virtual.RBP_classKey = nil; update(virtual)
assert(not virtual.RBP_classColor, "Do not apply player colors to friendly NPCs")
virtual.RBP_classKey, virtual.RBP_nameString = "FRIENDLY_PLAYER", "OutsideGroup"
env.PlatesVisible = {[plate] = virtual}
env.ReactionByPlateColor = function() return 5 end
env.UpdateClassColor = update
local barlessUpdates = 0
env.UpdateBarlessPlate = function(v)
    barlessUpdates = barlessUpdates + 1
    assert(v == virtual and v.RBP_classColor == colors.PALADIN)
end
local tick = assert(load(tickSource .. "\nreturn PlatesSecUpdate", "RBP installed refresh", "t", env))()
tick(); assert(barlessUpdates == 0, "No bridge result yet")
plate.class = "PALADIN"; tick()
assert(barlessUpdates == 1, "Refresh an already visible barless plate when the DLL becomes available")
tick(); assert(barlessUpdates == 1, "Stable colors do not rebuild the barless plate")
env.NozdorAction = nil; virtual.RBP_nameString = "PartyMember"; update(virtual)
assert(virtual.RBP_classColor == colors.PRIEST, "Addon still works without Nozdor Action")
print("PASS: installed RefinedBlizzPlates exact class resolution, group fallback and delayed barless refresh")
