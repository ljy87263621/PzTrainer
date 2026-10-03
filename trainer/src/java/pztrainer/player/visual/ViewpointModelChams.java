package pztrainer.player.visual;

import java.lang.reflect.Field;
import java.lang.reflect.Method;
import java.lang.reflect.Proxy;
import java.util.Arrays;
import org.lwjgl.opengl.GL11;
import org.lwjgl.opengl.GL13;
import org.lwjgl.opengl.GL14;
import org.lwjgl.opengl.GL15;
import org.lwjgl.opengl.GL20;
import org.lwjgl.opengl.GL30;
import org.lwjgl.opengl.GL31;
import org.lwjgl.opengl.GL33;
import org.lwjgl.opengl.GL40;

public final class ViewpointModelChams {
    private static Api api;
    public static volatile String error = "";

    private static Field field(Class<?> type, String name) throws ReflectiveOperationException {
        Field result = type.getDeclaredField(name);
        result.setAccessible(true);
        return result;
    }
    private static Method method(Class<?> type, String name, Class<?>... parameters) throws ReflectiveOperationException {
        Method result = type.getDeclaredMethod(name, parameters);
        result.setAccessible(true);
        return result;
    }

    private static final class Api {
        final Field models, frame, draws, values, count, outline, batches, runs, projection;
        final Method begin, end, use, matrix, colour, draw;
        final Class<?> filter;
        Api() throws ReflectiveOperationException {
            Class<?> renderer = Class.forName("viewpoint.render.WorldRenderer");
            Class<?> pass = Class.forName("viewpoint.render.ModelPass");
            Class<?> data = Class.forName("viewpoint.render.ModelDraws");
            Class<?> context = Class.forName("viewpoint.render.FrameContext");
            Class<?> program = Class.forName("viewpoint.platform.GlProgram");
            Class<?> batch = Class.forName("viewpoint.render.ModelBatches");
            filter = Class.forName("viewpoint.render.ModelBatches$Filter");
            models = field(renderer, "models"); frame = field(renderer, "frame");
            draws = field(pass, "draws"); values = field(data, "values"); count = field(data, "count");
            outline = field(pass, "outline"); batches = field(pass, "batches"); runs = field(pass, "runs");
            projection = field(context, "plainViewProjection");
            begin = method(pass, "begin"); end = method(pass, "end"); use = method(program, "use");
            matrix = method(program, "set", String.class, Class.forName("org.joml.Matrix4f"));
            colour = method(program, "set", String.class, float.class, float.class, float.class);
            draw = method(batch, "draw", filter, Class.forName("viewpoint.render.ModelBatches$State"));
            if (field(data, "VALUES").getInt(null) != 60 || field(data, "OUTLINE").getInt(null) != 32)
                throw new IllegalStateException("Viewpoint model record layout changed");
        }
    }

    public static void validate() throws ReflectiveOperationException { if (api == null) api = new Api(); }

    public static int[] groups(float[] records, int count, float[] markers) {
        if (count < 0 || count > records.length / 60 || markers.length != 48)
            throw new IllegalArgumentException("Invalid Viewpoint model records");
        int[] groups = new int[count];
        Arrays.fill(groups, -1);
        for (int index = 0; index < count; index++) {
            int at = index * 60 + 32;
            if (records[at + 3] <= 0) continue;
            for (int group = 0; group < 12; group++) {
                int marker = group * 4;
                if (Math.abs(records[at] - markers[marker]) <= 0.004f
                    && Math.abs(records[at + 1] - markers[marker + 1]) <= 0.004f
                    && Math.abs(records[at + 2] - markers[marker + 2]) <= 0.004f) {
                    groups[index] = group;
                    break;
                }
            }
        }
        return groups;
    }

