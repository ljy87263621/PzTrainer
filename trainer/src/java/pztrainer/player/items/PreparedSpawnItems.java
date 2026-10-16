package pztrainer.player.items;

import java.util.ArrayList;
import zombie.inventory.InventoryItem;
import zombie.inventory.InventoryItemFactory;
import zombie.inventory.ItemContainer;
import zombie.inventory.types.Moveable;

/** Concrete item creation shared by state-carrying spawn routes. */
public final class PreparedSpawnItems {
    private static final String MOVEABLE_PREFIX="Moveables.";
    private PreparedSpawnItems() {}

    private static boolean media(String key) {
        return key.startsWith("RecordedMedia:") || key.equals("Base.VHS_Retail") ||
            key.equals("Base.VHS_Home") || key.equals("Base.Disc_Retail");
    }
    public static InventoryItem create(String key) {
        if(key==null || key.isEmpty())throw new IllegalArgumentException("Empty item key");
        if(media(key))return RecordedMediaItems.create(key);
        InventoryItem item=InventoryItemFactory.CreateItem(key);
        if(item==null)throw new IllegalArgumentException("Unknown item: "+key);
        if(key.startsWith(MOVEABLE_PREFIX) && !key.equals("Moveables.Moveable")) {
            String sprite=key.substring(MOVEABLE_PREFIX.length());
            if(sprite.isEmpty() || sprite.indexOf('.')>=0 || !(item instanceof Moveable moveable) ||
                !moveable.ReadFromWorldSprite(sprite) || !sprite.equals(moveable.getWorldSprite()))
                throw new IllegalArgumentException("Invalid furniture sprite: "+key);
        }
        return item;
    }
    public static String factoryType(InventoryItem item) {
        if(item instanceof Moveable moveable && moveable.getWorldSprite()!=null && !moveable.getWorldSprite().isEmpty())
            return MOVEABLE_PREFIX+moveable.getWorldSprite();
        return item.getFullType();
    }
    /** Invoke through the game-main-thread queue; string AddItems loses prepared state. */
    public static ArrayList<InventoryItem> addToContainer(ItemContainer container,String key,Integer quantity) {
        if(container==null || quantity==null || quantity<1 || quantity>100)
            throw new IllegalArgumentException("Invalid prepared item request");
        ArrayList<InventoryItem> items=new ArrayList<>();
        for(int i=0;i<quantity;++i)items.add(create(key));
        try {
            for(InventoryItem item:items)
                if(container.AddItem(item)!=item || item.getContainer()!=container)
                    throw new IllegalStateException("Prepared item ownership failed");
        } catch(RuntimeException | LinkageError failure) {
            for(InventoryItem item:items)if(item.getContainer()==container)container.DoRemoveItem(item);
            throw failure;
        }
        return items;
    }
    public static int count(ItemContainer container,String key) {
        if(container==null || key==null)return -1;
        if(!media(key) && !key.startsWith(MOVEABLE_PREFIX))return container.getItemCount(key);
        int count=0;
        for(InventoryItem item:container.getItems()) {
            if(key.startsWith("RecordedMedia:")) {
                if(item.getMediaData()!=null && item.getMediaData().getId().equals(key.substring(14)))++count;
            } else if(key.startsWith(MOVEABLE_PREFIX) && !key.equals("Moveables.Moveable")) {
                if(item instanceof Moveable moveable && key.substring(MOVEABLE_PREFIX.length()).equals(moveable.getWorldSprite()))++count;
            } else if(item.getFullType().equals(key) && (!media(key) || item.isRecordedMedia()))++count;
        }
        return count;
    }
}
