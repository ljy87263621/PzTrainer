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
        System.out.println("Native capture: explicit ownership, nesting and exception cleanup passed");
    }
}
