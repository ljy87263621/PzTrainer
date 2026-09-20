local action, payload = ...
local main = MainScreen and MainScreen.instance
local function visible(ui)
    if not ui or not ui.javaObject or not ui:isReallyVisible() then return false end
    local root = ui.javaObject
    while root:getParent() do root = root:getParent() end
    return UIManager.getUI():contains(root)
end
assert(main and main.desc, "Open the game's character creation screen first")
local owner = main
if CoopCharacterCreation and visible(CoopCharacterCreation.instance) then owner = CoopCharacterCreation.instance end
local prof, appearance, spawn = owner.charCreationProfession, owner.charCreationMain, owner.mapSpawnSelect
assert(visible(prof) or visible(appearance) or visible(spawn), "Character creation is not active")
local desc = main.desc
local function clean(value) return tostring(value or ""):gsub("[\t\r\n]", " ") end
local function id(row) return row.item:getType():getName() end
local function find(list, value)
    for index, row in ipairs(list and list.items or {}) do
        if id(row) == value then return row.item, index end
    end
end
local function requireProfession() assert(visible(prof), "Open the profession / traits screen for this operation") end
local function requireAppearance() assert(visible(appearance), "Open the appearance screen for this operation") end
local function selectCombo(combo, value)
    assert(combo, "This control is unavailable")
    for i, text in ipairs(combo.options) do
        if clean(text) == value then combo.selected = i; return end
    end
    error("Selection changed; refresh the character editor")
end

if action == "profession" then
    requireProfession()
    local definition, index = find(prof.listboxProf, payload)
    assert(definition, "Profession is not available")
    prof.listboxProf.selected = index
    prof:onSelectProf(definition)
elseif action == "trait_add" then
    requireProfession()
    local trait = find(prof.listboxTrait, payload) or find(prof.listboxBadTrait, payload)
    assert(trait and not prof:isTraitExcluded(trait), "Trait is unavailable or mutually exclusive")
    prof:addTrait(trait)
    prof:checkXPBoost()
elseif action == "trait_remove" then
    requireProfession()
    local trait, index = find(prof.listboxTraitSelected, payload)
    assert(trait and not trait:isFree(), "Profession-granted traits cannot be removed separately")
    prof:removeTrait(index)
    prof:checkXPBoost()
elseif action == "points" then
    requireProfession()
    local points = tonumber(payload)
    assert(points and points >= 0 and points <= 1000, "Points must be between 0 and 1000")
    prof.pointToSpend = prof.pointToSpend + math.floor(points) - prof:PointToSpend()
