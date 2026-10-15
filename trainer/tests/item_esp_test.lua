-- Execute the complete production script with the game's Kahlua interpreter.
local checks = 0
local function check(value, message) assert(value, message); checks = checks + 1 end
local function near(a, b) return math.abs(a - b) < 0.001 end
function require() end
local controls = {}
local category = {}
function category:toggle(label, default)
    if controls[label] == nil then controls[label] = default end
    return controls[label]
end
function category:slider(label, lo, hi, default) return default end
function category:color() return {r=1,g=1,b=1,a=1} end
local blacklist = {}
function category:item_multiselect() return blacklist end
local icon = {rect=function() end, line=function() end}
local status = {detected=false, active=false, ready=false}
local projections, statusReads = {}, 0
local projected = {active=true,ready=true,visible=true,x=450,y=320}
local function statusApi(width, height)
    statusReads = statusReads + 1
    check(width == 1920 and height == 1080, "current UI viewport supplied to camera refresh")
    return status
end
PZSA = {ui={icon=function() return icon end, category=function() return category end},
    items={catalog=function() return {} end}, world={viewpoint_status=statusApi,
    to_screen=function(x,y,z) projections[#projections+1]={x=x,y=y,z=z}; return projected end}}
local rects, texts, icons = {}, 0, 0
local canvas = {initialise=function() end, instantiate=function() end,
    drawRect=function(self,x,y,w,h) rects[#rects+1]={x=x+w/2,y=y+22} end,
    drawRectBorder=function() end, drawTextureScaledAspect=function() icons=icons+1 end, drawText=function() texts=texts+1 end}
ISUIElement = {new=function() return canvas end}
UIFont = {Small=1}
function getTextManager() return {MeasureStringX=function() return 30 end} end
local callbacks = {}
local function event(name) return {Add=function(fn) callbacks[name]=fn end,
    Remove=function(fn) check(callbacks[name]==fn, "cleanup removes correct callback"); callbacks[name]=nil end} end
Events = {OnTick=event("tick"),OnPostUIDraw=event("draw")}
local zoom, offX, offY = 1, -200, 300
local core = {getZoom=function() return zoom end, getScreenWidth=function() return 1920 end,
    getScreenHeight=function() return 1080 end}
function getCore() return core end
function getCameraOffX() return offX end
function getCameraOffY() return offY end
IsoUtils = {XToScreen=function(x,y,z) return (x-y)*64 end,
    YToScreen=function(x,y,z) return (x+y)*32-z*192 end}
local function list(values) return {size=function() return #values end,get=function(self,i) return values[i+1] end} end
local item = {getFullType=function() return "Base.Test" end,getDisplayName=function() return "Test" end,getTex=function() return {} end}
local worldX,worldY,worldZ = 10.5,12.5,0
local world = {getItem=function() return item end,getWorldPosX=function() return worldX end,
    getWorldPosY=function() return worldY end,getWorldPosZ=function() return worldZ end}
function instanceof(obj,name) return obj==world end
local gameVisible = false
local square = {isCanSee=function(self,index) check(index==0,"actual player visibility index used"); return gameVisible end,
    getWorldObjects=function() return list({world}) end,getObjects=function() return list({}) end,
    getX=function() return 10 end,getY=function() return 12 end,getZ=function() return 0 end}
function getCell() return {getGridSquare=function(self,x,y,z) if x==10 and y==12 then return square end end} end
local dead = false
function getPlayer() return {getX=function() return 10 end,getY=function() return 12 end,
    getZ=function() return 0 end,isDead=function() return dead end} end
local cleanup = itemScript()
controls["启用物品 ESP"] = true
local function scan() for i=1,30 do callbacks.tick() end end
local function draw() rects={}; projections={}; texts=0; icons=0; callbacks.draw() end
local function vanillaAnchor()
    draw()
    check(#rects==1,"one item drawn")
    check(near(rects[1].x,((worldX-worldY)*64-offX)/zoom),"vanilla X follows zoom and camera")
    check(near(rects[1].y,((worldX+worldY)*32-worldZ*192-offY)/zoom),"vanilla Y follows zoom and camera")
end
scan()
for _,value in ipairs({1,0.5,1.5,2}) do zoom=value; vanillaAnchor() end
offX,offY = -270,250
vanillaAnchor() -- Camera/zoom changes without rescanning.
worldX,worldY,worldZ = 10.2,12.8,0.1
scan(); vanillaAnchor()
status = {detected=true,active=false,ready=false}
vanillaAnchor() -- Installed mod, vanilla view.
status = {detected=true,active=true,ready=true}
draw(); check(#rects==1 and near(rects[1].x,450) and near(rects[1].y,320),"automatic 3D uses matrix UI coordinates without vanilla zoom")
check(near(projections[1].x,worldX) and near(projections[1].z,worldZ),"3D uses precise ground world position")
controls["名称仅实际可见"]=true;controls["图标仅实际可见"]=true
draw();check(#rects==0,"3D item ESP respects actual game visibility independently of projection clipping")
gameVisible=true;draw();check(#rects==1,"3D visible item restored immediately")
gameVisible=false;controls["名称仅实际可见"]=false;controls["图标仅实际可见"]=false
status.ready=false; draw(); check(#rects==0,"active 3D with unavailable camera does not draw 2D")
status.active=false; vanillaAnchor()
controls["自动切换 3D Item ESP"] = false
controls["手动启用 3D Item ESP（关闭自动后生效）"] = false
status.active=true; status.ready=true
vanillaAnchor()
controls["手动启用 3D Item ESP（关闭自动后生效）"] = true
status.active=false; status.ready=false -- Manual mode is independent of status auto-selection.
draw(); check(#rects==1 and near(rects[1].x,450),"manual 3D calls matrix projection")
projected.ready=false; draw(); check(#rects==0,"manual mode waits for camera")
projected.ready=true; projected.visible=false; draw(); check(#rects==0,"3D rejects clipped point")
projected.visible=true
controls["自动切换 3D Item ESP"] = true
PZSA.world.viewpoint_status=nil
projected.active=true; draw(); check(#rects==1 and near(rects[1].x,450),"old DLL projection detects active 3D")
projected.active=false; vanillaAnchor()
PZSA.world=nil; vanillaAnchor()
zoom=0; draw(); check(#rects==0,"invalid zoom skips drawing")
zoom=1
controls["名称仅实际可见"]=true
draw(); check(#rects==1 and texts==0 and icons==1,"hidden item name filtered independently of icon")
controls["图标仅实际可见"]=true
draw(); check(#rects==0,"hidden item leaves no empty ESP panel when both layers filtered")
gameVisible=true
draw(); check(#rects==1 and texts>0 and icons==1,"visibility change applies without scan cache refresh")
gameVisible=false
controls["名称仅实际可见"]=false
draw(); check(#rects==1 and texts>0 and icons==0,"icon visibility independent of item name")
controls["图标仅实际可见"]=false
blacklist["Base.Test"]=true; scan(); draw(); check(#rects==0,"blacklist respected")
blacklist["Base.Test"]=nil
scan(); controls["启用物品 ESP"]=false; draw(); check(#rects==0,"master switch disables drawing")
controls["启用物品 ESP"]=true; dead=true; scan(); draw(); check(#rects==0,"dead player clears scan")
cleanup()
check(callbacks.tick==nil and callbacks.draw==nil,"no handlers remain after unload")
check(statusReads>0,"native detection used")
return "Item ESP checks passed: " .. checks
