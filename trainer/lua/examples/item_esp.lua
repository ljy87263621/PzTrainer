-- PZSA Item ESP
-- The category, navigation icon, settings, world scan and rendering are all
-- declared here. The native bridge only supplies reusable UI/catalog controls.

require "ISUI/ISUIElement"

local icon = PZSA.ui.icon()
icon:rect(-7.0, -6.0, 3.5, 4.5, 2.0, 1.5)
icon:line(3.0, 3.5, 7.0, 7.5, 2.0)
icon:line(-4.5, -2.5, 1.0, -2.5, 1.2)
icon:line(-4.5, 0.0, 0.0, 0.0, 1.2)

local category = PZSA.ui.category("物品 ESP", icon)
category:toggle("启用物品 ESP", false)
category:toggle("自动切换 3D Item ESP", true)
category:toggle("手动启用 3D Item ESP（关闭自动后生效）", false)
category:toggle("显示名称", true)
category:toggle("显示图标", true)
category:toggle("名称仅实际可见", false)
category:toggle("图标仅实际可见", false)
category:toggle("显示地面物品", true)
category:toggle("显示容器物品", true)
category:slider("显示范围（格）", 2, 35, 12)
category:slider("最大显示条目", 20, 300, 120)
category:color("ESP 颜色", 0.25, 0.78, 1.0, 0.95)
category:item_multiselect("物品黑名单")

local canvas = ISUIElement:new(0, 0, 1, 1)
canvas:initialise()
canvas:instantiate()

local cachedEntries = {}
local tick = 0
local offsetCache = {}
local catalogByType = {}
local catalogLoaded = false

local function refreshCatalog()
    catalogByType = {}
    local catalog = PZSA.items.catalog()
    for index = 1, #catalog do
        local entry = catalog[index]
        if entry ~= nil and entry.full_type ~= nil then
            catalogByType[entry.full_type] = entry
            catalogLoaded = true
        end
    end
end

refreshCatalog()

local function getOffsets(radius)
    local cached = offsetCache[radius]
    if cached ~= nil then return cached end

    cached = {}
    for offsetY = -radius, radius do
        for offsetX = -radius, radius do
            table.insert(cached, {
                x = offsetX,
                y = offsetY,
                distanceSquared = offsetX * offsetX + offsetY * offsetY
            })
        end
    end
    table.sort(cached, function(left, right)
        return left.distanceSquared < right.distanceSquared
    end)
    offsetCache[radius] = cached
    return cached
end

local function itemName(item, fullType)
    local name = item:getDisplayName()
    if name ~= nil and name ~= "" then return name end
    local catalogEntry = catalogByType[fullType]
    if catalogEntry ~= nil and catalogEntry.display_name ~= nil then
        return catalogEntry.display_name
    end
    return fullType
end

local function addItemEntry(entries, item, x, y, z, source, blacklist, maximum)
    if item == nil or #entries >= maximum then return end
    local fullType = item:getFullType()
    if fullType == nil or blacklist[fullType] then return end

    table.insert(entries, {
        x = x,
        y = y,
        z = z,
        fullType = fullType,
        name = itemName(item, fullType),
        source = source,
        texture = item:getTex()
    })
end

local function scanSquare(
        entries, square, showGround, showContainers, blacklist, maximum)
    if showGround then
        local worldObjects = square:getWorldObjects()
        for index = 0, worldObjects:size() - 1 do
            local worldObject = worldObjects:get(index)
            if instanceof(worldObject, "IsoWorldInventoryObject") then
                addItemEntry(
                    entries, worldObject:getItem(),
                    worldObject:getWorldPosX(), worldObject:getWorldPosY(),
                    worldObject:getWorldPosZ(), nil, blacklist, maximum)
                if #entries >= maximum then return end
            end
        end
    end

    if not showContainers then return end
    local objects = square:getObjects()
    for objectIndex = 0, objects:size() - 1 do
        local object = objects:get(objectIndex)
        for containerIndex = 0, object:getContainerCount() - 1 do
            local container = object:getContainerByIndex(containerIndex)
            if container ~= nil then
                local containerName = container:getType()
                if containerName == nil or containerName == "" then
                    containerName = "Container"
                end
                local items = container:getItems()
                for itemIndex = 0, items:size() - 1 do
                    addItemEntry(
                        entries, items:get(itemIndex),
                        square:getX() + 0.5, square:getY() + 0.5,
                        square:getZ(), containerName, blacklist, maximum)
                    if #entries >= maximum then return end
                end
            end
        end
    end