elseif action == "name" then
    requireAppearance()
    local first, last = payload:match("^([^\t]*)\t([^\t]*)$")
    assert(first and #first > 0 and #last > 0 and #first <= 128 and #last <= 128, "Enter a first and last name")
    desc:setForename(first); desc:setSurname(last)
    appearance.forenameEntry:setText(first); appearance.surnameEntry:setText(last)
elseif action == "gender" then
    requireAppearance()
    assert(payload == "1" or payload == "2", "Invalid gender")
    appearance.genderCombo.selected = tonumber(payload)
    appearance:onGenderSelected(appearance.genderCombo)
elseif action == "hair" or action == "beard" or action == "voice" then
    requireAppearance()
    local combos = {hair = appearance.hairTypeCombo, beard = appearance.beardTypeCombo, voice = appearance.voiceTypeCombo}
    local combo = combos[action]
    selectCombo(combo, payload)
    if action == "hair" then appearance:onHairTypeSelected(combo)
    elseif action == "beard" then appearance:onBeardTypeSelected(combo)
    else appearance:onVoiceTypeSelected() end
elseif action == "hair_color" then
    requireAppearance()
    local r, g, b = payload:match("^([%d.]+),([%d.]+),([%d.]+)$")
    r, g, b = tonumber(r), tonumber(g), tonumber(b)
    assert(r and g and b and r <= 1 and g <= 1 and b <= 1, "Invalid hair color")
    local color = ImmutableColor.new(r, g, b)
    local visual = desc:getHumanVisual()
    visual:setHairColor(color); visual:setNaturalHairColor(color)
    visual:setBeardColor(color); visual:setNaturalBeardColor(color)
    appearance.avatarPanel:setSurvivorDesc(desc)
elseif action == "clothing" or action == "clothing_texture" then
    requireAppearance()
    local slot, option = payload:match("^([^|]+)|(.+)$")
    assert(slot and option, "Invalid clothing selection")
    local combos = action == "clothing" and appearance.clothingCombo or appearance.clothingTextureCombo
    local combo = combos and combos[slot]
    selectCombo(combo, option)
    if action == "clothing" then appearance:onClothingComboSelected(combo, slot)
    else appearance:onClothingTextureComboSelected(combo, slot) end
elseif action == "voice_pitch" then
    requireAppearance()
    local pitch = tonumber(payload)
    assert(pitch and pitch >= -100 and pitch <= 100, "Voice pitch must be between -100 and 100")
    appearance.voicePitchSlider:setCurrentValue(pitch, true)
    appearance:onVoiceTypeSelected()
elseif action == "skin" then
    requireAppearance()
    local value = tonumber(payload)
    assert(value and value >= 1 and value <= 5, "Skin index must be between 1 and 5")
    desc:getHumanVisual():setSkinTextureIndex(math.floor(value) - 1)
    appearance:syncUIWithTorso()
    appearance.avatarPanel:setSurvivorDesc(desc)
elseif action == "spawn" then
    assert(visible(spawn) and spawn.listbox, "Open the spawn selection screen")
    local matched = false
    for index, row in ipairs(spawn.listbox.items) do
        local region = row.item and row.item.region
        if region and (region.key or region.name) == payload then
            spawn.listbox.selected, spawn.selectedRegion = index, region
            matched = true
            break
        end
    end
    assert(matched, "Spawn region is not available")
elseif action == "save_profession" then
    requireProfession(); prof:saveBuildStep1()
elseif action == "load_profession" then
    requireProfession(); selectCombo(prof.savedBuilds, payload); prof:loadBuild(prof.savedBuilds)
elseif action == "save_appearance" then
    requireAppearance(); appearance:saveBuildStep1()
elseif action == "load_appearance" then
    requireAppearance(); selectCombo(appearance.savedBuilds, payload); appearance:loadOutfit(appearance.savedBuilds)
elseif action == "world" then
    assert(not isClient(), "World parameters are controlled by the server")
    assert(visible(spawn) and main.createWorld and spawn.textEntry, "World parameters can only be changed on a new world's spawn screen")
    local name, seed = payload:match("^([^\t]*)\t([^\t]*)$")
    assert(name and name ~= "" and sanitizeWorldName(name) == name and name:sub(1, 1) ~= "." and name:sub(-1) ~= ".", "Invalid world name")
    assert(not checkSaveFolderExists(getWorld():getGameMode() .. getFileSeparator() .. name), "World save name already exists")
    spawn.textEntry:setText(name)
    if spawn.seedPanel and spawn.seedPanel.seedTextBox then spawn.seedPanel.seedTextBox:setText(seed) end
elseif action ~= "refresh" then error("Unknown character creation operation") end

local rows = {}
local function row(kind, key, label) rows[#rows + 1] = kind .. "\t" .. clean(key) .. "\t" .. clean(label) end
row("stage", "", visible(prof) and "职业与特质" or visible(appearance) and "角色外观" or "出生地点")
row("first", "", desc:getForename()); row("last", "", desc:getSurname())
if prof and prof.listboxProf then
    for _, entry in ipairs(prof.listboxProf.items) do row("profession", id(entry), entry.text) end
    for _, list in ipairs({prof.listboxTrait, prof.listboxBadTrait}) do
        for _, entry in ipairs(list.items or {}) do row("trait_add", id(entry), entry.text .. " (" .. entry.item:getCost() .. ")") end
    end
    for _, entry in ipairs(prof.listboxTraitSelected.items or {}) do row("trait_remove", id(entry), entry.text) end
    row("points", "", prof:PointToSpend())
    for _, text in ipairs(prof.savedBuilds.options) do row("load_profession", clean(text), text) end
end
if appearance then
    for _, pair in ipairs({{"hair", appearance.hairTypeCombo}, {"beard", appearance.beardTypeCombo}, {"voice", appearance.voiceTypeCombo}, {"load_appearance", appearance.savedBuilds}}) do
        if pair[2] then for _, text in ipairs(pair[2].options) do row(pair[1], clean(text), text) end end
    end
    for _, pair in ipairs({{"clothing", appearance.clothingCombo}, {"clothing_texture", appearance.clothingTextureCombo}}) do
        for slot, combo in pairs(pair[2] or {}) do
            if combo:isVisible() then
                for _, text in ipairs(combo.options) do row(pair[1], clean(slot) .. "|" .. clean(text), tostring(slot) .. " / " .. clean(text)) end
            end
        end
    end
end
if spawn and spawn.listbox then
    for _, entry in ipairs(spawn.listbox.items) do
        local region = entry.item and entry.item.region
        if region then row("spawn", region.key or region.name, entry.text) end
    end
end
return table.concat(rows, "\n"), appearance, desc
