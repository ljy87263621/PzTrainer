package pztrainer.player.visual;

import java.lang.reflect.Field;
import java.lang.reflect.Method;
import java.lang.reflect.Proxy;
import java.util.Arrays;
import java.util.HashMap;
import zombie.core.skinnedmodel.model.ModelInstance;
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
        final Field models, frame, draws, values, count, outline, batches, runs, projection, instances, flags, panini;
        final Method begin, end, use, matrix, colour, draw, id;
        final Class<?> filter;
        final int recordStride, outlineOffset, selfFlag;
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
            instances = field(data, "instances"); flags = field(data, "flags");
            Field warp=null;
            try { warp=field(context,"panini"); } catch(NoSuchFieldException absent) {}
            panini=warp;
            begin = method(pass, "begin"); end = method(pass, "end"); use = method(program, "use");
            id = method(program, "id");
            matrix = method(program, "set", String.class, Class.forName("org.joml.Matrix4f"));
            colour = method(program, "set", String.class, float.class, float.class, float.class);
            draw = method(batch, "draw", filter, Class.forName("viewpoint.render.ModelBatches$State"));
            recordStride = field(data, "VALUES").getInt(null);
            outlineOffset = field(data, "OUTLINE").getInt(null);
            selfFlag = field(data,"SELF").getInt(null);
            if (outlineOffset < 0 || recordStride < outlineOffset + 4 || recordStride > 256)
                throw new IllegalStateException("Invalid Viewpoint model record layout");
        }
    }

    public static void validate() throws ReflectiveOperationException { if (api == null) api = new Api(); }

    public static int[] groupsByOwner(ModelInstance[] instances, int count, int[] identities, int[] entityGroups) {
        if (count < 0 || count > instances.length || identities.length != entityGroups.length)
            throw new IllegalArgumentException("Invalid model ownership map");
        HashMap<Integer,Integer> owners = new HashMap<>();
        for (int i = 0; i < identities.length; i++) {
            int group = entityGroups[i];
            if (group < 0 || group >= 12) throw new IllegalArgumentException("Invalid entity group");
            Integer old = owners.putIfAbsent(identities[i], group);
            if (old != null && old != group) owners.put(identities[i], -1);
        }
        int[] result = new int[count]; Arrays.fill(result, -1);
        for (int i = 0; i < count; i++) {
            ModelInstance instance = instances[i];
            for (int depth = 0; instance != null && depth < 64; depth++, instance = instance.parent) {
                Object owner = instance.object != null ? instance.object : instance.character;
                if (owner != null) {
                    result[i] = owners.getOrDefault(System.identityHashCode(owner), -1);
                    break;
                }
            }
        }
        return result;
    }

    public static int[] groups(float[] records, int count, float[] markers) {
        return groups(records,count,markers,api == null ? 60 : api.recordStride,
            api == null ? 32 : api.outlineOffset);
    }
    public static int[] groups(float[] records, int count, float[] markers, int stride, int outlineOffset) {
        if (outlineOffset < 0 || stride < outlineOffset + 4 || count < 0 || count > records.length / stride || markers.length != 48)
            throw new IllegalArgumentException("Invalid Viewpoint model records");
        int[] groups = new int[count];
        Arrays.fill(groups, -1);
        for (int index = 0; index < count; index++) {
            int at = index * stride + outlineOffset;
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
        final boolean legacyAlpha = org.lwjgl.opengl.GL.getCapabilities().glAlphaFunc != 0;
        final boolean alpha = legacyAlpha && GL11.glIsEnabled(GL11.GL_ALPHA_TEST);
        final boolean stencil = GL11.glIsEnabled(GL11.GL_STENCIL_TEST);
        final boolean logic = GL11.glIsEnabled(GL11.GL_COLOR_LOGIC_OP);
        final boolean discard = GL11.glIsEnabled(GL30.GL_RASTERIZER_DISCARD);
        final boolean depthMask = GL11.glGetBoolean(GL11.GL_DEPTH_WRITEMASK);
        final int drawFbo=GL11.glGetInteger(GL30.GL_DRAW_FRAMEBUFFER_BINDING);
        final int readFbo=GL11.glGetInteger(GL30.GL_READ_FRAMEBUFFER_BINDING);
        final int[] viewport = new int[4], texture2d = new int[80], textureBuffer = new int[80], sampler = new int[80];
        final float[] blendColour = new float[4];
        final float[] clearColour = new float[4];
        final java.nio.ByteBuffer colourMask=org.lwjgl.BufferUtils.createByteBuffer(4);
        GlState() {
            GL11.glGetIntegerv(GL11.GL_VIEWPORT, viewport);
            GL11.glGetFloatv(GL14.GL_BLEND_COLOR, blendColour);
            GL11.glGetFloatv(GL11.GL_COLOR_CLEAR_VALUE,clearColour);
            GL11.glGetBooleanv(GL11.GL_COLOR_WRITEMASK,colourMask);
            for (int unit : units()) {
                GL13.glActiveTexture(GL13.GL_TEXTURE0 + unit);
                texture2d[unit] = GL11.glGetInteger(GL11.GL_TEXTURE_BINDING_2D);
                textureBuffer[unit] = GL11.glGetInteger(GL31.GL_TEXTURE_BINDING_BUFFER);
                sampler[unit] = GL30.glGetIntegeri(GL33.GL_SAMPLER_BINDING, unit);
            }
            GL13.glActiveTexture(activeTexture);
        }
        private static int[] units() {
            int[] units = new int[20];
            units[0] = 0; units[1] = 1; units[2] = 31; units[3] = 40;
            for (int i = 0; i < 16; i++) units[i + 4] = 64 + i;
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
            GL30.glBindFramebuffer(GL30.GL_DRAW_FRAMEBUFFER,drawFbo);
            GL30.glBindFramebuffer(GL30.GL_READ_FRAMEBUFFER,readFbo);
            GL11.glClearColor(clearColour[0],clearColour[1],clearColour[2],clearColour[3]);
            GL11.glColorMask(colourMask.get(0)!=0,colourMask.get(1)!=0,colourMask.get(2)!=0,colourMask.get(3)!=0);
            GL11.glViewport(viewport[0], viewport[1], viewport[2], viewport[3]);
            GL11.glDepthFunc(depthFunc); GL11.glDepthMask(depthMask);
            GL11.glCullFace(cullFace);
            GL14.glBlendFuncSeparate(srcRgb, dstRgb, srcAlpha, dstAlpha);
            GL20.glBlendEquationSeparate(equationRgb, equationAlpha);
            GL14.glBlendColor(blendColour[0], blendColour[1], blendColour[2], blendColour[3]);
            enabled(GL11.GL_BLEND, blend); enabled(GL11.GL_DEPTH_TEST, depth);
            enabled(GL11.GL_CULL_FACE, cull); enabled(GL11.GL_SCISSOR_TEST, scissor);
            if (legacyAlpha) enabled(GL11.GL_ALPHA_TEST, alpha);
            enabled(GL11.GL_STENCIL_TEST, stencil); enabled(GL11.GL_COLOR_LOGIC_OP, logic);
            enabled(GL30.GL_RASTERIZER_DISCARD, discard);
        }
    }

    public static int render(float[] markers, float[] colours, int width, int height) {
        int[] effects=new int[12]; Arrays.fill(effects,2);
        return renderInternal(markers,colours,new float[48],effects,null,null,null,width,height);
    }
    public static int render(float[] markers, float[] colours, int[] effects, int width, int height) {
        int[] legacyEffects=effects.clone();
        for(int i=0;i<legacyEffects.length;i++)if(legacyEffects[i]==0)legacyEffects[i]=2;
        return renderInternal(markers,colours,new float[48],legacyEffects,null,null,null,width,height);
    }
    public static int render(float[] markers,float[] colours,float[] edges,int[] effects,
                             int[] identities,int[] entityGroups,int width,int height) {
        return renderInternal(markers,colours,edges,effects,identities,entityGroups,null,width,height);
    }
    public static int render(float[] markers,float[] colours,float[] edges,int[] effects,
                             int[] identities,int[] entityGroups,int[] layers,int width,int height) {
        return renderInternal(markers,colours,edges,effects,identities,entityGroups,layers,width,height);
    }
    private static float[] panini(Object context) throws ReflectiveOperationException {
        float[] values={0,1,1,1,1};
        Object warp=api.panini==null?null:api.panini.get(context);
        if(warp==null)return values;
        String[] names={"d","halfX","halfY","tanX","tanY"};
        for(int i=0;i<names.length;i++)values[i]=(float)field(warp.getClass(),names[i]).getDouble(warp);
        return values;
    }
    private static int renderInternal(float[] markers,float[] colours,float[] edges,int[] effects,
                                      int[] identities,int[] entityGroups,int[] layers,int width,int height) {
        error = "";
        if (width <= 0 || height <= 0 || colours.length != 48 || edges.length != 48 || effects.length != 12) return 0;
        try {
            validate();
            Object pass = api.models.get(null), context = api.frame.get(null);
            if (pass == null || context == null) return 0;
            Object data = api.draws.get(pass);
            if (data == null) return 0;
            int count=api.count.getInt(data);
            int[] groups = identities==null
                ? groups((float[]) api.values.get(data),count,markers,api.recordStride,api.outlineOffset)
                : groupsByOwner((ModelInstance[])api.instances.get(data),count,identities,entityGroups);
            int[] allowedLayers=new int[count]; Arrays.fill(allowedLayers,3);
            if(layers!=null) {
                if(identities==null || layers.length!=identities.length)throw new IllegalArgumentException("Invalid model layer map");
                for(int value:layers)if(value<0 || value>3)throw new IllegalArgumentException("Invalid model layer flags");
                allowedLayers=groupsByOwner((ModelInstance[])api.instances.get(data),count,identities,layers);
            }
            final int[] selectedLayers=allowedLayers;
            int[] flags=(int[])api.flags.get(data);
            if(identities!=null)for(int i=0;i<count;i++)if(groups[i]>=0 && groups[i]<6 && (flags[i]&api.selfFlag)!=0)groups[i]=-1;
            boolean[] present = new boolean[12];
            for (int i=0;i<groups.length;i++) {
                int group=groups[i]; int layer=allowedLayers[i];
                if(group>=0 && layer>=0 && (((layer&1)!=0 && effects[group]!=0 && colours[group*4+3]>0) || ((layer&2)!=0 && edges[group*4+3]>0)))present[group]=true;
            }
            if (!contains(present)) return 0;
            Object program = api.outline.get(pass);
            GlState state = new GlState();
            int painted = 0;
            try {
                GL13.glActiveTexture(GL13.GL_TEXTURE0);
                if (state.legacyAlpha) GL11.glDisable(GL11.GL_ALPHA_TEST);
                GL11.glDisable(GL11.GL_STENCIL_TEST); GL11.glDisable(GL11.GL_COLOR_LOGIC_OP);
                GL11.glDisable(GL30.GL_RASTERIZER_DISCARD);
                ModelScreenPass screen=ModelScreenPass.get(width,height);
                float[] warp=panini(context);
                api.begin.invoke(pass);
                GL11.glViewport(0, 0, width, height);
                GL11.glDisable(GL11.GL_DEPTH_TEST); GL11.glDepthMask(false);
                GL11.glDisable(GL11.GL_SCISSOR_TEST); GL11.glDisable(GL11.GL_CULL_FACE);
                GL11.glColorMask(true,true,true,true);
                GL20.glBlendEquationSeparate(GL14.GL_FUNC_ADD, GL14.GL_FUNC_ADD);
                for (int group = 0; group < 12; group++) {
                    if (!present[group]) continue;
                    api.use.invoke(program);
                    api.matrix.invoke(program,"uViewProjection",api.projection.get(context));
                    final int selected = group;
                    Object fillFilter = Proxy.newProxyInstance(api.filter.getClassLoader(), new Class<?>[]{api.filter},
                        (proxy, called, args) -> groups[(Integer) args[0]] == selected && (selectedLayers[(Integer)args[0]]&1)!=0);
                    Object rimFilter = Proxy.newProxyInstance(api.filter.getClassLoader(), new Class<?>[]{api.filter},
                        (proxy, called, args) -> groups[(Integer) args[0]] == selected && (selectedLayers[(Integer)args[0]]&2)!=0);
                    int at = group * 4;
                    screen.begin(true);
                    GL11.glDisable(GL11.GL_BLEND);
                    if(edges[at+3]>0) {
                        api.colour.invoke(program,"uColour",1f,1f,1f);
                        painted+=((Number)api.draw.invoke(api.batches.get(pass),rimFilter,api.runs.get(pass))).intValue();
                    }
                    boolean fill=effects[group]!=0 && colours[at+3]>0;
                    screen.begin(false);
                    api.colour.invoke(program, "uColour", colours[at], colours[at + 1], colours[at + 2]);
                    if (fill && group < 6 && (effects[group] == 8 || effects[group] == 9)) {
                        ModelMaterial.use(((Number)api.id.invoke(program)).intValue(), effects[group]);
                    }
                    GL11.glEnable(GL11.GL_BLEND);
                    GL14.glBlendFuncSeparate(GL14.GL_CONSTANT_ALPHA,GL14.GL_ONE_MINUS_CONSTANT_ALPHA,
                        GL14.GL_CONSTANT_ALPHA,GL14.GL_ONE_MINUS_CONSTANT_ALPHA);
                    GL14.glBlendColor(0, 0, 0, colours[at + 3]);
                    if(fill)painted += ((Number) api.draw.invoke(api.batches.get(pass), fillFilter, api.runs.get(pass))).intValue();
                    screen.composite(state.drawFbo,fill,edges,at,warp);
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
