import pztrainer.player.visual.NativeModelCapture;
public class NativeModelCaptureTest {
    // Headless character: only the animation lookup is replaced; render-data
    // construction, lighting allocation and reference release are real game code.
    public static class CaptureZombie extends zombie.characters.IsoGameCharacter {
        zombie.core.skinnedmodel.animation.AnimationPlayer animation;
        public CaptureZombie() {super(null,0,0,0);}
        @Override public zombie.core.skinnedmodel.animation.AnimationPlayer getAnimationPlayer() {return animation;}
        @Override public float getAlpha(int index) {return 1f;}
    }
    public static void main(String[] args) throws Exception {
        if(NativeModelCapture.currentIdentity()!=0)throw new AssertionError("Unowned draw has an identity");
        NativeModelCapture.capture(42,()-> {
            if(NativeModelCapture.currentIdentity()!=42)throw new AssertionError("Scoped owner missing");
            NativeModelCapture.capture(7,()-> {
                if(NativeModelCapture.currentIdentity()!=7)throw new AssertionError("Nested owner missing");
            });
            if(NativeModelCapture.currentIdentity()!=42)throw new AssertionError("Outer owner lost");
        });
        try {NativeModelCapture.capture(91,()->{throw new IllegalStateException("fixture");});}
        catch(IllegalStateException expected) {}
        if(NativeModelCapture.currentIdentity()!=0)throw new AssertionError("Owner leaked to subsequent player draw");
        var unsafeField=Class.forName("sun.misc.Unsafe").getDeclaredField("theUnsafe");unsafeField.setAccessible(true);
        Object unsafe=unsafeField.get(null);
        var allocate=unsafe.getClass().getMethod("allocateInstance",Class.class);
        var zombie=(zombie.characters.IsoGameCharacter)allocate.invoke(unsafe,zombie.characters.IsoZombie.class);
        var player=(zombie.characters.IsoPlayer)allocate.invoke(unsafe,zombie.characters.IsoPlayer.class);
        var groups=new java.util.HashMap<Integer,Integer>();groups.put(System.identityHashCode(zombie),0);
        groups.put(System.identityHashCode(player),0); // Deliberately stale/wrong group.
        if(NativeModelCapture.selectedGroup(zombie,player,groups)!=0)
            throw new AssertionError("Loaded unseen zombie was culled before explicit capture");
        if(NativeModelCapture.selectedGroup(player,player,groups)!=-1)
            throw new AssertionError("Local player was selected for a zombie capture");
        var model=new zombie.core.skinnedmodel.model.ModelInstance(); model.object=zombie;model.character=zombie;
        var slot=new zombie.core.skinnedmodel.ModelManager.ModelSlot(0,model,zombie);slot.active=true;
        if(!NativeModelCapture.ownsSlot(zombie,slot))throw new AssertionError("Bone ESP owner lost model slot");
        if(NativeModelCapture.ownsSlot(player,slot))throw new AssertionError("Reused slot painted another character");
        slot.character=null;
        if(NativeModelCapture.ownsSlot(zombie,slot))throw new AssertionError("Released slot was accepted");
        slot.character=zombie;model.object=player;
        if(NativeModelCapture.ownsSlot(zombie,slot))throw new AssertionError("Stale model object was accepted");
        // The game's actual postRender consumes one slot reference, even with no model geometry.
        var data=new zombie.core.skinnedmodel.model.ModelSlotRenderData();data.modelSlot=slot;
        slot.renderRefCount=1;data.postRender();
        if(slot.renderRefCount!=0)throw new AssertionError("Game slot release protocol changed");
        model.object=zombie;slot.character=zombie;
        model.playerData=new zombie.core.skinnedmodel.model.ModelInstance.PlayerData[]{new zombie.core.skinnedmodel.model.ModelInstance.PlayerData()};
        var prepare=NativeModelCapture.class.getDeclaredMethod("prepare",zombie.core.skinnedmodel.ModelManager.ModelSlot.class);
        prepare.setAccessible(true);
        try {((zombie.core.skinnedmodel.model.ModelSlotRenderData)prepare.invoke(null,slot)).postRender();}
        catch(java.lang.reflect.InvocationTargetException expected) {
            if(!(expected.getCause() instanceof RuntimeException))throw expected;
        }
        if(slot.renderRefCount!=0)throw new AssertionError("Failed initialization leaked or underflowed the slot reference");
        var actor=(CaptureZombie)allocate.invoke(unsafe,CaptureZombie.class);
        actor.animation=(zombie.core.skinnedmodel.animation.AnimationPlayer)allocate.invoke(unsafe,zombie.core.skinnedmodel.animation.AnimationPlayer.class);
        var asset=(zombie.core.skinnedmodel.model.Model)allocate.invoke(unsafe,zombie.core.skinnedmodel.model.Model.class);
        asset.effect=(zombie.core.skinnedmodel.shader.Shader)allocate.invoke(unsafe,zombie.core.skinnedmodel.shader.Shader.class);
        var privateType=Class.forName("zombie.asset.Asset$PRIVATE");
        var constructor=privateType.getDeclaredConstructor(zombie.asset.Asset.class);constructor.setAccessible(true);
        var assetState=zombie.asset.Asset.class.getDeclaredField("priv");assetState.setAccessible(true);
        assetState.set(asset,constructor.newInstance(asset)); // Unloaded mesh: no GL needed for frame construction.
        model=new zombie.core.skinnedmodel.model.ModelInstance();model.object=actor;model.character=actor;model.model=asset;
        slot=new zombie.core.skinnedmodel.ModelManager.ModelSlot(0,model,actor);slot.active=true;
        if(model.playerData!=null)throw new AssertionError("Fixture must start with unseen/uninitialized model data");
        for(int index:new int[]{0,1,0}) {
            var frameState=Class.forName("zombie.iso.IsoCamera").getField("frameState").get(null);
            frameState.getClass().getField("playerIndex").setInt(frameState,index);
            var prepared=(zombie.core.skinnedmodel.model.ModelSlotRenderData)prepare.invoke(null,slot);
            if(model.playerData==null || model.playerData[index]==null)
                throw new AssertionError("Explicit capture did not initialize engine per-player lighting");
            if(prepared.character!=actor || prepared.animPlayer!=actor.animation || prepared.alpha!=1f || slot.renderRefCount!=1)
                throw new AssertionError("Incomplete native capture frame");
            prepared.postRender();
            if(slot.renderRefCount!=0)throw new AssertionError("Successful capture leaked a model slot reference");
        }
        System.out.println("Native capture: explicit ownership, nesting and exception cleanup passed");
    }
}
