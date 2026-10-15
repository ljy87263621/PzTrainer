import pztrainer.player.visual.NativeModelCapture;
public class NativeModelCaptureTest {
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
        System.out.println("Native capture: explicit ownership, nesting and exception cleanup passed");
    }
}
