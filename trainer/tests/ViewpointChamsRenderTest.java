import java.lang.reflect.Array;
import java.lang.reflect.Field;
import java.lang.reflect.Proxy;
import java.nio.ByteBuffer;
import org.lwjgl.BufferUtils;
import org.lwjgl.opengl.GL11;
import org.lwjgl.opengl.GL15;
import org.lwjgl.opengl.GL20;
import org.lwjgl.opengl.GL30;
import pztrainer.player.visual.ViewpointModelChams;
import pztrainer.player.visual.ModelMaterial;

public class ViewpointChamsRenderTest {
    private static Field field(Class<?> type, String name) throws Exception {
        Field field = type.getDeclaredField(name); field.setAccessible(true); return field;
    }
    private static Object construct(Class<?> type) throws Exception {
        var constructor = type.getDeclaredConstructor(); constructor.setAccessible(true); return constructor.newInstance();
    }
    private static int shader(int type, String source) {
        int shader = GL20.glCreateShader(type); GL20.glShaderSource(shader, source); GL20.glCompileShader(shader);
        if (GL20.glGetShaderi(shader, GL20.GL_COMPILE_STATUS) == 0) throw new AssertionError(GL20.glGetShaderInfoLog(shader));
        return shader;
    }
    private static String modShader(String name) throws Exception {
        Class<?> shaders=Class.forName("viewpoint.platform.Shaders");
        var load=shaders.getDeclaredMethod("load",String.class); load.setAccessible(true);
        Object source=load.invoke(null,name);
        return (String)field(source.getClass(),"text").get(source);
    }
    private static void actualShaderCompatibility() throws Exception {
        int program=GL20.glCreateProgram();
        int vs=shader(GL20.GL_VERTEX_SHADER,modShader("model.vert"));
        int fs=shader(GL20.GL_FRAGMENT_SHADER,modShader("model_outline.frag"));
        try {
            GL20.glAttachShader(program,vs); GL20.glAttachShader(program,fs); GL20.glLinkProgram(program);
            if (GL20.glGetProgrami(program,GL20.GL_LINK_STATUS)==0) throw new AssertionError(GL20.glGetProgramInfoLog(program));
            ModelMaterial.use(program,8);
            if(GL11.glGetInteger(GL20.GL_CURRENT_PROGRAM)==program) throw new AssertionError("Actual mod shader was not cloned");
            ModelMaterial.use(program,9);
            if(GL11.glGetError()!=GL11.GL_NO_ERROR) throw new AssertionError("Actual ViewPoint vertex/fragment uniforms are incompatible");
        } finally { GL20.glUseProgram(0); GL20.glDeleteProgram(program); GL20.glDeleteShader(vs); GL20.glDeleteShader(fs); }
    }
    public static void run() throws Exception {
        actualShaderCompatibility();
        Class<?> passType = Class.forName("viewpoint.render.ModelPass");
        Class<?> unsafeType = Class.forName("sun.misc.Unsafe");
        Object unsafe = field(unsafeType, "theUnsafe").get(null);
        Object pass = unsafeType.getMethod("allocateInstance", Class.class).invoke(unsafe, passType);
        Class<?> dataType = Class.forName("viewpoint.render.ModelDraws");
        Object data = construct(dataType);
        field(dataType, "count").setInt(data, 1);
        float[] markers = new float[48], colours = new float[48];
        for (int group = 0; group < 12; group++) {
            markers[group * 4] = 0.05f * (group + 1);
            markers[group * 4 + 1] = 0.9f - 0.03f * group;
            markers[group * 4 + 2] = 0.1f + 0.02f * group;
        }
        dataType.getMethod("outline", int.class, float.class, float.class, float.class, float.class, boolean.class)
            .invoke(data, 0, markers[0], markers[1], markers[2], 0f, false);
        colours[0] = 1f; colours[3] = 0.6f;
        field(passType, "draws").set(pass, data);

        int vao = GL30.glGenVertexArrays(), vertex = GL15.glGenBuffers(), indices = GL15.glGenBuffers();
        GL30.glBindVertexArray(vao); GL15.glBindBuffer(GL15.GL_ARRAY_BUFFER, vertex);
        GL15.glBufferData(GL15.GL_ARRAY_BUFFER, new float[]{-1, -1, 0, 1, -1, 0, 0, 1, 0}, GL15.GL_STATIC_DRAW);
        GL20.glVertexAttribPointer(0, 3, GL11.GL_FLOAT, false, 12, 0L); GL20.glEnableVertexAttribArray(0);
        GL15.glBindBuffer(GL15.GL_ELEMENT_ARRAY_BUFFER, indices);
        GL15.glBufferData(GL15.GL_ELEMENT_ARRAY_BUFFER, new int[]{0, 1, 2}, GL15.GL_STATIC_DRAW);
        Class<?> layoutType = Class.forName("viewpoint.render.ModelMeshes$Layout");
        var layoutConstructor = layoutType.getDeclaredConstructor(int.class, int[].class, int.class);
        layoutConstructor.setAccessible(true);
        Object layout = layoutConstructor.newInstance(12, new int[]{0}, 0);
        field(layoutType, "vao").setInt(layout, vao);
        Class<?> heldType = Class.forName("viewpoint.render.ModelMeshes$Held");
        Object held = construct(heldType);
        field(heldType, "layout").set(held, layout); field(heldType, "elements").setInt(held, 3);
        Object heldArray = Array.newInstance(heldType, 1); Array.set(heldArray, 0, held);
        Class<?> batchType = Class.forName("viewpoint.render.ModelBatches");
        Object batch = construct(batchType);
        field(batchType, "draws").set(batch, data); field(batchType, "held").set(batch, heldArray);
        field(batchType, "textures").set(batch, Array.newInstance(Class.forName("zombie.core.textures.Texture"), 1));
        field(batchType, "count").setInt(batch, 1);
        field(passType, "batches").set(pass, batch);
        Class<?> stateType = Class.forName("viewpoint.render.ModelBatches$State");
        field(passType, "runs").set(pass, Proxy.newProxyInstance(stateType.getClassLoader(), new Class<?>[]{stateType},
            (proxy, called, arguments) -> null));

        String vertexSource = "#version 330\nlayout(location=0) in vec3 aPos; uniform mat4 uViewProjection; void main(){gl_Position=uViewProjection*vec4(aPos,1);}";
        String fragmentSource = "#version 330\nuniform vec3 uColour; out vec4 fragColor; void main(){fragColor=vec4(uColour,1);}";
        int program = GL20.glCreateProgram();
        GL20.glAttachShader(program, shader(GL20.GL_VERTEX_SHADER, vertexSource));
        GL20.glAttachShader(program, shader(GL20.GL_FRAGMENT_SHADER, fragmentSource)); GL20.glLinkProgram(program);
        if (GL20.glGetProgrami(program, GL20.GL_LINK_STATUS) == 0) throw new AssertionError(GL20.glGetProgramInfoLog(program));
        Class<?> programType = Class.forName("viewpoint.platform.GlProgram");
        var programConstructor = programType.getDeclaredConstructor(String.class, String.class, String.class);
        programConstructor.setAccessible(true);
        Object outline = programConstructor.newInstance("chams test", "", "");
        field(programType, "id").setInt(outline, program); field(programType, "source").set(outline, vertexSource + fragmentSource);
        field(passType, "outline").set(pass, outline);
        Field models = field(Class.forName("viewpoint.render.WorldRenderer"), "models"); models.set(null, pass);

        GL20.glUseProgram(0); GL30.glBindVertexArray(0);
        GL11.glDisable(GL11.GL_SCISSOR_TEST); GL11.glClearColor(0, 0, 0, 1); GL11.glClear(GL11.GL_COLOR_BUFFER_BIT);
        int painted = ViewpointModelChams.render(markers, colours, 64, 64);
        if (painted != 1) throw new AssertionError("Actual Viewpoint batch was not drawn: " + ViewpointModelChams.error);
        ByteBuffer pixel = BufferUtils.createByteBuffer(4);
        GL11.glReadPixels(32, 32, 1, 1, GL11.GL_RGBA, GL11.GL_UNSIGNED_BYTE, pixel);
        if ((pixel.get(0) & 255) < 150 || (pixel.get(0) & 255) > 156 || pixel.get(1) != 0 || pixel.get(2) != 0)
            throw new AssertionError("Configured colour/opacity missing from actual framebuffer: " + (pixel.get(0) & 255));
        if (GL11.glGetError() != GL11.GL_NO_ERROR) throw new AssertionError("OpenGL error in Viewpoint model rendering");
        independentLayers(pass, data, markers, colours, pixel);
        GL11.glEnable(GL11.GL_ALPHA_TEST);
        GL11.glAlphaFunc(GL11.GL_GREATER, 0.99f);
        GL11.glClear(GL11.GL_COLOR_BUFFER_BIT);
        if (ViewpointModelChams.render(markers, colours, 64, 64) != 1)
            throw new AssertionError("Inherited alpha-test replay did not submit the model");
        GL11.glReadPixels(32,32,1,1,GL11.GL_RGBA,GL11.GL_UNSIGNED_BYTE,pixel);
        if ((pixel.get(0)&255)<150)
            throw new AssertionError("Game alpha test suppressed the final material despite a successful draw count");
        if (!GL11.glIsEnabled(GL11.GL_ALPHA_TEST))
            throw new AssertionError("Game alpha test was not restored");
        GL11.glDisable(GL11.GL_ALPHA_TEST);
        GL11.glEnable(GL11.GL_STENCIL_TEST); GL11.glStencilFunc(GL11.GL_NEVER,0,255);
        GL11.glClear(GL11.GL_COLOR_BUFFER_BIT);
        ViewpointModelChams.render(markers,colours,64,64);
        GL11.glReadPixels(32,32,1,1,GL11.GL_RGBA,GL11.GL_UNSIGNED_BYTE,pixel);
        if ((pixel.get(0)&255)<150) throw new AssertionError("Game stencil test suppressed model composition");
        if (!GL11.glIsEnabled(GL11.GL_STENCIL_TEST)) throw new AssertionError("Game stencil test was not restored");
        GL11.glDisable(GL11.GL_STENCIL_TEST);
        int[] effects = new int[12]; effects[0] = 8;
        GL11.glClear(GL11.GL_COLOR_BUFFER_BIT);
        if (ViewpointModelChams.render(markers, colours, effects, 64, 64) != 1)
            throw new AssertionError("Thermal character material was not drawn: " + ViewpointModelChams.error);
        ByteBuffer cool = BufferUtils.createByteBuffer(4), warm = BufferUtils.createByteBuffer(4);
        GL11.glReadPixels(32, 16, 1, 1, GL11.GL_RGBA, GL11.GL_UNSIGNED_BYTE, cool);
        GL11.glReadPixels(32, 48, 1, 1, GL11.GL_RGBA, GL11.GL_UNSIGNED_BYTE, warm);
        if ((warm.get(1)&255) <= (cool.get(1)&255) + 20)
            throw new AssertionError("Thermal material must vary over the actual model surface");
        effects[0] = 9; GL11.glClear(GL11.GL_COLOR_BUFFER_BIT);
        if (ViewpointModelChams.render(markers, colours, effects, 64, 64) != 1)
            throw new AssertionError("White-hot material was not drawn: " + ViewpointModelChams.error);
        GL11.glReadPixels(32, 48, 1, 1, GL11.GL_RGBA, GL11.GL_UNSIGNED_BYTE, warm);
        if (warm.get(0)!=warm.get(1) || warm.get(1)!=warm.get(2)) throw new AssertionError("White-hot must be greyscale");
        if (GL11.glGetInteger(GL20.GL_CURRENT_PROGRAM)!=0 || GL11.glGetError()!=GL11.GL_NO_ERROR)
            throw new AssertionError("Thermal shader binding leaked into the game");
        GL11.glClear(GL11.GL_COLOR_BUFFER_BIT);
        if (ViewpointModelChams.render(markers, colours, 64, 64)!=1) throw new AssertionError("Switching back to solid failed");
        GL11.glReadPixels(32,32,1,1,GL11.GL_RGBA,GL11.GL_UNSIGNED_BYTE,pixel);
        if (pixel.get(1)!=0 || pixel.get(2)!=0) throw new AssertionError("Thermal output leaked into solid material");
        float[] reusedRecords = (float[]) field(dataType, "values").get(data);
        reusedRecords[35] = 0f;
        GL11.glClear(GL11.GL_COLOR_BUFFER_BIT);
        if (ViewpointModelChams.render(markers, colours, 64, 64) != 0) throw new AssertionError("Reused scenery was drawn with stale character RGB");
        GL11.glReadPixels(32, 32, 1, 1, GL11.GL_RGBA, GL11.GL_UNSIGNED_BYTE, pixel);
        if (pixel.get(0) != 0 || pixel.get(1) != 0 || pixel.get(2) != 0) throw new AssertionError("Scenery framebuffer received character colour");
        reusedRecords[35] = 0.001f;
        colours[3] = 0f;
        if (ViewpointModelChams.render(markers, colours, 64, 64) != 0) throw new AssertionError("Disabled group was drawn");
        models.set(null, null);
        System.out.println("ViewPoint: actual installed shaders compile; controlled ModelPass/ModelBatches framebuffer verifies thermal gradient, white-hot, switching, opacity, scenery isolation and state restoration.");
    }

