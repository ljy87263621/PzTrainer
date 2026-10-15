import java.lang.reflect.Field;
import java.util.Arrays;
import zombie.inventory.InventoryItem;
import zombie.inventory.InventoryItemFactory;
import zombie.radio.ZomboidRadio;
import zombie.radio.media.RecordedMedia;
import zombie.scripting.ScriptManager;
import zombie.scripting.objects.Item;
import zombie.scripting.objects.ItemType;
import zombie.scripting.objects.ScriptModule;

public final class RecordedMediaItemsTest {
    private static int checks;
    private static void check(boolean value,String message) {
        ++checks; if(!value) throw new AssertionError(message);
    }
    public static void prepare() throws Exception {
        zombie.core.random.RandStandard.INSTANCE.init();
        zombie.core.random.RandLua.INSTANCE.init();
        zombie.network.GameServer.server=true;
        Field mod=ScriptManager.class.getDeclaredField("currentLoadFileMod");
        mod.setAccessible(true); mod.set(null,"pz-vanilla");
        ScriptModule module=new ScriptModule(); module.name="Base";
        for(String name:Arrays.asList("VHS_Retail","VHS_Home","Disc_Retail")) {
            Item item=new Item(); item.setModule(module); item.InitLoadPP(name);
            item.setItemType(ItemType.NORMAL); item.displayName=name; item.moduleDotType="Base."+name;
            module.items.getScriptMap().put(name,item);
        }
        ScriptManager.instance.moduleMap.put("Base",module);
        ZomboidRadio.getInstance();
        Field media=ZomboidRadio.class.getDeclaredField("recordedMedia"); media.setAccessible(true);
        RecordedMedia registry=new RecordedMedia(); media.set(null,registry);
        var retail=registry.register("Retail-VHS","retail-test","Woodcraft Test",0);
        retail.setTitle("Woodcraft Test"); retail.setSubtitle("Episode 1");
        retail.addLine("Test programme line",1,1,1,"WDW+1");
        var home=registry.register("Home-VHS","home-test","Home recording",0);
        home.addLine("Test home line",1,1,1,null);
        var cd=registry.register("CDs","cd-test","Music Test",0);
        cd.addLine("Test music line",1,1,1,null);
        registry.register("Retail-VHS","empty-test","Empty recording",0);
    }
    private static InventoryItem create(String key) throws Exception {
        return (InventoryItem)Class.forName("pztrainer.player.items.RecordedMediaItems")
            .getMethod("create",String.class).invoke(null,key);
    }
    public static void main(String[] args) throws Exception {
        prepare();
        InventoryItem blank=InventoryItemFactory.CreateItem("Base.VHS_Retail");
        check(blank!=null && blank.getRecordedMediaIndex()==-1 && blank.getMediaData()==null,
            "Fixture must reproduce the blank ordinary factory item");
        if(args.length>0 && args[0].equals("--baseline")) {
            check(blank.isRecordedMedia() && blank.getMediaData().getLineCount()>0,
                "Legacy factory path creates an unplayable VHS without programme metadata");
            return;
        }
        String[] catalog=(String[])Class.forName("pztrainer.player.items.RecordedMediaItems")
            .getMethod("catalog").invoke(null);
        check(catalog.length==15,"Catalog must include exactly three playable recordings");
        check(Arrays.stream(catalog).noneMatch(v->v.equals("RecordedMedia:empty-test")),"No empty records");
        check(Arrays.stream(catalog).anyMatch(v->v.contains("Woodcraft Test")),"Programme title searchable");
        for(String key:Arrays.asList("RecordedMedia:retail-test","RecordedMedia:home-test","RecordedMedia:cd-test",
                "Base.VHS_Retail","Base.VHS_Home","Base.Disc_Retail")) {
            InventoryItem item=create(key);
            check(item!=null && item.isRecordedMedia(),"Playable item: "+key);
            check(item.getMediaData()!=null && item.getMediaData().getLineCount()>0,"Actual lines: "+key);
            check(item.getMediaType()==item.getMediaData().getMediaType(),"Playback media type: "+key);
            check(item.getDisplayName().contains(item.getMediaData().getTranslatedItemDisplayName()),"Display: "+key);
            if(key.startsWith("RecordedMedia:")) check(item.getMediaData().getId().equals(key.substring(14)),"Exact ID");
            var inventory=new zombie.inventory.ItemContainer();
            Object added=new java.beans.Expression(inventory,"AddItem",new Object[]{item}).getValue();
            check(added==item && item.getContainer()==inventory,"Main-thread object insertion preserves instance");
            check(item.getMediaData().getLineCount()>0,"Insertion preserves content");
            check(create(key)!=item,"Quantity creates independent instances");
        }
        for(String invalid:Arrays.asList("RecordedMedia:missing","RecordedMedia:","RecordedMedia:empty-test","Base.Axe")) {
            try { create(invalid); throw new AssertionError("Unexpected creation: "+invalid); }
            catch(java.lang.reflect.InvocationTargetException expected) {
                check(expected.getCause() instanceof IllegalArgumentException,"Explicit invalid media error");
            }
        }
        var empty=new RecordedMedia();
        Field media=ZomboidRadio.class.getDeclaredField("recordedMedia");media.setAccessible(true);media.set(null,empty);
        check(((String[])Class.forName("pztrainer.player.items.RecordedMediaItems").getMethod("catalog").invoke(null)).length==0,
            "Unloaded registry is retryable without cached fake entries");
        try {create("Base.VHS_Retail");throw new AssertionError("No random blank fallback");}
        catch(java.lang.reflect.InvocationTargetException expected) {check(true,"Empty category rejected");}
        System.out.println("Recorded media actual JVM checks passed: "+checks);
    }
}