end

local function scanWorld(player, range, maximum, showGround, showContainers, blacklist)
    local cell = getCell()
    if cell == nil then return {} end

    local entries = {}
    local playerX = player:getX()
    local playerY = player:getY()
    local playerZ = math.floor(player:getZ())
    local radius = math.ceil(range)
    local rangeSquared = range * range
    local offsets = getOffsets(radius)

    for index = 1, #offsets do
        local offset = offsets[index]
        if offset.distanceSquared <= rangeSquared then
            local square = cell:getGridSquare(
                math.floor(playerX) + offset.x,
                math.floor(playerY) + offset.y,
                playerZ)
            if square ~= nil then
                scanSquare(
                    entries, square, showGround, showContainers,
                    blacklist, maximum)
                if #entries >= maximum then return entries end
            end
        end
    end
    return entries
end

local function onTick()
    tick = tick + 1
    if not catalogLoaded and tick % 300 == 0 then refreshCatalog() end
    local enabled = category:toggle("启用物品 ESP", false)
    if not enabled then
        cachedEntries = {}
        return
    end
    if tick % 30 ~= 0 then return end

    local player = getPlayer()
    if player == nil or player:isDead() then
        cachedEntries = {}
        return
    end

    local range = category:slider("显示范围（格）", 2, 35, 12)
    local maximum = math.floor(
        category:slider("最大显示条目", 20, 300, 120))
    local showGround = category:toggle("显示地面物品", true)
    local showContainers = category:toggle("显示容器物品", true)
    local blacklist = category:item_multiselect("物品黑名单")
    cachedEntries = scanWorld(
        player, range, maximum, showGround, showContainers, blacklist)
end

local function use3DProjection(screenWidth, screenHeight)
    local world = PZSA.world
    local status = nil
    if world ~= nil and world.viewpoint_status ~= nil then
        -- Refresh once at UI draw time, before projecting any cached items.
        status = world.viewpoint_status(screenWidth, screenHeight)
    end
    if not category:toggle("自动切换 3D Item ESP", true) then
        return category:toggle("手动启用 3D Item ESP（关闭自动后生效）", false), true
    end
    if status ~= nil then return status.active == true, status.ready == true end
    -- API 3 DLLs expose detection through to_screen only.
    if world ~= nil and world.to_screen ~= nil then
        local projected = world.to_screen(0, 0, 0)
        return projected ~= nil and projected.active == true, true
    end
    return false, true
end

local function toScreen(entry, use3D, zoom)
    if use3D then
        if PZSA.world == nil or PZSA.world.to_screen == nil then return nil end
        local projected = PZSA.world.to_screen(entry.x, entry.y, entry.z)
        if projected == nil or not projected.ready then return nil end
        return projected.x, projected.y, projected.visible
    end
    if zoom == nil or zoom ~= zoom or zoom <= 0.01 or zoom == math.huge then return nil end
    local screenX = IsoUtils.XToScreen(entry.x, entry.y, entry.z, 0)
    local screenY = IsoUtils.YToScreen(entry.x, entry.y, entry.z, 0)
    -- IsoUtils and camera offsets are in the zoomed world framebuffer space;
    -- OnPostUIDraw uses UI pixels. Scale the whole camera-relative result.
    return (screenX - getCameraOffX()) / zoom,
        (screenY - getCameraOffY()) / zoom, true
end

