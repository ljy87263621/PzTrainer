package pztrainer.player.visual;

import java.lang.constant.ClassDesc;
import java.lang.constant.MethodTypeDesc;
import java.lang.reflect.InvocationHandler;
import java.lang.reflect.Proxy;
import java.util.List;
import java.util.function.Predicate;

/** Submit models while the game is populating its world frame, before outline finalization. */
public final class NativeCaptureTransforms {
    private static Class<?> api(String name) throws ClassNotFoundException {
        return Class.forName("java.lang.classfile."+name);
    }
    public static byte[] transform(byte[] bytes) throws Exception {
        Object format=api("ClassFile").getMethod("of").invoke(null);
        Object model=api("ClassFile").getMethod("parse",byte[].class).invoke(format,(Object)bytes);
        Object owner=api("ClassModel").getMethod("thisClass").invoke(model);
        String ownerName=(String)api("constantpool.ClassEntry").getMethod("asInternalName").invoke(owner);
        if(!ownerName.equals("zombie/gameStates/IngameState"))throw new IllegalArgumentException("Wrong native frame class");
        Predicate<Object> match=method->{
            try {
                Object name=api("MethodModel").getMethod("methodName").invoke(method);
                Object type=api("MethodModel").getMethod("methodType").invoke(method);
                var equals=api("constantpool.Utf8Entry").getMethod("equalsString",String.class);
                return (boolean)equals.invoke(name,"renderFrameInternal") && (boolean)equals.invoke(type,"(I)V");
            } catch(ReflectiveOperationException failure) {throw new IllegalStateException(failure);}
        };
        if(((List<?>)api("ClassModel").getMethod("methods").invoke(model)).stream().filter(match).count()!=1)
            throw new IllegalArgumentException("IngameState renderFrameInternal signature changed");
        var with=api("ClassFileBuilder").getMethod("with",api("ClassFileElement"));
        var invoke=api("CodeBuilder").getMethod("invokestatic",ClassDesc.class,String.class,MethodTypeDesc.class);
        Class<?> calls=api("instruction.InvokeInstruction"); int[] edits={0};
        Object code=Proxy.newProxyInstance(NativeCaptureTransforms.class.getClassLoader(),new Class<?>[]{api("CodeTransform")},
            (proxy,called,args)->{
                if(called.isDefault())return InvocationHandler.invokeDefault(proxy,called,args);
                if(!called.getName().equals("accept"))throw new UnsupportedOperationException(called.toString());
                Object builder=args[0],element=args[1];
                if(calls.isInstance(element)) {
                    String name=calls.getMethod("name").invoke(element).toString();
                    String target=(String)api("constantpool.ClassEntry").getMethod("asInternalName")
                        .invoke(calls.getMethod("owner").invoke(element));
                    if(target.equals("zombie/core/skinnedmodel/model/ModelOutlines") && name.equals("endFrameMain")) {
                        invoke.invoke(builder,ClassDesc.of("pztrainer.player.visual.NativeModelCapture"),
                            "queuePublished",MethodTypeDesc.ofDescriptor("()V"));edits[0]++;
                    }
                }
                with.invoke(builder,element);return null;
            });
        Object transform=api("ClassTransform").getMethod("transformingMethodBodies",Predicate.class,api("CodeTransform"))
            .invoke(null,match,code);
        byte[] result=(byte[])api("ClassFile").getMethod("transformClass",api("ClassModel"),api("ClassTransform"))
            .invoke(format,model,transform);
        if(edits[0]!=1 || !((List<?>)api("ClassFile").getMethod("verify",byte[].class).invoke(format,(Object)result)).isEmpty())
            throw new VerifyError("Native frame hook did not match exactly once");
        return result;
    }
}
