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
    public static void run() throws Exception {
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
        System.out.println("Actual Viewpoint ModelPass/ModelBatches with a controlled mesh: colour, transparency, reused-scenery isolation and disabled-group framebuffer checks passed.");
    }
}