    private static void independentLayers(Object pass, Object data, float[] markers, float[] colours, ByteBuffer pixel) throws Exception {
        var render = ViewpointModelChams.class.getMethod("render", float[].class, float[].class, float[].class,
            int[].class, int[].class, int[].class, int.class, int.class);
        Class<?> unsafeType = Class.forName("sun.misc.Unsafe");
        Object unsafe = field(unsafeType,"theUnsafe").get(null);
        var owner = (zombie.iso.IsoMovingObject)unsafeType.getMethod("allocateInstance",Class.class)
            .invoke(unsafe,zombie.characters.IsoZombie.class);
        var instance = new zombie.core.skinnedmodel.model.ModelInstance(); instance.object=owner;
        var instances = (zombie.core.skinnedmodel.model.ModelInstance[])field(data.getClass(),"instances").get(data);
        instances[0]=instance;
        float[] records=(float[])field(data.getClass(),"values").get(data);
        records[35]=0; // First-person/cached capture without an outline marker.
        int[] identities={System.identityHashCode(owner)}, groups={0}, effects=new int[12];
        float[] edges=new float[48]; edges[1]=1; edges[3]=1;
        GL11.glClear(GL11.GL_COLOR_BUFFER_BIT);
        int painted=(Integer)render.invoke(null,markers,colours,edges,effects,identities,groups,64,64);
        if(painted<1)throw new AssertionError("Outline-only unmarked model was not drawn: "+ViewpointModelChams.error);
        GL11.glReadPixels(32,32,1,1,GL11.GL_RGBA,GL11.GL_UNSIGNED_BYTE,pixel);
        if(pixel.get(0)!=0 || pixel.get(1)!=0 || pixel.get(2)!=0)throw new AssertionError("Outline-only mode painted the model interior");
        GL11.glReadPixels(49,32,1,1,GL11.GL_RGBA,GL11.GL_UNSIGNED_BYTE,pixel);
        if((pixel.get(1)&255)<20 || pixel.get(0)!=0)throw new AssertionError("Independent edge glow did not appear outside the silhouette");
        effects[0]=2; edges[3]=0; GL11.glClear(GL11.GL_COLOR_BUFFER_BIT);
        if((Integer)render.invoke(null,markers,colours,edges,effects,identities,groups,64,64)<1)
            throw new AssertionError("Fill-only first-person model was not drawn");
        GL11.glReadPixels(32,32,1,1,GL11.GL_RGBA,GL11.GL_UNSIGNED_BYTE,pixel);
        if((pixel.get(0)&255)<150 || pixel.get(1)!=0)throw new AssertionError("Fill-only colour missing");
        GL11.glReadPixels(49,32,1,1,GL11.GL_RGBA,GL11.GL_UNSIGNED_BYTE,pixel);
        if(pixel.get(0)!=0 || pixel.get(1)!=0)throw new AssertionError("Fill-only mode leaked outside the model");
        edges[3]=1; GL11.glClear(GL11.GL_COLOR_BUFFER_BIT);
        render.invoke(null,markers,colours,edges,effects,identities,groups,64,64);
        GL11.glReadPixels(32,32,1,1,GL11.GL_RGBA,GL11.GL_UNSIGNED_BYTE,pixel);
        if((pixel.get(0)&255)<150 || pixel.get(1)!=0)throw new AssertionError("Combined mode changed interior fill");
        var filteredRender=ViewpointModelChams.class.getMethod("render",float[].class,float[].class,float[].class,
            int[].class,int[].class,int[].class,int[].class,int.class,int.class);
        GL11.glClear(GL11.GL_COLOR_BUFFER_BIT);
        filteredRender.invoke(null,markers,colours,edges,effects,identities,groups,new int[]{2},64,64);
        GL11.glReadPixels(32,32,1,1,GL11.GL_RGBA,GL11.GL_UNSIGNED_BYTE,pixel);
        if(pixel.get(0)!=0 || pixel.get(1)!=0)throw new AssertionError("Fill visibility leaked into independently enabled rim");
        GL11.glReadPixels(49,32,1,1,GL11.GL_RGBA,GL11.GL_UNSIGNED_BYTE,pixel);
        if((pixel.get(1)&255)<20)throw new AssertionError("Fill visibility incorrectly suppressed rim");
        GL11.glClear(GL11.GL_COLOR_BUFFER_BIT);
        if((Integer)filteredRender.invoke(null,markers,colours,edges,effects,identities,groups,new int[]{0},64,64)!=0)
            throw new AssertionError("Fully hidden owner was still submitted");
        Object context=field(Class.forName("viewpoint.render.WorldRenderer"),"frame").get(null);
        var projection=(org.joml.Matrix4f)field(context.getClass(),"plainViewProjection").get(context);
        projection.setPerspective((float)Math.PI/2,1,0.1f,100).translate(0,0,-2);
        GL11.glClear(GL11.GL_COLOR_BUFFER_BIT);
        if((Integer)render.invoke(null,markers,colours,edges,effects,identities,groups,64,64)<1)
            throw new AssertionError("Perspective first-person replay failed");
        GL11.glReadPixels(32,32,1,1,GL11.GL_RGBA,GL11.GL_UNSIGNED_BYTE,pixel);
        if((pixel.get(0)&255)<150)throw new AssertionError("Perspective material missing from framebuffer");
        projection.identity();
        int[] flags=(int[])field(data.getClass(),"flags").get(data);
        flags[0]=data.getClass().getField("SELF").getInt(null);
        if((Integer)render.invoke(null,markers,colours,edges,effects,identities,groups,64,64)!=0)
            throw new AssertionError("First-person self model must not obstruct the camera with colour");
        flags[0]=0;
        var warpField=field(context.getClass(),"panini"); Object originalWarp=warpField.get(context);
        Class<?> warpType=Class.forName("viewpoint.render.Panini");
        Object warp=warpType.getMethod("ofView",float.class,float.class,float.class).invoke(null,(float)Math.PI/2,1f,1f);
        warpField.set(context,warp); edges[3]=0; GL11.glClear(GL11.GL_COLOR_BUFFER_BIT);
        render.invoke(null,markers,colours,edges,effects,identities,groups,64,64);
        ByteBuffer frame=BufferUtils.createByteBuffer(64*64*4);
        GL11.glReadPixels(0,0,64,64,GL11.GL_RGBA,GL11.GL_UNSIGNED_BYTE,frame);
        var toFrame=warpType.getMethod("toFrame",float.class,float.class,float[].class);
        int checked=0;
        for(int y=4;y<60;y+=4)for(int x=4;x<60;x+=4) {
            float[] point=new float[2]; toFrame.invoke(warp,(x+0.5f)/32-1,(y+0.5f)/32-1,point);
            float margin=1-2*Math.abs(point[0])-point[1];
            if(Math.abs(margin)<0.08f || Math.abs(point[0])>0.95f || Math.abs(point[1])>0.95f)continue;
            boolean expected=margin>0;
            if(((frame.get((y*64+x)*4)&255)>100)!=expected)
                throw new AssertionError("Model composite differs from actual Panini camera mapping at "+x+","+y);
            checked++;
        }
        if(checked<20)throw new AssertionError("Insufficient Panini framebuffer coverage");
        warpField.set(context,originalWarp);
        GL11.glClear(GL11.GL_COLOR_BUFFER_BIT);
        if((Integer)render.invoke(null,markers,colours,edges,effects,new int[0],new int[0],64,64)!=0)
            throw new AssertionError("Untracked player/scenery received character colour");
        instances[0]=null; records[35]=0.001f;
        if(GL11.glGetInteger(GL20.GL_CURRENT_PROGRAM)!=0 || GL11.glGetError()!=GL11.GL_NO_ERROR)
            throw new AssertionError("Independent material layers leaked GL state");
        System.out.println("ViewPoint: unoutlined owner identity, perspective/Panini cameras, fill-only, edge-only, combined layers, self and untracked exclusion passed.");
    }
}
