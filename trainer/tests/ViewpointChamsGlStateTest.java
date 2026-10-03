import org.lwjgl.glfw.GLFW;
import org.lwjgl.opengl.GL;
import org.lwjgl.opengl.GL11;
import org.lwjgl.opengl.GL13;
import org.lwjgl.opengl.GL14;
import org.lwjgl.opengl.GL15;
import org.lwjgl.opengl.GL20;
import org.lwjgl.opengl.GL30;
import org.lwjgl.opengl.GL31;
import org.lwjgl.opengl.GL33;
import org.lwjgl.opengl.GL40;

public class ViewpointChamsGlStateTest {
    private static void check(boolean value, String message) { if (!value) throw new AssertionError(message); }
    public static void main(String[] args) throws Exception {
        check(GLFW.glfwInit(), "GLFW initialization failed");
        GLFW.glfwWindowHint(GLFW.GLFW_VISIBLE, GLFW.GLFW_FALSE);
        GLFW.glfwWindowHint(GLFW.GLFW_CONTEXT_VERSION_MAJOR, 4);
        GLFW.glfwWindowHint(GLFW.GLFW_CONTEXT_VERSION_MINOR, 3);
        long window = GLFW.glfwCreateWindow(64, 64, "Viewpoint chams test", 0, 0);
        check(window != 0, "Hidden OpenGL context creation failed");
        try {
            GLFW.glfwMakeContextCurrent(window);
            GL.createCapabilities();
            int vao = GL30.glGenVertexArrays(), buffer = GL15.glGenBuffers();
            int texture = GL11.glGenTextures(), textureBuffer = GL11.glGenTextures(), sampler = GL33.glGenSamplers();
            GL30.glBindVertexArray(vao);
            GL15.glBindBuffer(GL15.GL_ARRAY_BUFFER, buffer);
            GL15.glBindBuffer(GL40.GL_DRAW_INDIRECT_BUFFER, buffer);
            GL11.glViewport(3, 5, 37, 41);
            GL11.glEnable(GL11.GL_DEPTH_TEST); GL11.glDepthMask(true); GL11.glDepthFunc(GL11.GL_GREATER);
            GL11.glEnable(GL11.GL_CULL_FACE); GL11.glCullFace(GL11.GL_FRONT);
            GL11.glEnable(GL11.GL_SCISSOR_TEST); GL11.glDisable(GL11.GL_BLEND);
            GL14.glBlendFuncSeparate(GL11.GL_ONE, GL11.GL_ZERO, GL11.GL_ZERO, GL11.GL_ONE);
            GL20.glBlendEquationSeparate(GL14.GL_FUNC_SUBTRACT, GL14.GL_FUNC_REVERSE_SUBTRACT);
            GL14.glBlendColor(0.1f, 0.2f, 0.3f, 0.4f);
            GL13.glActiveTexture(GL13.GL_TEXTURE0 + 64);
            GL11.glBindTexture(GL11.GL_TEXTURE_2D, texture);
            GL11.glBindTexture(GL31.GL_TEXTURE_BUFFER, textureBuffer);
            GL33.glBindSampler(64, sampler);
            Class<?> stateType = Class.forName("pztrainer.player.visual.ViewpointModelChams$GlState");
            var constructor = stateType.getDeclaredConstructor(); constructor.setAccessible(true);
            Object state = constructor.newInstance();
            GL30.glBindVertexArray(0); GL15.glBindBuffer(GL15.GL_ARRAY_BUFFER, 0);
            GL15.glBindBuffer(GL40.GL_DRAW_INDIRECT_BUFFER, 0);
            GL11.glViewport(0, 0, 64, 64);
            GL11.glDisable(GL11.GL_DEPTH_TEST); GL11.glDepthMask(false); GL11.glDepthFunc(GL11.GL_LESS);
            GL11.glDisable(GL11.GL_CULL_FACE); GL11.glCullFace(GL11.GL_BACK);
            GL11.glDisable(GL11.GL_SCISSOR_TEST); GL11.glEnable(GL11.GL_BLEND);
            GL14.glBlendColor(0, 0, 0, 1);
            GL11.glBindTexture(GL11.GL_TEXTURE_2D, 0); GL11.glBindTexture(GL31.GL_TEXTURE_BUFFER, 0);
            GL33.glBindSampler(64, 0); GL13.glActiveTexture(GL13.GL_TEXTURE0);
            var restore = stateType.getDeclaredMethod("restore"); restore.setAccessible(true); restore.invoke(state);
            check(GL11.glGetInteger(GL30.GL_VERTEX_ARRAY_BINDING) == vao, "VAO not restored");
            check(GL11.glGetInteger(GL15.GL_ARRAY_BUFFER_BINDING) == buffer, "Vertex buffer not restored");
            check(GL11.glGetInteger(GL40.GL_DRAW_INDIRECT_BUFFER_BINDING) == buffer, "Indirect draw buffer not restored");
            check(GL11.glGetInteger(GL13.GL_ACTIVE_TEXTURE) == GL13.GL_TEXTURE0 + 64, "Texture unit not restored");
            check(GL11.glGetInteger(GL11.GL_TEXTURE_BINDING_2D) == texture, "Model texture not restored");
            check(GL11.glGetInteger(GL31.GL_TEXTURE_BINDING_BUFFER) == textureBuffer, "Palette texture not restored");
            check(GL30.glGetIntegeri(GL33.GL_SAMPLER_BINDING, 64) == sampler, "Texture sampler not restored");
            check(GL11.glIsEnabled(GL11.GL_DEPTH_TEST) && GL11.glGetBoolean(GL11.GL_DEPTH_WRITEMASK), "Depth state not restored");
            check(GL11.glGetInteger(GL11.GL_DEPTH_FUNC) == GL11.GL_GREATER, "Depth comparison not restored");
            check(GL11.glIsEnabled(GL11.GL_CULL_FACE) && GL11.glGetInteger(GL11.GL_CULL_FACE_MODE) == GL11.GL_FRONT, "Cull state not restored");
            check(GL11.glIsEnabled(GL11.GL_SCISSOR_TEST) && !GL11.glIsEnabled(GL11.GL_BLEND), "UI clipping or blending state changed");
            int[] viewport = new int[4]; GL11.glGetIntegerv(GL11.GL_VIEWPORT, viewport);
            check(viewport[0] == 3 && viewport[1] == 5 && viewport[2] == 37 && viewport[3] == 41, "Viewport not restored");
            float[] colour = new float[4]; GL11.glGetFloatv(GL14.GL_BLEND_COLOR, colour);
            check(Math.abs(colour[3] - 0.4f) < 0.001f, "Model opacity not restored");
            check(GL11.glGetError() == GL11.GL_NO_ERROR, "OpenGL error during state preservation");
            System.out.println("Hidden OpenGL context: model textures, samplers, indirect buffer, viewport, depth, culling and UI state restoration passed.");
            ViewpointChamsRenderTest.run();
        } finally { GLFW.glfwDestroyWindow(window); GLFW.glfwTerminate(); }
    }
}
