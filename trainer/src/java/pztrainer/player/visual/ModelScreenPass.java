package pztrainer.player.visual;

import java.io.IOException;
import java.nio.charset.StandardCharsets;
import java.util.IdentityHashMap;
import org.lwjgl.opengl.GL;
import org.lwjgl.opengl.GL11;
import org.lwjgl.opengl.GL12;
import org.lwjgl.opengl.GL13;
import org.lwjgl.opengl.GL14;
import org.lwjgl.opengl.GL20;
import org.lwjgl.opengl.GL30;
import org.lwjgl.opengl.GL33;

/** Private screen targets keep model interiors and edge glow independent. */
public final class ModelScreenPass {
    private static final IdentityHashMap<Object,ModelScreenPass> contexts=new IdentityHashMap<>();
    private final int program,vao;
    private final int[] fbo=new int[2],texture=new int[2];
    private int width,height;
    private final int maskLocation,fillLocation,enabledLocation,edgeLocation,pixelLocation,paniniLocation,tanLocation;
    private static String resource(String name) {
        try(var input=ModelScreenPass.class.getResourceAsStream(name)) {
            if(input==null)throw new IllegalStateException("Missing model composite shader: "+name);
            return new String(input.readAllBytes(),StandardCharsets.UTF_8);
        } catch(IOException failure) { throw new IllegalStateException(failure); }
    }
    private static int compile(int stage,String source) {
        int shader=GL20.glCreateShader(stage); GL20.glShaderSource(shader,source); GL20.glCompileShader(shader);
        if(GL20.glGetShaderi(shader,GL20.GL_COMPILE_STATUS)==0) {
            String error=GL20.glGetShaderInfoLog(shader); GL20.glDeleteShader(shader); throw new IllegalStateException(error);
        }
        return shader;
    }
    private ModelScreenPass() {
        int vs=compile(GL20.GL_VERTEX_SHADER,resource("model_composite.vert")),fs=0;
        program=GL20.glCreateProgram();
        try {
            fs=compile(GL20.GL_FRAGMENT_SHADER,resource("model_composite.frag"));
            GL20.glAttachShader(program,vs); GL20.glAttachShader(program,fs); GL20.glLinkProgram(program);
            if(GL20.glGetProgrami(program,GL20.GL_LINK_STATUS)==0)throw new IllegalStateException(GL20.glGetProgramInfoLog(program));
        } finally { GL20.glDeleteShader(vs); if(fs!=0)GL20.glDeleteShader(fs); }
        vao=GL30.glGenVertexArrays();
        for(int i=0;i<2;i++) { fbo[i]=GL30.glGenFramebuffers(); texture[i]=GL11.glGenTextures(); }
        maskLocation=location("maskTexture"); fillLocation=location("fillTexture"); enabledLocation=location("fillEnabled");
        edgeLocation=location("edgeColour"); pixelLocation=location("pixelSize"); paniniLocation=location("panini"); tanLocation=location("paniniTan");
    }
    private int location(String name) { return GL20.glGetUniformLocation(program,name); }
    public static ModelScreenPass get(int width,int height) {
        ModelScreenPass pass=contexts.computeIfAbsent(GL.getCapabilities(),key->new ModelScreenPass());
        pass.resize(width,height); return pass;
    }
    private void resize(int width,int height) {
        if(this.width==width && this.height==height)return;
        if(width<=0 || height<=0)throw new IllegalArgumentException("Invalid model target dimensions");
        for(int i=0;i<2;i++) {
            GL11.glBindTexture(GL11.GL_TEXTURE_2D,texture[i]);
            GL11.glTexImage2D(GL11.GL_TEXTURE_2D,0,GL11.GL_RGBA8,width,height,0,GL11.GL_RGBA,GL11.GL_UNSIGNED_BYTE,(java.nio.ByteBuffer)null);
            GL11.glTexParameteri(GL11.GL_TEXTURE_2D,GL11.GL_TEXTURE_MIN_FILTER,GL11.GL_LINEAR);
            GL11.glTexParameteri(GL11.GL_TEXTURE_2D,GL11.GL_TEXTURE_MAG_FILTER,GL11.GL_LINEAR);
            GL11.glTexParameteri(GL11.GL_TEXTURE_2D,GL11.GL_TEXTURE_WRAP_S,GL12.GL_CLAMP_TO_EDGE);
            GL11.glTexParameteri(GL11.GL_TEXTURE_2D,GL11.GL_TEXTURE_WRAP_T,GL12.GL_CLAMP_TO_EDGE);
            GL30.glBindFramebuffer(GL30.GL_DRAW_FRAMEBUFFER,fbo[i]);
            GL30.glFramebufferTexture2D(GL30.GL_DRAW_FRAMEBUFFER,GL30.GL_COLOR_ATTACHMENT0,GL11.GL_TEXTURE_2D,texture[i],0);
            GL11.glDrawBuffer(GL30.GL_COLOR_ATTACHMENT0);
            if(GL30.glCheckFramebufferStatus(GL30.GL_DRAW_FRAMEBUFFER)!=GL30.GL_FRAMEBUFFER_COMPLETE)
                throw new IllegalStateException("Model target framebuffer incomplete");
        }
        this.width=width; this.height=height;
    }
    public void begin(boolean mask) {
        GL30.glBindFramebuffer(GL30.GL_DRAW_FRAMEBUFFER,fbo[mask?0:1]);
        GL11.glViewport(0,0,width,height); GL11.glClearColor(0,0,0,0); GL11.glClear(GL11.GL_COLOR_BUFFER_BIT);
    }
    public void composite(int output,boolean fill,float[] edges,int offset,float[] panini) {
        GL30.glBindFramebuffer(GL30.GL_DRAW_FRAMEBUFFER,output); GL11.glViewport(0,0,width,height);
        GL11.glDisable(GL11.GL_DEPTH_TEST); GL11.glDisable(GL11.GL_CULL_FACE); GL11.glDisable(GL11.GL_SCISSOR_TEST);
        GL20.glUseProgram(program); GL30.glBindVertexArray(vao);
        for(int i=0;i<2;i++) {
            GL13.glActiveTexture(GL13.GL_TEXTURE0+i); GL33.glBindSampler(i,0); GL11.glBindTexture(GL11.GL_TEXTURE_2D,texture[i]);
        }
        GL20.glUniform1i(maskLocation,0); GL20.glUniform1i(fillLocation,1); GL20.glUniform1i(enabledLocation,fill?1:0);
        GL20.glUniform4f(edgeLocation,edges[offset],edges[offset+1],edges[offset+2],edges[offset+3]);
        GL20.glUniform2f(pixelLocation,1f/width,1f/height);
        GL20.glUniform3f(paniniLocation,panini[0],panini[1],panini[2]); GL20.glUniform2f(tanLocation,panini[3],panini[4]);
        GL11.glEnable(GL11.GL_BLEND); GL14.glBlendFuncSeparate(GL11.GL_ONE,GL11.GL_ONE_MINUS_SRC_ALPHA,GL11.GL_ONE,GL11.GL_ONE_MINUS_SRC_ALPHA);
        GL11.glDrawArrays(GL11.GL_TRIANGLES,0,3);
    }
}
