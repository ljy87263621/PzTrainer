package pztrainer.player.items;

import java.util.ArrayList;
import java.util.Comparator;
import zombie.inventory.InventoryItem;
import zombie.inventory.InventoryItemFactory;
import zombie.radio.ZomboidRadio;
import zombie.radio.media.MediaData;
import zombie.radio.media.RecordedMedia;

/** Registered programmes, rather than empty VHS/CD script-item shells. */
public final class RecordedMediaItems {
    private static final String PREFIX="RecordedMedia:";
    private static final String[] CATEGORIES={"Retail-VHS","Home-VHS","CDs"};
    private static String baseType(String category) {
        if("Retail-VHS".equals(category)) return "Base.VHS_Retail";
        if("Home-VHS".equals(category)) return "Base.VHS_Home";
        if("CDs".equals(category)) return "Base.Disc_Retail";
        return null;
    }
    private static boolean playable(MediaData data) {
        return data!=null && data.getIndex()>=0 && data.getLineCount()>0 && baseType(data.getCategory())!=null;
    }
    private static ArrayList<MediaData> recordings(RecordedMedia registry,String category) {
        ArrayList<MediaData> result=new ArrayList<>();
        var all=registry.getAllMediaForCategory(category);
        if(all!=null) for(MediaData data:all) if(playable(data)) result.add(data);
        result.sort(Comparator.comparing(MediaData::getId));
        return result;
    }
    private static String text(String value) { return value==null ? "" : value; }
    /** Flat rows: unique key, display, category, search aliases, actual script type. */
    public static String[] catalog() {
        RecordedMedia registry=ZomboidRadio.getInstance().getRecordedMedia();
        ArrayList<String> result=new ArrayList<>();
        if(registry==null) return new String[0];
        for(String category:CATEGORIES) for(MediaData data:recordings(registry,category)) {
            String title=text(data.getTranslatedItemDisplayName());
            if(title.isEmpty()) title=text(data.getTranslatedTitle());
            if(title.isEmpty()) title=data.getId();
            result.add(PREFIX+data.getId());
            result.add(title);
            result.add("Entertainment");
            result.add(category+" "+baseType(category)+" "+data.getId()+" "+
                text(data.getTranslatedTitle())+" "+text(data.getTitleEN())+" "+
                text(data.getTranslatedSubTitle())+" "+text(data.getSubtitleEN())+" "+
                text(data.getTranslatedAuthor())+" "+text(data.getAuthorEN())+" "+
                (category.equals("CDs") ? "CD 光盘 唱片 音乐" : "VHS 录像带 录影带 录象带 电视 节目"));
            result.add(baseType(category));
        }
        return result.toArray(new String[0]);
    }
    /** Called through the existing game-main-thread queue before inventory/ground insertion. */
    public static InventoryItem create(String key) {
        RecordedMedia registry=ZomboidRadio.getInstance().getRecordedMedia();
        if(registry==null || key==null) throw new IllegalArgumentException("Recorded media registry unavailable");
        MediaData data=null;
        if(key.startsWith(PREFIX)) {
            data=registry.getMediaData(key.substring(PREFIX.length()));
        } else {
            for(String category:CATEGORIES) if(baseType(category).equals(key)) {
                var available=recordings(registry,category);
                if(!available.isEmpty()) data=available.get(zombie.core.random.Rand.Next(available.size()));
                break;
            }
        }
        if(!playable(data)) throw new IllegalArgumentException("Playable recording not found: "+key);
        InventoryItem item=InventoryItemFactory.CreateItem(baseType(data.getCategory()));
        if(item==null) throw new IllegalArgumentException("Recorded media script type unavailable");
        item.setRecordedMediaData(data);
        if(item.getMediaData()!=data || item.getRecordedMediaIndex()!=data.getIndex() ||
                item.getMediaType()!=data.getMediaType())
            throw new IllegalStateException("Recorded media binding failed: "+key);
        return item;
    }
}
