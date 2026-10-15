package pztrainer.player.visual;

import java.io.IOException;
import java.nio.IntBuffer;
import java.nio.charset.StandardCharsets;
import java.util.ArrayList;
import java.util.HashMap;
import java.util.IdentityHashMap;
import java.util.Map;
import java.util.Arrays;
import java.util.regex.Pattern;
import org.lwjgl.BufferUtils;
import org.lwjgl.opengl.GL;
import org.lwjgl.opengl.GL20;

/** Character material programs reuse the mod's skinning and alpha mask. */
public final class ModelMaterial {
    private ModelMaterial() {}
    private static final Pattern MAIN = Pattern.compile("void\\s+main\\s*\\(\\s*(?:void\\s*)?\\)");
    private static final Pattern OUTPUT = Pattern.compile("out\\s+vec4\\s+(\\w+)\\s*;");
    private static final Map<Object, Map<Integer, Material>> contexts = new IdentityHashMap<>();
    private static final Map<Object, Map<Integer, Failure>> failures = new IdentityHashMap<>();
    private static final class Failure {
        final int[] shaders;
        final String message;
        Failure(int[] shaders, RuntimeException failure) { this.shaders=shaders; message=failure.toString(); }
    }
    private static String shared;

    private static String rename(String source) {
        var match = MAIN.matcher(source);
        if (!match.find()) throw new IllegalArgumentException("Model shader has no main function");
        return match.replaceFirst("void pzsaOriginalMain()");
    }
    public static String[] sources(String vertex, String fragment, String position) {
        if (shared == null) {
            try (var input = ModelMaterial.class.getResourceAsStream("thermal_material.glsl")) {
                if (input == null) throw new IllegalStateException("Shared thermal material is missing");
                shared = new String(input.readAllBytes(), StandardCharsets.UTF_8);
            } catch (IOException failure) { throw new IllegalStateException(failure); }
        }
        String output = "gl_FragColor";
        if (!fragment.contains(output)) {
            var match = OUTPUT.matcher(fragment);
            if (!match.find()) throw new IllegalArgumentException("Unsupported model fragment output");
            output = match.group(1);
        }
        return new String[]{rename(vertex) + "\nout vec3 pzsaSurfacePosition;\nvoid main(){pzsaOriginalMain();pzsaSurfacePosition=" + position + ".xyz;}\n",
            rename(fragment) + "\nin vec3 pzsaSurfacePosition;\nuniform int pzsaThermalMode;\n" + shared +
                "\nvoid main(){pzsaOriginalMain();" + output + ".rgb=pzsaHeatColour(pzsaSurfacePosition,pzsaThermalMode);}\n"};
    }
    private static int compile(int type, String source) {
        int shader = GL20.glCreateShader(type);
        GL20.glShaderSource(shader, source); GL20.glCompileShader(shader);
        if (GL20.glGetShaderi(shader, GL20.GL_COMPILE_STATUS) == 0) {
            String error = GL20.glGetShaderInfoLog(shader); GL20.glDeleteShader(shader);
            throw new IllegalStateException(error);
        }
        return shader;
    }
    private static final class Uniform {
        final int from, to, type;
        final float[] values;
        Uniform(int original, int clone, String name, int type) {
            from=GL20.glGetUniformLocation(original,name); to=GL20.glGetUniformLocation(clone,name); this.type=type;
            values=new float[type==GL20.GL_FLOAT_MAT4?16:type==GL20.GL_FLOAT_VEC4?4:type==GL20.GL_FLOAT_VEC3?3:type==GL20.GL_FLOAT_VEC2?2:1];
        }
        void copy(int original) {
            if (from<0 || to<0) return;
            if (type == GL20.GL_FLOAT || type == GL20.GL_FLOAT_VEC2 || type == GL20.GL_FLOAT_VEC3 || type == GL20.GL_FLOAT_VEC4 || type == GL20.GL_FLOAT_MAT4) {
                GL20.glGetUniformfv(original,from,values);
                switch (type) {
                    case GL20.GL_FLOAT_MAT4: GL20.glUniformMatrix4fv(to,false,values); break;
                    case GL20.GL_FLOAT_VEC4: GL20.glUniform4fv(to,values); break;
                    case GL20.GL_FLOAT_VEC3: GL20.glUniform3fv(to,values); break;
                    case GL20.GL_FLOAT_VEC2: GL20.glUniform2fv(to,values); break;
                    default: GL20.glUniform1f(to,values[0]);
                }
            } else {
                // ViewPoint's remaining model uniforms are integer flags and samplers.
                GL20.glUniform1i(to,GL20.glGetUniformi(original,from));
            }
        }
    }
    private static final class Material {
        int program;
        int retainer;
        final int[] shaders;
        final ArrayList<Uniform> uniforms = new ArrayList<>();
        int mode;
        Material(int original, int[] attached) {
            shaders=attached;
            String vertex="",fragment="";
            for (int shader:attached) {
                int type=GL20.glGetShaderi(shader,GL20.GL_SHADER_TYPE);
                if (type==GL20.GL_VERTEX_SHADER) vertex=GL20.glGetShaderSource(shader);
                else if (type==GL20.GL_FRAGMENT_SHADER) fragment=GL20.glGetShaderSource(shader);
                else throw new IllegalArgumentException("Unsupported character shader stage");
            }
            String[] source=sources(vertex,fragment,"aPos");
            int vs=compile(GL20.GL_VERTEX_SHADER,source[0]),fs=0;
            try {
                fs=compile(GL20.GL_FRAGMENT_SHADER,source[1]);
                program=GL20.glCreateProgram();
                GL20.glAttachShader(program,vs); GL20.glAttachShader(program,fs);
                int attributes=GL20.glGetProgrami(original,GL20.GL_ACTIVE_ATTRIBUTES);
                IntBuffer size=BufferUtils.createIntBuffer(1),type=BufferUtils.createIntBuffer(1);
                for (int i=0;i<attributes;i++) {
                    String name=GL20.glGetActiveAttrib(original,i,size,type);
                    GL20.glBindAttribLocation(program,GL20.glGetAttribLocation(original,name),name);
                }
                GL20.glLinkProgram(program);
                if (GL20.glGetProgrami(program,GL20.GL_LINK_STATUS)==0) throw new IllegalStateException(GL20.glGetProgramInfoLog(program));
                int count=GL20.glGetProgrami(original,GL20.GL_ACTIVE_UNIFORMS);
                for (int i=0;i<count;i++) {
                    String name=GL20.glGetActiveUniform(original,i,size,type);
                    for (int element=0;element<size.get(0);element++) {
                        String key=size.get(0)==1?name:name.replace("[0]","["+element+"]");
                        uniforms.add(new Uniform(original,program,key,type.get(0)));
                    }
                }
                mode=GL20.glGetUniformLocation(program,"pzsaThermalMode");
                // Retain the source shader identities without touching the
                // original program, so GL object-id reuse cannot alias a cache entry.
                retainer=GL20.glCreateProgram();
                for(int shader:attached) GL20.glAttachShader(retainer,shader);
            } catch (RuntimeException failure) {
                if (program!=0) GL20.glDeleteProgram(program); throw failure;
            } finally { GL20.glDeleteShader(vs); if(fs!=0)GL20.glDeleteShader(fs); }
        }
    }
    public static void use(int original, int effect) {
        Object context=GL.getCapabilities();
        Map<Integer,Material> programs=contexts.computeIfAbsent(context, key->new HashMap<>());
        Map<Integer,Failure> failed=failures.computeIfAbsent(context,key->new HashMap<>());
        int[] attached=new int[GL20.glGetProgrami(original,GL20.GL_ATTACHED_SHADERS)];
        GL20.glGetAttachedShaders(original,null,attached);
        Failure previous=failed.get(original);
        if(previous!=null && Arrays.equals(previous.shaders,attached)) throw new IllegalStateException(previous.message);
        failed.remove(original);
        Material material=programs.get(original);
        if(material!=null && !Arrays.equals(material.shaders,attached)) {
            GL20.glDeleteProgram(material.program); GL20.glDeleteProgram(material.retainer);
            programs.remove(original); material=null;
        }
        if(material==null) {
            try { material=new Material(original,attached); programs.put(original,material); }
            catch(RuntimeException failure) { failed.put(original,new Failure(attached,failure)); throw failure; }
        }
        GL20.glUseProgram(material.program);
        for (Uniform uniform:material.uniforms) uniform.copy(original);
        GL20.glUniform1i(material.mode,effect==9?2:1);
    }
}
