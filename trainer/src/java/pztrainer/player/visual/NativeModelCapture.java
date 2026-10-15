package pztrainer.player.visual;

import java.util.HashMap;
import java.util.HashSet;
import zombie.characters.IsoGameCharacter;
import zombie.characters.IsoPlayer;
import zombie.characters.IsoZombie;
import zombie.characters.animals.IsoAnimal;
import zombie.core.SpriteRenderer;
import zombie.core.skinnedmodel.ModelManager;
import zombie.core.skinnedmodel.model.ModelSlotRenderData;
import zombie.core.textures.ColorInfo;
import zombie.core.textures.TextureDraw;
import zombie.iso.IsoMovingObject;
import zombie.iso.IsoWorld;

/** Explicit model submissions do not depend on the game's visibility culling. */
public final class NativeModelCapture {
    private static final ThreadLocal<Integer> identity=ThreadLocal.withInitial(()->0);
    public static volatile String error="";
    public static int currentIdentity() { return identity.get(); }
    public static void capture(int owner,Runnable draw) {
        int previous=identity.get(); identity.set(owner);
        try { draw.run(); } finally { identity.set(previous); }
    }
    public static int selectedGroup(IsoGameCharacter character,IsoPlayer local,java.util.Map<Integer,Integer> selected) {
        if(character==local)return -1;
        Integer group=selected.get(System.identityHashCode(character));
        if(group==null || group<0 || group>=9)return -1;
        if(group<3 && !(character instanceof IsoZombie))return -1;
        if(group>=3 && group<6 && !(character instanceof IsoPlayer))return -1;
        if(group>=6 && !(character instanceof IsoAnimal))return -1;
        return group;
    }
    public static boolean ownsSlot(IsoGameCharacter character,ModelManager.ModelSlot slot) {
        return character!=null && slot!=null && slot.active && !slot.remove && slot.model!=null &&
            slot.character==character && slot.model.object==character;
    }
    private static ModelSlotRenderData prepare(ModelManager.ModelSlot slot) {
        // Explicit submissions include models skipped by vanilla visibility
        // culling. Let the engine allocate/update the current player's lighting
        // before init reads model.playerData[playerIndex], as ViewPoint does.
        slot.model.updateLights();
        ModelSlotRenderData data=ModelSlotRenderData.alloc();
        data.modelSlot=slot;
        slot.renderRefCount++;
        try {
            // init only fills frame data; initModel establishes character, pose and position first.
            data.initModel(slot);
            data.init(slot);
            data.alpha=1f;
            return data;
        } catch(RuntimeException failure) {
            try { data.postRender(); } catch(RuntimeException cleanup) {failure.addSuppressed(cleanup);}
            throw failure;
        }
    }
    private static boolean enqueue(IsoGameCharacter character,int owner,int group,float[] markers) {
        try {return enqueueReady(character,owner,group,markers);}
        catch(RuntimeException failure) {error=failure.toString();return false;}
    }
    private static boolean enqueueReady(IsoGameCharacter character,int owner,int group,float[] markers) {
        if(character.legsSprite==null || character.legsSprite.modelSlot==null)return false;
        var slot=character.legsSprite.modelSlot;
        if(!ownsSlot(character,slot) || !character.hasActiveModel() || character.getAnimationPlayer()==null)return false;
        ModelSlotRenderData data=prepare(slot);
        ColorInfo colour=new ColorInfo(markers[group*4],markers[group*4+1],markers[group*4+2],0);
        try {SpriteRenderer.instance.drawGeneric(new TextureDraw.GenericDrawer() {
            public void render() {
                try {
                    data.checkReady();
                    if(data.canRender()) capture(owner,()->data.performRenderCharacterOutline(false,colour,false));
                } catch(RuntimeException failure) { error=failure.toString(); }
            }
            public void postRender() { data.postRender(); }
        });} catch(RuntimeException failure) {data.postRender();throw failure;}
        return true;
    }
    /** Called through MainThread's queue; pose/refcount initialization belongs to the game thread. */
    public static int queue(int[] identities,int[] groups,float[] markers) {
        if(identities.length!=groups.length || markers.length!=48)throw new IllegalArgumentException("Invalid capture selection");
        error="";
        if(IsoWorld.instance==null || IsoWorld.instance.getCell()==null || IsoPlayer.getInstance()==null)return 0;
        var cell=IsoWorld.instance.getCell();
        HashMap<Integer,Integer> selected=new HashMap<>();
        for(int i=0;i<identities.length;i++)if(groups[i]>=0 && groups[i]<9)selected.put(identities[i],groups[i]);
        HashSet<IsoMovingObject> objects=new HashSet<>(cell.getObjectList());
        objects.addAll(cell.getZombieList());
        int queued=0;
        // Reserve the local player's pixels before target submissions, even at the capture limit.
        if(!selected.isEmpty())enqueue(IsoPlayer.getInstance(),-1,0,markers);
        for(IsoMovingObject object:objects) {
            if(!(object instanceof IsoGameCharacter character))continue;
            try {
            int owner=System.identityHashCode(object);
            int group=selectedGroup(character,IsoPlayer.getInstance(),selected);
            if(group<0 || character.legsSprite==null)continue;
            if(!character.isAddedToModelManager())ModelManager.instance.Add(character);
            if(character.legsSprite.modelSlot==null)continue;
            if(enqueue(character,owner,group,markers))queued++;
            } catch(RuntimeException failure) {error=failure.toString();}
        }
        return queued;
    }
}
