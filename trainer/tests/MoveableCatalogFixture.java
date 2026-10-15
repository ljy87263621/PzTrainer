import java.util.ArrayList;
import java.util.HashMap;
import zombie.core.TilePropertyAliasMap;
import zombie.iso.sprite.IsoSpriteManager;
import zombie.inventory.InventoryItemFactory;
import zombie.inventory.types.Moveable;
import zombie.network.GameServer;
import zombie.scripting.ScriptManager;
import zombie.scripting.objects.ScriptModule;
import zombie.scripting.objects.Item;
import zombie.scripting.objects.ItemType;

public final class MoveableCatalogFixture {
    public static void prepare() throws Exception {
        zombie.core.random.RandStandard.INSTANCE.init();
        zombie.core.random.RandLua.INSTANCE.init();
        GameServer.server=true;
        var mod=ScriptManager.class.getDeclaredField("currentLoadFileMod");mod.setAccessible(true);mod.set(null,"pz-vanilla");
        ScriptModule module=new ScriptModule(); module.name="Base";
        Item definition=new Item(); definition.setModule(module); definition.InitLoadPP("Moveable");
        definition.setItemType(ItemType.MOVEABLE); definition.displayName="Moveable Object";
        definition.moduleDotType="Base.Moveable";
        module.items.getScriptMap().put("Moveable",definition);
        ScriptManager.instance.moduleMap.put("Base",module);
        HashMap<String,ArrayList<String>> properties=new HashMap<>();
        properties.put("IsMoveAble",new ArrayList<>());
        properties.put("CustomName",new ArrayList<>(java.util.List.of("Cubicle","Chair")));
        properties.put("GroupName",new ArrayList<>(java.util.List.of("Type A Grey")));
        properties.put("Facing",new ArrayList<>(java.util.List.of("E")));
        properties.put("PickUpWeight",new ArrayList<>(java.util.List.of("30")));
        TilePropertyAliasMap.instance.Generate(properties);
        IsoSpriteManager.instance.namedMap.clear();
        var divider=IsoSpriteManager.instance.AddSprite("location_business_office_generic_01_18");
        divider.getProperties().set("IsMoveAble",""); divider.getProperties().set("CustomName","Cubicle");
        divider.getProperties().set("GroupName","Type A Grey"); divider.getProperties().set("Facing","E");
        divider.getProperties().set("PickUpWeight","30");
        var chair=IsoSpriteManager.instance.AddSprite("furniture_seating_01_0");
        chair.getProperties().set("IsMoveAble","");chair.getProperties().set("CustomName","Chair");
        IsoSpriteManager.instance.AddSprite("not_moveable_wall_0");
        var item=InventoryItemFactory.CreateItem("Moveables.location_business_office_generic_01_18");
        if (!(item instanceof Moveable) || !((Moveable)item).ReadFromWorldSprite("location_business_office_generic_01_18"))
            throw new AssertionError("Actual engine factory could not create divider");
        var inventory=new zombie.inventory.ItemContainer();
        if (inventory.AddItem("Moveables.location_business_office_generic_01_18")!=null)
            throw new AssertionError("Fixture should reproduce the ordinary string-lookup omission");
        var created=new java.beans.Expression(InventoryItemFactory.class,"CreateItem",new Object[]{"Moveables.location_business_office_generic_01_18"}).getValue();
        var added=(zombie.inventory.InventoryItem)new java.beans.Expression(inventory,"AddItem",new Object[]{created}).getValue();
        if (!(added instanceof Moveable) || added.getContainer()!=inventory)
            throw new AssertionError("Native AddItem path did not place the furniture into the inventory");
        System.out.println("Factory divider: "+item.getDisplayName()+"; world sprite="+((Moveable)item).getWorldSprite());
    }
    public static void loadActualTiles(String file) {
        IsoSpriteManager.instance.namedMap.clear();
        zombie.iso.IsoWorld.PropertyValueMap.clear();
        zombie.iso.IsoWorld.instance.LoadTileDefinitionsPropertyStrings(IsoSpriteManager.instance,file,1);
        TilePropertyAliasMap.instance.Generate(zombie.iso.IsoWorld.PropertyValueMap);
        zombie.iso.IsoWorld.instance.LoadTileDefinitions(IsoSpriteManager.instance,file,1);
        if (IsoSpriteManager.instance.namedMap.isEmpty()) throw new AssertionError("Actual tile data was not loaded");
    }
}
