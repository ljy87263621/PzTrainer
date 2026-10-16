import java.nio.file.Files;
import java.nio.file.Path;
import org.lwjgl.glfw.GLFW;
import org.lwjgl.opengl.*;
import pztrainer.player.visual.NativeModelCapture;

public class NativeCaptureGlTest {
    private static native String initialize(String jar);
    private static native float[] begin(int mode,int category);
    private static native int replay();
    private static int shader(int kind,String source) {
        int shader=GL20.glCreateShader(kind);GL20.glShaderSource(shader,source);GL20.glCompileShader(shader);
        if(GL20.glGetShaderi(shader,GL20.GL_COMPILE_STATUS)==0)throw new AssertionError(GL20.glGetShaderInfoLog(shader));
        return shader;
    }
    private static int[] pixel(int x,int y) {
        var data=org.lwjgl.BufferUtils.createByteBuffer(4);GL11.glReadPixels(x,y,1,1,GL11.GL_RGBA,GL11.GL_UNSIGNED_BYTE,data);
        return new int[]{data.get(0)&255,data.get(1)&255,data.get(2)&255,data.get(3)&255};
    }
    public static void main(String[] args) throws Exception {
        if(!GLFW.glfwInit())throw new AssertionError("GLFW init failed");
        GLFW.glfwWindowHint(GLFW.GLFW_VISIBLE,GLFW.GLFW_FALSE);
        long window=GLFW.glfwCreateWindow(64,64,"Native model capture test",0,0);
        if(window==0)throw new AssertionError("GL window failed");
        GLFW.glfwMakeContextCurrent(window);GL.createCapabilities();
        System.load(args[0]);String error=initialize(args[1]);
        if(!error.isEmpty())throw new AssertionError("Native hook installation failed: "+error);
        Path shaders=Path.of(args[2],"media","shaders");
        int program=GL20.glCreateProgram();
        GL20.glAttachShader(program,shader(GL20.GL_VERTEX_SHADER,Files.readString(shaders.resolve("aim_outline_solid.vert"))));
        GL20.glAttachShader(program,shader(GL20.GL_FRAGMENT_SHADER,Files.readString(shaders.resolve("aim_outline_solid.frag"))));
        GL20.glLinkProgram(program);
        if(GL20.glGetProgrami(program,GL20.GL_LINK_STATUS)==0)throw new AssertionError(GL20.glGetProgramInfoLog(program));
        int vao=GL30.glGenVertexArrays();GL30.glBindVertexArray(vao);
        int vbo=GL15.glGenBuffers();GL15.glBindBuffer(GL15.GL_ARRAY_BUFFER,vbo);
        GL15.glBufferData(GL15.GL_ARRAY_BUFFER,new float[]{-.5f,-.5f,0,.5f,-.5f,0,.5f,.5f,0,-.5f,.5f,0},GL15.GL_STATIC_DRAW);
        GL20.glVertexAttribPointer(0,3,GL11.GL_FLOAT,false,12,0);GL20.glEnableVertexAttribArray(0);
        GL20.glVertexAttrib4f(2,1,0,0,0);GL20.glVertexAttrib4f(3,0,0,0,0);
        int ebo=GL15.glGenBuffers();GL15.glBindBuffer(GL15.GL_ELEMENT_ARRAY_BUFFER,ebo);
        GL15.glBufferData(GL15.GL_ELEMENT_ARRAY_BUFFER,new int[]{0,1,2,0,2,3},GL15.GL_STATIC_DRAW);
        int texture=GL11.glGenTextures();GL11.glBindTexture(GL11.GL_TEXTURE_2D,texture);
        GL11.glTexImage2D(GL11.GL_TEXTURE_2D,0,GL11.GL_RGBA,1,1,0,GL11.GL_RGBA,GL11.GL_UNSIGNED_BYTE,new int[]{-1});
        GL11.glTexParameteri(GL11.GL_TEXTURE_2D,GL11.GL_TEXTURE_MIN_FILTER,GL11.GL_NEAREST);
        GL11.glTexParameteri(GL11.GL_TEXTURE_2D,GL11.GL_TEXTURE_MAG_FILTER,GL11.GL_NEAREST);
        int output=GL30.glGenFramebuffers(),image=GL11.glGenTextures();GL11.glBindTexture(GL11.GL_TEXTURE_2D,image);
        GL11.glTexImage2D(GL11.GL_TEXTURE_2D,0,GL11.GL_RGBA,64,64,0,GL11.GL_RGBA,GL11.GL_UNSIGNED_BYTE,(java.nio.ByteBuffer)null);
        GL30.glBindFramebuffer(GL30.GL_FRAMEBUFFER,output);GL30.glFramebufferTexture2D(GL30.GL_FRAMEBUFFER,GL30.GL_COLOR_ATTACHMENT0,GL11.GL_TEXTURE_2D,image,0);
        GL11.glDrawBuffer(GL30.GL_COLOR_ATTACHMENT0);GL11.glReadBuffer(GL30.GL_COLOR_ATTACHMENT0);
        float[] identity={1,0,0,0,0,1,0,0,0,0,1,0,0,0,0,1};
        for(int category=0;category<2;++category)for(int size:new int[]{64,96,128})for(int mode=0;mode<3;++mode) {
            float[] marker=begin(mode,category);GL30.glBindVertexArray(vao);GL20.glUseProgram(program);
            GL20.glUniformMatrix4fv(GL20.glGetUniformLocation(program,"ModelViewProjection"),false,identity);
            GL20.glUniformMatrix4fv(GL20.glGetUniformLocation(program,"MatrixPalette[0]"),false,identity);
            GL20.glUniform1f(GL20.glGetUniformLocation(program,"FinalScale"),1);
            // Match the actual engine marker. Explicit owner identity selects the entity.
            GL20.glUniform4f(GL20.glGetUniformLocation(program,"u_color"),marker[0],marker[1],marker[2],1);
            GL11.glBindTexture(GL11.GL_TEXTURE_2D,texture);GL11.glViewport(0,0,size,size);
            NativeModelCapture.capture(category==0?42:84,()->GL11.glDrawElements(GL11.GL_TRIANGLES,6,GL11.GL_UNSIGNED_INT,0));
            GL11.glDrawElements(GL11.GL_TRIANGLES,6,GL11.GL_UNSIGNED_INT,0); // unowned overlapping draw
            GL11.glViewport(0,0,64,64);GL11.glClearColor(0,0,0,1);GL11.glClear(GL11.GL_COLOR_BUFFER_BIT);
            int count=replay();
            if(count!=1)throw new AssertionError("No exact owned draw captured: "+count+" (viewport "+size+")");
            int[] center=pixel(32,32),edge=pixel(14,32),outside=pixel(8,32);
            if((mode==1?center[0]!=0:center[0]<200) || (mode==0?edge[1]!=0:edge[1]<20) || outside[0]!=0 || outside[1]!=0)
                throw new AssertionError("Native fill/rim pixels failed mode="+mode+" viewport="+size+" center="+java.util.Arrays.toString(center)+" edge="+java.util.Arrays.toString(edge));
        }
        float[] marker=begin(2,0);GL30.glBindVertexArray(vao);GL20.glUseProgram(program);
        GL11.glBindTexture(GL11.GL_TEXTURE_2D,texture);GL11.glViewport(0,0,128,128);
        GL20.glUniform4f(GL20.glGetUniformLocation(program,"u_color"),marker[0],marker[1],marker[2],1);
        GL20.glUniformMatrix4fv(GL20.glGetUniformLocation(program,"ModelViewProjection"),false,identity);
        NativeModelCapture.capture(42,()->GL11.glDrawElements(GL11.GL_TRIANGLES,6,GL11.GL_UNSIGNED_INT,0));
        float[] self=identity.clone();self[0]=.25f;
        GL20.glUniformMatrix4fv(GL20.glGetUniformLocation(program,"ModelViewProjection"),false,self);
        NativeModelCapture.capture(-1,()->GL11.glDrawElements(GL11.GL_TRIANGLES,6,GL11.GL_UNSIGNED_INT,0));
        float[] other=identity.clone();other[12]=-.75f;
        GL20.glUniformMatrix4fv(GL20.glGetUniformLocation(program,"ModelViewProjection"),false,other);
        NativeModelCapture.capture(999,()->GL11.glDrawElements(GL11.GL_TRIANGLES,6,GL11.GL_UNSIGNED_INT,0));
        GL11.glViewport(0,0,64,64);GL11.glClearColor(0,0,0,1);GL11.glClear(GL11.GL_COLOR_BUFFER_BIT);
        if(replay()!=3 || pixel(32,32)[0]!=0 || pixel(20,32)[0]<200 || pixel(14,32)[1]<20 || pixel(8,32)[0]!=0 || pixel(8,32)[1]!=0)
            throw new AssertionError("Self mask / untracked owner leaked colour into overlapping models");
        if(GL11.glGetInteger(GL30.GL_FRAMEBUFFER_BINDING)!=output)throw new AssertionError("Output framebuffer lost");
        System.out.println("Production native hooks + actual game outline shaders: zombie/player identity, fill/rim/combined, self mask, untracked exclusion and 1/1.5/2x viewport scaling passed.");
        GLFW.glfwDestroyWindow(window);GLFW.glfwTerminate();
    }
}
