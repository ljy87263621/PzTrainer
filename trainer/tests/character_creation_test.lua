local count = 0
local function check(value, message) assert(value, message); count = count + 1 end
local client = false
function isClient() return client end
UIManager = {getUI = function() return {contains = function() return true end} end}
local function ui()
    return {shown = true, javaObject = {getParent = function() return nil end},
        isReallyVisible = function(self) return self.shown end}
end
local function trait(name, cost, free)
    return {getType = function() return {getName = function() return name end} end,
        getCost = function() return cost end, isFree = function() return free end}
end
local a, b = trait("base:strong", 6, false), trait("base:free", 0, true)
local profession = trait("base:carpenter", 0, false)
local prof = ui()
prof.listboxProf = {items = {{item = profession, text = "木匠"}}}
prof.listboxTrait = {items = {{item = a, text = "强壮"}}}
prof.listboxBadTrait = {items = {}}
prof.listboxTraitSelected = {items = {{item = b, text = "职业特质"}}}
prof.savedBuilds = {options = {"测试预设"}}
prof.pointToSpend, prof.cost = 0, -4
function prof:PointToSpend() return self.pointToSpend + self.cost end
function prof:onSelectProf(value) self.profession = value end
function prof:isTraitExcluded(value) return self.excluded == value end
function prof:addTrait(value) self.added = value end
function prof:removeTrait(index) self.removed = index end
function prof:checkXPBoost() self.checked = true end
function prof:saveBuildStep1() self.saved = true end
function prof:loadBuild(box) self.loaded = box.selected end
local desc = {first = "旧名", last = "旧姓"}
function desc:getForename() return self.first end
function desc:getSurname() return self.last end
function desc:setForename(value) self.first = value end
function desc:setSurname(value) self.last = value end
MainScreen = {instance = {desc = desc, charCreationProfession = prof}}
local result = editor("refresh", "")
check(result:find("木匠", 1, true), "UTF-8 catalogue")
editor("profession", "base:carpenter")
check(prof.profession == profession and prof.listboxProf.selected == 1, "official profession callback")
editor("trait_add", "base:strong")
check(prof.added == a and prof.checked, "trait callback and XP refresh")
prof.excluded = a
check(not pcall(editor, "trait_add", "base:strong"), "mutual exclusion")
check(not pcall(editor, "trait_remove", "base:free"), "free trait protection")
check(not pcall(editor, "profession", "base:missing"), "stale profession rejection")
editor("points", "100")
check(prof:PointToSpend() == 100, "point total accounts for profession cost")
check(not pcall(editor, "points", "1001"), "point limit")
editor("load_profession", "测试预设")
check(prof.loaded == 1, "original preset load")
prof.shown = false
check(not pcall(editor, "refresh", ""), "inactive creation gate")
local appearance = ui()
appearance.forenameEntry = {setText = function(self, value) self.value = value end}
appearance.surnameEntry = {setText = function(self, value) self.value = value end}
appearance.savedBuilds = {options = {}}
MainScreen.instance.charCreationMain = appearance
editor("name", "玉米\t兄弟")
check(desc.first == "玉米" and desc.last == "兄弟" and appearance.forenameEntry.value == "玉米", "name and UI stay synchronized")
check(not pcall(editor, "trait_add", "base:strong"), "wrong creation stage rejected")
check(not pcall(editor, "name", "\t"), "empty name rejected")
client = true
check(pcall(editor, "refresh", ""), "multiplayer creation refresh allowed")
editor("name", "Online\tPlayer")
check(desc.first == "Online" and desc.last == "Player", "multiplayer appearance editing allowed")
check(not pcall(editor, "world", "Server\t123"), "server world parameters remain protected")
return "Character creation: " .. count .. " checks passed"
