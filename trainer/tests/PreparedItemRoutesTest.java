import java.lang.reflect.Method;
import java.nio.ByteBuffer;
import java.util.List;
import java.util.Map;
import zombie.GameWindow;
import zombie.core.network.ByteBufferWriter;
import zombie.inventory.InventoryItem;
import zombie.inventory.InventoryItemFactory;
import zombie.inventory.ItemContainer;
import zombie.inventory.types.Moveable;
import zombie.radio.devices.DeviceData;
import zombie.scripting.ScriptManager;

public final class PreparedItemRoutesTest {
    private static ClassLoader helperLoader=PreparedItemRoutesTest.class.getClassLoader();
    private static int checks;
    private static void check(boolean value,String message) {
        ++checks; if(!value)throw new AssertionError(message);
    }
    private static Object call(String name,Class<?>[] types,Object... args) throws Exception {
        return Class.forName("pztrainer.player.items.PreparedSpawnItems",true,helperLoader).getMethod(name,types).invoke(null,args);
    }
    private static InventoryItem create(String key) throws Exception {
        return (InventoryItem)call("create",new Class<?>[]{String.class},key);
    }
    @SuppressWarnings("unchecked")
    private static List<InventoryItem> add(ItemContainer container,String key,int count) throws Exception {
        return (List<InventoryItem>)call("addToContainer",new Class<?>[]{ItemContainer.class,String.class,Integer.class},container,key,count);
    }
    @SuppressWarnings("unchecked")
    private static void prepareDictionary() throws Exception {
        var field=zombie.world.WorldDictionary.class.getDeclaredField("data");field.setAccessible(true);
        Object dictionary=field.get(null);
        if(dictionary==null) {
            var ctor=Class.forName("zombie.world.DictionaryData").getDeclaredConstructor();ctor.setAccessible(true);
            dictionary=ctor.newInstance();field.set(null,dictionary);
        }
        var byId=dictionary.getClass().getDeclaredField("itemIdToInfoMap");byId.setAccessible(true);
        var byType=dictionary.getClass().getDeclaredField("itemTypeToInfoMap");byType.setAccessible(true);
        short id=1;
        for(var script:ScriptManager.instance.moduleMap.get("Base").items.getScriptMap().values()) {
            script.setRegistry_id(id);
            var info=new zombie.world.ItemInfo();
            for(var pair:Map.of("fullType",script.getFullName(),"registryId",(Object)id,"moduleName","Base","name",script.getName(),"isLoaded",true,"entityScript",script).entrySet()) {
                var member=zombie.world.DictionaryInfo.class.getDeclaredField(pair.getKey());member.setAccessible(true);member.set(info,pair.getValue());
            }
            var member=zombie.world.ItemInfo.class.getDeclaredField("scriptItem");member.setAccessible(true);member.set(info,script);
            ((Map<Short,zombie.world.ItemInfo>)byId.get(dictionary)).put(id,info);
            ((Map<String,zombie.world.ItemInfo>)byType.get(dictionary)).put(script.getFullName(),info);
            ++id;
        }
    }
    public static void prepare() throws Exception {
        RecordedMediaItemsTest.prepare();
        var mediaModule=ScriptManager.instance.moduleMap.get("Base");
        MoveableCatalogFixture.prepare();
        ScriptManager.instance.moduleMap.get("Base").items.getScriptMap().putAll(mediaModule.items.getScriptMap());
        prepareDictionary();
    }
    public static void main(String[] args) throws Exception {
        if(args.length==2) {
            var preparedLoader=new java.net.URLClassLoader(new java.net.URL[]{java.nio.file.Path.of(args[0]).toUri().toURL()},helperLoader);
            helperLoader=new java.net.URLClassLoader(new java.net.URL[]{java.nio.file.Path.of(args[1]).toUri().toURL()},preparedLoader);
        }
        prepare();
        String furniture="Moveables.location_business_office_generic_01_18";
        ItemContainer container=new ItemContainer();
        check(container.AddItems(furniture,2).isEmpty(),"Legacy string AddItems drops furniture");
        for(String key:List.of(furniture,"RecordedMedia:retail-test","RecordedMedia:home-test","RecordedMedia:cd-test","Base.VHS_Retail")) {
            var items=add(container,key,3);
            check(items.size()==3,"Exact prepared quantity");
            check(items.get(0)!=items.get(1),"Distinct instances");
            for(InventoryItem item:items) {
                check(item.getContainer()==container,"Prepared object ownership");
                if(key.equals(furniture))check(item instanceof Moveable && ((Moveable)item).getWorldSprite().equals(key.substring("Moveables.".length())),"Exact furniture sprite");
                else {
                    check(item.isRecordedMedia() && item.getMediaData()!=null && item.getMediaData().getLineCount()>0,"No blank media");
                    if(key.startsWith("RecordedMedia:"))check(item.getMediaData().getId().equals(key.substring(14)),"Exact programme");
                }
                ByteBuffer saved=ByteBuffer.allocate(65536);item.saveWithSize(saved,false);saved.flip();
                InventoryItem restored=InventoryItem.loadItem(saved,zombie.iso.IsoWorld.getWorldVersion());
                check(restored!=null,"Engine item serialization restores object");
                if(item instanceof Moveable original)
                    check(restored instanceof Moveable && original.getWorldSprite().equals(((Moveable)restored).getWorldSprite()),"Serialized furniture preserves sprite/orientation");
                else check(restored.getMediaData()!=null && restored.getMediaData().getId().equals(item.getMediaData().getId()),"Serialized media preserves programme");
            }
            int before=container.getItems().size();
            for(String invalid:List.of("RecordedMedia:missing","RecordedMedia:empty-test","Moveables.missing_sprite")) {
                try {add(container,invalid,2);throw new AssertionError("Invalid prepared key accepted");}
                catch(java.lang.reflect.InvocationTargetException expected) {check(container.getItems().size()==before,"Failure leaves container unchanged");}
            }
        }
        check((Integer)call("count",new Class<?>[]{ItemContainer.class,String.class},container,"RecordedMedia:retail-test")==6,"Count selected programme including generic selection");
        check((Integer)call("count",new Class<?>[]{ItemContainer.class,String.class},container,"RecordedMedia:home-test")==3,"Do not count other programmes");
        check((Integer)call("count",new Class<?>[]{ItemContainer.class,String.class},container,furniture)==3,"Count dynamic sprite");
        for(int count:new int[]{0,101}) {
            int before=container.getItems().size();
            try {add(container,furniture,count);throw new AssertionError("Invalid quantity accepted");}
            catch(java.lang.reflect.InvocationTargetException expected) {check(container.getItems().size()==before,"Invalid quantity is atomic");}
        }
        Method write=Class.forName("pztrainer.items.MediaItemPayload",true,helperLoader).getDeclaredMethod("writeTarget",ByteBufferWriter.class,InventoryItem.class);
        write.setAccessible(true);
        for(String key:List.of("RecordedMedia:retail-test","RecordedMedia:home-test","RecordedMedia:cd-test",furniture)) {
            InventoryItem target=create(key);
            ByteBuffer packet=ByteBuffer.allocate(1024);
            write.invoke(null,new ByteBufferWriter(packet),target);
            packet.flip();short index=packet.getShort();check(packet.get()!=0,"Media target present");
            String type=GameWindow.ReadString(packet);
            System.out.println("Media removal target: "+key+" -> "+type+" / "+index);
            check(type.equals(key.equals(furniture)?furniture:target.getFullType()) && !type.startsWith("RecordedMedia:"),"Packet carries reconstructable engine item type");
            check(index==(target.isRecordedMedia()?target.getRecordedMediaIndex():Short.MAX_VALUE),"Exact index, sentinel only for non-media");
            DeviceData device=new DeviceData();
            var field=DeviceData.class.getDeclaredField("mediaItem");field.setAccessible(true);field.set(device,type);
            device.setMediaIndex(index);
            zombie.network.GameServer.server=false;
            ItemContainer result=new ItemContainer();device.removeMediaItem(result);
            check(result.getItems().size()==1,"Engine media removal creates one concrete item");
            InventoryItem returned=result.getItems().get(0);
            if(target.isRecordedMedia())check(returned.getMediaData()!=null && returned.getMediaData().getId().equals(target.getMediaData().getId()),"Engine removal preserves programme");
            else check(returned instanceof Moveable && ((Moveable)returned).getWorldSprite().equals(((Moveable)target).getWorldSprite()),"Engine removal preserves furniture");
        }
        Class<?> unsafeType=Class.forName("sun.misc.Unsafe");
        var unsafeField=unsafeType.getDeclaredField("theUnsafe");unsafeField.setAccessible(true);
        Object unsafe=unsafeField.get(null);
        var player=(zombie.characters.IsoPlayer)unsafeType.getMethod("allocateInstance",Class.class).invoke(unsafe,zombie.characters.IsoPlayer.class);
        var inventoryField=zombie.characters.IsoGameCharacter.class.getDeclaredField("inventory");inventoryField.setAccessible(true);
        ItemContainer lightInventory=new ItemContainer();inventoryField.set(player,lightInventory);
        var light=new zombie.iso.objects.IsoLightSwitch((zombie.iso.IsoCell)null);
        light.lights.add(new zombie.iso.IsoLightSource(0,0,0,1,1,1,10));
        light.setBulbItemRaw(furniture);
        InventoryItem lightResult=light.removeLightBulb(player);
        check(lightResult instanceof Moveable && ((Moveable)lightResult).getWorldSprite().equals(furniture.substring("Moveables.".length())),"Engine light removal preserves furniture sprite");
        check(lightResult.getContainer()==lightInventory,"Engine light removal inserts actual item");
        var lightHelper=Class.forName("pztrainer.items.LightItemPayload",true,helperLoader);
        int lightCount=(Integer)lightHelper.getMethod("countInventory",zombie.characters.IsoPlayer.class,String.class).invoke(null,player,furniture);
        check(lightCount==1,"Light completion counts sprite rather than the aliased full type");
        System.out.println("Prepared item route checks passed: "+checks);
    }
}
