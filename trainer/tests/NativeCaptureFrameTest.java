import java.util.List;
import pztrainer.player.visual.NativeModelCapture;

public class NativeCaptureFrameTest {
    private static Class<?> api(String name) throws Exception {return Class.forName("java.lang.classfile."+name);}
    public static void main(String[] args) throws Exception {
        Class<?> transformer;
        try {transformer=Class.forName("pztrainer.player.visual.NativeCaptureTransforms");}
        catch(ClassNotFoundException missing) {throw new AssertionError("Native capture has no world-frame submission hook",missing);}
        byte[] original;
        try(var stream=ClassLoader.getSystemResourceAsStream("zombie/gameStates/IngameState.class")) {original=stream.readAllBytes();}
        byte[] changed=(byte[])transformer.getMethod("transform",byte[].class).invoke(null,(Object)original);
        Object format=api("ClassFile").getMethod("of").invoke(null);
        Object model=api("ClassFile").getMethod("parse",byte[].class).invoke(format,(Object)changed);
        int hooks=0;
        for(Object method:(List<?>)api("ClassModel").getMethod("methods").invoke(model)) {
            var body=(java.util.Optional<?>)api("MethodModel").getMethod("code").invoke(method);
            if(body.isEmpty())continue;
            String previous="",previousOwner="";
            for(Object element:(Iterable<?>)body.get())if(api("instruction.InvokeInstruction").isInstance(element)) {
                String name=api("instruction.InvokeInstruction").getMethod("name").invoke(element).toString();
                String owner=(String)api("constantpool.ClassEntry").getMethod("asInternalName").invoke(api("instruction.InvokeInstruction").getMethod("owner").invoke(element));
                if(owner.equals("zombie/core/skinnedmodel/model/ModelOutlines") && name.equals("endFrameMain")) {
                    if(!previous.equals("queuePublished") || !previousOwner.equals("pztrainer/player/visual/NativeModelCapture"))
                        throw new AssertionError("Native capture submissions are outside the active world frame");
                    hooks++;
                }
                previous=name;previousOwner=owner;
            }
        }
        if(hooks!=1)throw new AssertionError("World frame hook must match exactly once");
        var publish=NativeModelCapture.class.getMethod("publish",int[].class,int[].class,float[].class);
        publish.invoke(null,new int[]{42},new int[]{0},new float[48]);
        NativeModelCapture.class.getMethod("clearPublished").invoke(null);
        NativeModelCapture.class.getMethod("queuePublished").invoke(null); // Empty request must not touch a game world.
        System.out.println("Actual IngameState: explicit capture is submitted inside the world-frame lifetime; selection publish/clear passed.");
    }
}
