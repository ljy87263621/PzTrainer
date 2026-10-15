import java.util.List;
import pztrainer.player.visual.ViewpointChamsTransforms;
public class ViewpointFrameHookTest {
    private static native String install(String jar);
    private static Class<?> api(String name) throws Exception{return Class.forName("java.lang.classfile."+name);}
    public static void main(String[] args) throws Exception {
        if(args.length==2) {
            System.load(args[0]);String error=install(args[1]);
            if(!error.isEmpty())throw new AssertionError("Production JNI/JVMTI frame hook failed: "+error);
            System.out.println("Production C++ JNI/JVMTI: installed live ViewPoint frame callback.");
        }
        pztrainer.player.visual.ViewpointModelChams.publish(new float[48],new float[48],new float[48],new int[12],
            new int[0],new int[0],new int[0],1,1);
        pztrainer.player.visual.ViewpointModelChams.renderPublished(); // No GL context: must never escape into the game.
        if(pztrainer.player.visual.ViewpointModelChams.error.isEmpty())throw new AssertionError("Missing safe frame callback failure diagnostic");
        pztrainer.player.visual.ViewpointModelChams.clearPublished();
        byte[] original;
        try(var input=ClassLoader.getSystemResourceAsStream("viewpoint/render/WorldRenderer.class")) {original=input.readAllBytes();}
        byte[] transformed=ViewpointChamsTransforms.transform(original);
        Object format=api("ClassFile").getMethod("of").invoke(null);
        Object model=api("ClassFile").getMethod("parse",byte[].class).invoke(format,(Object)transformed);
        int hooks=0;
        for(Object method:(List<?>)api("ClassModel").getMethod("methods").invoke(model)) {
            var code=(java.util.Optional<?>)api("MethodModel").getMethod("code").invoke(method);
            if(code.isEmpty())continue;
            String previous="";
            for(Object element:(Iterable<?>)code.get()) if(api("instruction.InvokeInstruction").isInstance(element)) {
                String name=api("instruction.InvokeInstruction").getMethod("name").invoke(element).toString();
                String owner=(String)api("constantpool.ClassEntry").getMethod("asInternalName").invoke(api("instruction.InvokeInstruction").getMethod("owner").invoke(element));
                if(name.equals("endFrame") && owner.equals("viewpoint/render/ModelPass")) {
                    if(!previous.equals("renderPublished"))throw new AssertionError("Effects are outside the live frame lifetime");
                    hooks++;
                }
                previous=name;
            }
        }
        if(hooks!=1)throw new AssertionError("Missing exact end-of-frame hook");
        try { ViewpointChamsTransforms.transform(new byte[0]);throw new AssertionError("Invalid class accepted"); }
        catch(ReflectiveOperationException|IllegalArgumentException expected) {}
        System.out.println("Installed ViewPoint WorldRenderer: verified frame callback before batch retirement.");
    }
}