local function drawEntry(entry, x, y, stackIndex, showNames, showIcons, color)
    local iconSize = showIcons and 18 or 0
    local gap = showIcons and showNames and 5 or 0
    local label = ""
    if showNames then
        label = entry.name
        if entry.source ~= nil then
            label = "[" .. entry.source .. "] " .. label
        end
    end

    local textWidth = 0
    if label ~= "" then
        textWidth = getTextManager():MeasureStringX(UIFont.Small, label)
    end
    local width = math.max(6, iconSize + gap + textWidth + 8)
    local height = math.max(8, iconSize + 4)
    local drawX = x - width * 0.5
    local drawY = y - 22 - stackIndex * (height + 2)

    canvas:drawRect(drawX, drawY, width, height, 0.68, 0.025, 0.03, 0.045)
    canvas:drawRectBorder(
        drawX, drawY, width, height,
        color.a, color.r, color.g, color.b)

    local contentX = drawX + 4
    if showIcons and entry.texture ~= nil then
        canvas:drawTextureScaledAspect(
            entry.texture, contentX, drawY + 2, iconSize, iconSize,
            color.a, 1.0, 1.0, 1.0)
        contentX = contentX + iconSize + gap
    end
    if label ~= "" then
        canvas:drawText(
            label, contentX + 1, drawY + 3,
            0.0, 0.0, 0.0, math.min(1.0, color.a + 0.05), UIFont.Small)
        canvas:drawText(
            label, contentX, drawY + 2,
            color.r, color.g, color.b, color.a, UIFont.Small)
    end
end

local function onPostUIDraw()
    if not category:toggle("启用物品 ESP", false) then return end
    local showNames = category:toggle("显示名称", true)
    local showIcons = category:toggle("显示图标", true)
    local visibleNames = category:toggle("名称仅实际可见", false)
    local visibleIcons = category:toggle("图标仅实际可见", false)
    local color = category:color("ESP 颜色", 0.25, 0.78, 1.0, 0.95)
    local core = getCore()
    local screenWidth = core:getScreenWidth()
    local screenHeight = core:getScreenHeight()
    local use3D, cameraReady = use3DProjection(screenWidth, screenHeight)
    if use3D and not cameraReady then return end
    local zoom = use3D and 1 or core:getZoom(0)
    local screenStacks = {}
    local squareVisibility = {}
    local player = getPlayer()
    local playerIndex = player ~= nil and player.getPlayerNum ~= nil and player:getPlayerNum() or 0
    local cell = (visibleNames or visibleIcons) and getCell() or nil

    for index = 1, #cachedEntries do
        local entry = cachedEntries[index]
        local x, y, visible = toScreen(entry, use3D, zoom)
        if visible and x ~= nil and y ~= nil and
           x >= -100 and x <= screenWidth + 100 and
           y >= -100 and y <= screenHeight + 100 then
            local gameVisible = true
            if visibleNames or visibleIcons then
                local squareKey = math.floor(entry.x) .. ":" .. math.floor(entry.y) .. ":" .. math.floor(entry.z)
                gameVisible = squareVisibility[squareKey]
                if gameVisible == nil then
                    local square = cell ~= nil and cell:getGridSquare(math.floor(entry.x), math.floor(entry.y), math.floor(entry.z)) or nil
                    local ok, value = pcall(function() return square ~= nil and square:isCanSee(playerIndex) end)
                    gameVisible = ok and value == true
                    squareVisibility[squareKey] = gameVisible
                end
            end
            local entryNames = showNames and (not visibleNames or gameVisible)
            local entryIcons = showIcons and (not visibleIcons or gameVisible)
            if entryNames or entryIcons then
            local key = tostring(math.floor(x / 8)) .. ":" ..
                tostring(math.floor(y / 8))
            local stackIndex = screenStacks[key] or 0
            screenStacks[key] = stackIndex + 1
            drawEntry(
                entry, x, y, stackIndex,
                entryNames, entryIcons, color)
            end
        end
    end
end

Events.OnTick.Add(onTick)
Events.OnPostUIDraw.Add(onPostUIDraw)

return function()
    Events.OnTick.Remove(onTick)
    Events.OnPostUIDraw.Remove(onPostUIDraw)
    cachedEntries = {}
end