    private static final class GlState {
        final int program = GL11.glGetInteger(GL20.GL_CURRENT_PROGRAM);
        final int vao = GL11.glGetInteger(GL30.GL_VERTEX_ARRAY_BINDING);
        final int arrayBuffer = GL11.glGetInteger(GL15.GL_ARRAY_BUFFER_BINDING);
        final int drawBuffer = GL11.glGetInteger(GL40.GL_DRAW_INDIRECT_BUFFER_BINDING);
        final int activeTexture = GL11.glGetInteger(GL13.GL_ACTIVE_TEXTURE);
        final int depthFunc = GL11.glGetInteger(GL11.GL_DEPTH_FUNC);
        final int cullFace = GL11.glGetInteger(GL11.GL_CULL_FACE_MODE);
        final int srcRgb = GL11.glGetInteger(GL14.GL_BLEND_SRC_RGB), dstRgb = GL11.glGetInteger(GL14.GL_BLEND_DST_RGB);
        final int srcAlpha = GL11.glGetInteger(GL14.GL_BLEND_SRC_ALPHA), dstAlpha = GL11.glGetInteger(GL14.GL_BLEND_DST_ALPHA);
        final int equationRgb = GL11.glGetInteger(GL20.GL_BLEND_EQUATION_RGB), equationAlpha = GL11.glGetInteger(GL20.GL_BLEND_EQUATION_ALPHA);
        final boolean blend = GL11.glIsEnabled(GL11.GL_BLEND), depth = GL11.glIsEnabled(GL11.GL_DEPTH_TEST);
        final boolean cull = GL11.glIsEnabled(GL11.GL_CULL_FACE), scissor = GL11.glIsEnabled(GL11.GL_SCISSOR_TEST);
        final boolean depthMask = GL11.glGetBoolean(GL11.GL_DEPTH_WRITEMASK);
        final int[] viewport = new int[4], texture2d = new int[80], textureBuffer = new int[80], sampler = new int[80];
        final float[] blendColour = new float[4];
        GlState() {
            GL11.glGetIntegerv(GL11.GL_VIEWPORT, viewport);
            GL11.glGetFloatv(GL14.GL_BLEND_COLOR, blendColour);
            for (int unit : units()) {
                GL13.glActiveTexture(GL13.GL_TEXTURE0 + unit);
                texture2d[unit] = GL11.glGetInteger(GL11.GL_TEXTURE_BINDING_2D);
                textureBuffer[unit] = GL11.glGetInteger(GL31.GL_TEXTURE_BINDING_BUFFER);
                sampler[unit] = GL30.glGetIntegeri(GL33.GL_SAMPLER_BINDING, unit);
            }
            GL13.glActiveTexture(activeTexture);
        }
        private static int[] units() {
            int[] units = new int[19];
            units[0] = 0; units[1] = 31; units[2] = 40;
            for (int i = 0; i < 16; i++) units[i + 3] = 64 + i;
            return units;
        }
        private static void enabled(int capability, boolean value) {
            if (value) GL11.glEnable(capability); else GL11.glDisable(capability);
        }
        void restore() {
            for (int unit : units()) {
                GL13.glActiveTexture(GL13.GL_TEXTURE0 + unit);
                GL11.glBindTexture(GL11.GL_TEXTURE_2D, texture2d[unit]);
                GL11.glBindTexture(GL31.GL_TEXTURE_BUFFER, textureBuffer[unit]);
                GL33.glBindSampler(unit, sampler[unit]);
            }
            GL13.glActiveTexture(activeTexture);
            GL30.glBindVertexArray(vao);
            GL15.glBindBuffer(GL15.GL_ARRAY_BUFFER, arrayBuffer);
            GL15.glBindBuffer(GL40.GL_DRAW_INDIRECT_BUFFER, drawBuffer);
            GL20.glUseProgram(program);
            GL11.glViewport(viewport[0], viewport[1], viewport[2], viewport[3]);
            GL11.glDepthFunc(depthFunc); GL11.glDepthMask(depthMask);
            GL11.glCullFace(cullFace);
            GL14.glBlendFuncSeparate(srcRgb, dstRgb, srcAlpha, dstAlpha);
            GL20.glBlendEquationSeparate(equationRgb, equationAlpha);
            GL14.glBlendColor(blendColour[0], blendColour[1], blendColour[2], blendColour[3]);
            enabled(GL11.GL_BLEND, blend); enabled(GL11.GL_DEPTH_TEST, depth);
            enabled(GL11.GL_CULL_FACE, cull); enabled(GL11.GL_SCISSOR_TEST, scissor);
        }
    }

    public static int render(float[] markers, float[] colours, int width, int height) {
        error = "";
        if (width <= 0 || height <= 0 || colours.length != 48) return 0;
        try {
            validate();
            Object pass = api.models.get(null), context = api.frame.get(null);
            if (pass == null || context == null) return 0;
            Object data = api.draws.get(pass);
            if (data == null) return 0;
            int[] groups = groups((float[]) api.values.get(data), api.count.getInt(data), markers);
            boolean[] present = new boolean[12];
            for (int group : groups) if (group >= 0 && colours[group * 4 + 3] > 0) present[group] = true;
            if (!contains(present)) return 0;
            Object program = api.outline.get(pass);
            GlState state = new GlState();
            int painted = 0;
            try {
                api.use.invoke(program);
                api.matrix.invoke(program, "uViewProjection", api.projection.get(context));
                api.begin.invoke(pass);
                GL11.glViewport(0, 0, width, height);
                GL11.glDisable(GL11.GL_DEPTH_TEST); GL11.glDepthMask(false);
                GL11.glDisable(GL11.GL_SCISSOR_TEST); GL11.glDisable(GL11.GL_CULL_FACE);
                GL11.glEnable(GL11.GL_BLEND);
                GL20.glBlendEquationSeparate(GL14.GL_FUNC_ADD, GL14.GL_FUNC_ADD);
                GL14.glBlendFuncSeparate(GL14.GL_CONSTANT_ALPHA, GL14.GL_ONE_MINUS_CONSTANT_ALPHA, GL11.GL_ONE, GL11.GL_ONE_MINUS_SRC_ALPHA);
                for (int group = 0; group < 12; group++) {
                    if (!present[group]) continue;
                    final int selected = group;
                    Object filter = Proxy.newProxyInstance(api.filter.getClassLoader(), new Class<?>[]{api.filter},
                        (proxy, called, args) -> groups[(Integer) args[0]] == selected);
                    int at = group * 4;
                    api.colour.invoke(program, "uColour", colours[at], colours[at + 1], colours[at + 2]);
                    GL14.glBlendColor(0, 0, 0, colours[at + 3]);
                    painted += ((Number) api.draw.invoke(api.batches.get(pass), filter, api.runs.get(pass))).intValue();
                }
                return painted;
            } finally {
                try { api.end.invoke(pass); } finally { state.restore(); }
            }
        } catch (ReflectiveOperationException | RuntimeException failure) {
            error = (failure.getCause() == null ? failure : failure.getCause()).toString();
            return 0;
        }
    }
    private static boolean contains(boolean[] values) { for (boolean value : values) if (value) return true; return false; }
}
