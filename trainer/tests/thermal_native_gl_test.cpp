#include <cassert>
#include <fstream>
#include <iostream>
#include <filesystem>
#include <iterator>
#include "features/visual/native_model_material.hpp"
using namespace pztrainer::features::visual;
template<class T> T gl(const char* name) { auto p=wglGetProcAddress(name); assert(p); return reinterpret_cast<T>(p); }
std::string read(const std::filesystem::path& path) {
    std::ifstream stream(path); assert(stream); return {std::istreambuf_iterator<char>(stream),{}};
}
int wmain(int argc,wchar_t** argv) {
    _set_error_mode(_OUT_TO_STDERR);
    _set_abort_behavior(0, _WRITE_ABORT_MSG | _CALL_REPORTFAULT);
    assert(argc==2);
    HWND window=CreateWindowW(L"STATIC",L"thermal material test",WS_OVERLAPPED,0,0,64,64,nullptr,nullptr,GetModuleHandleW(nullptr),nullptr);
    assert(window); HDC dc=GetDC(window);
    PIXELFORMATDESCRIPTOR format{sizeof(format),1};
    format.dwFlags=PFD_DRAW_TO_WINDOW|PFD_SUPPORT_OPENGL|PFD_DOUBLEBUFFER;
    format.iPixelType=PFD_TYPE_RGBA; format.cColorBits=32; format.cDepthBits=24;
    int pixelFormat=ChoosePixelFormat(dc,&format); assert(pixelFormat && SetPixelFormat(dc,pixelFormat,&format));
    HGLRC context=wglCreateContext(dc); assert(context && wglMakeCurrent(dc,context));
    auto createShader=gl<GLuint(APIENTRY*)(GLenum)>("glCreateShader");
    auto shaderSource=gl<void(APIENTRY*)(GLuint,GLsizei,const char* const*,const GLint*)>("glShaderSource");
    auto compile=gl<void(APIENTRY*)(GLuint)>("glCompileShader");
    auto query=gl<void(APIENTRY*)(GLuint,GLenum,GLint*)>("glGetShaderiv");
    auto createProgram=gl<GLuint(APIENTRY*)()>("glCreateProgram");
    auto attach=gl<void(APIENTRY*)(GLuint,GLuint)>("glAttachShader");
    auto link=gl<void(APIENTRY*)(GLuint)>("glLinkProgram");
    auto getProgram=gl<void(APIENTRY*)(GLuint,GLenum,GLint*)>("glGetProgramiv");
    auto use=gl<void(APIENTRY*)(GLuint)>("glUseProgram");
    auto location=gl<GLint(APIENTRY*)(GLuint,const char*)>("glGetUniformLocation");
    auto matrix=gl<void(APIENTRY*)(GLint,GLsizei,GLboolean,const GLfloat*)>("glUniformMatrix4fv");
    auto f1=gl<void(APIENTRY*)(GLint,GLfloat)>("glUniform1f");
    auto i1=gl<void(APIENTRY*)(GLint,GLint)>("glUniform1i");
    auto f4=gl<void(APIENTRY*)(GLint,GLfloat,GLfloat,GLfloat,GLfloat)>("glUniform4f");
    auto genVao=gl<void(APIENTRY*)(GLsizei,GLuint*)>("glGenVertexArrays");
    auto bindVao=gl<void(APIENTRY*)(GLuint)>("glBindVertexArray");
    auto genBuffers=gl<void(APIENTRY*)(GLsizei,GLuint*)>("glGenBuffers");
    auto bindBuffer=gl<void(APIENTRY*)(GLenum,GLuint)>("glBindBuffer");
    auto data=gl<void(APIENTRY*)(GLenum,std::ptrdiff_t,const void*,GLenum)>("glBufferData");
    auto attrib=gl<void(APIENTRY*)(GLuint,GLint,GLenum,GLboolean,GLsizei,const void*)>("glVertexAttribPointer");
    auto enable=gl<void(APIENTRY*)(GLuint)>("glEnableVertexAttribArray");
    const auto root=std::filesystem::path(argv[1])/L"media/shaders";
    GLuint original=createProgram();
    for (auto stage:{std::pair<GLenum,const wchar_t*>{0x8B31,L"aim_outline_solid.vert"},{0x8B30,L"aim_outline_solid.frag"}}) {
        auto source=read(root/stage.second); const char* ptr=source.c_str();
        GLuint shader=createShader(stage.first); shaderSource(shader,1,&ptr,nullptr); compile(shader);
        GLint ok=0; query(shader,0x8B81,&ok); assert(ok); attach(original,shader);
    }
    link(original); GLint linked=0; getProgram(original,0x8B82,&linked); assert(linked);
    NativeModelMaterial materials; const auto* program=materials.Get(original);
    if (!program) { std::cerr<<materials.error; return 1; }
    assert(materials.Get(original)->id==program->id); // Cache avoids recompilation.
    GLuint vao=0,vbo=0; genVao(1,&vao); bindVao(vao); genBuffers(1,&vbo); bindBuffer(0x8892,vbo);
    const float vertices[]{
        -0.4f,0,0,1, 0,0,1,0, 1,0,0,0, 0,0,0,0, 0,0,
        0.4f,0,0,1, 0,0,1,0, 1,0,0,0, 0,0,0,0, 1,0,
        -0.4f,1.8f,0,1, 0,0,1,0, 1,0,0,0, 0,0,0,0, 0,1,
        0.4f,1.8f,0,1, 0,0,1,0, 1,0,0,0, 0,0,0,0, 1,1};
    data(0x8892,sizeof(vertices),vertices,0x88E4);
    for (GLuint index=0;index<5;index++) { attrib(index,index==4?2:4,GL_FLOAT,GL_FALSE,18*sizeof(float),reinterpret_cast<void*>(index*4*sizeof(float))); enable(index); }
    GLuint texture=0; glGenTextures(1,&texture); glBindTexture(GL_TEXTURE_2D,texture);
    unsigned char pixels[]{255,255,255,255,255,255,255,0};
    glTexImage2D(GL_TEXTURE_2D,0,GL_RGBA,2,1,0,GL_RGBA,GL_UNSIGNED_BYTE,pixels);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_NEAREST); glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_NEAREST);
    auto genFramebuffer=gl<void(APIENTRY*)(GLsizei,GLuint*)>("glGenFramebuffers");
    auto bindFramebuffer=gl<void(APIENTRY*)(GLenum,GLuint)>("glBindFramebuffer");
    auto attachTexture=gl<void(APIENTRY*)(GLenum,GLenum,GLenum,GLuint,GLint)>("glFramebufferTexture2D");
    GLuint framebuffer=0,target=0; genFramebuffer(1,&framebuffer); bindFramebuffer(0x8D40,framebuffer);
    glGenTextures(1,&target); glBindTexture(GL_TEXTURE_2D,target);
    glTexImage2D(GL_TEXTURE_2D,0,GL_RGBA,64,64,0,GL_RGBA,GL_UNSIGNED_BYTE,nullptr);
    attachTexture(0x8D40,0x8CE0,GL_TEXTURE_2D,target,0); glDrawBuffer(0x8CE0); glReadBuffer(0x8CE0);
    glBindTexture(GL_TEXTURE_2D,texture);
    glViewport(0,0,64,64); glDisable(GL_DEPTH_TEST); glEnable(GL_BLEND); glBlendFunc(GL_SRC_ALPHA,GL_ONE_MINUS_SRC_ALPHA);
    float projection[16]{}; projection[0]=2; projection[5]=projection[10]=projection[15]=1; projection[13]=-0.9f;
    float palette[60*16]{}; for(int j=0;j<60;j++)for(int k=0;k<4;k++)palette[j*16+k*5]=1;
    use(program->id); matrix(program->projection,1,GL_FALSE,projection); matrix(program->palette,60,GL_FALSE,palette);
    f1(program->scale,1); f4(program->colour,0,0,1,0.5f); i1(program->texture,0); i1(program->mode,1);
    glClearColor(0,0,0,1); glClear(GL_COLOR_BUFFER_BIT); glDrawArrays(GL_TRIANGLE_STRIP,0,4);
    unsigned char cool[4]{},warm[4]{},masked[4]{};
    glReadPixels(24,8,1,1,GL_RGBA,GL_UNSIGNED_BYTE,cool); glReadPixels(24,32,1,1,GL_RGBA,GL_UNSIGNED_BYTE,warm);
    glReadPixels(40,32,1,1,GL_RGBA,GL_UNSIGNED_BYTE,masked);
    std::cout<<"GPU thermal samples: warm="<<int(warm[0])<<","<<int(warm[1])<<","<<int(warm[2])
        <<" cool="<<int(cool[0])<<","<<int(cool[1])<<","<<int(cool[2])<<std::endl;
    assert(warm[1]>cool[1]+40 && warm[0]>100 && warm[0]<140);
    assert(masked[0]==0 && masked[1]==0 && masked[2]==0);
    const auto expected=ThermalPalette(ThermalSignal(-0.1171875f,0.915625f,0),false,0.5f);
    assert(std::abs(warm[0]-std::lround(expected.x*127.5f))<=2);
    i1(program->mode,2); glClear(GL_COLOR_BUFFER_BIT); glDrawArrays(GL_TRIANGLE_STRIP,0,4);
    glReadPixels(24,32,1,1,GL_RGBA,GL_UNSIGNED_BYTE,warm);
    assert(warm[0]==warm[1] && warm[1]==warm[2]);
    palette[12]=0.35f; matrix(program->palette,60,GL_FALSE,palette);
    glClear(GL_COLOR_BUFFER_BIT); glDrawArrays(GL_TRIANGLE_STRIP,0,4);
    glReadPixels(10,32,1,1,GL_RGBA,GL_UNSIGNED_BYTE,masked); assert(masked[0]==0);
    use(original); i1(location(original,"Texture"),0); matrix(location(original,"ModelViewProjection"),1,GL_FALSE,projection);
    matrix(location(original,"MatrixPalette[0]"),60,GL_FALSE,palette); f1(location(original,"FinalScale"),1); f4(location(original,"u_color"),0,0,1,1);
    glClear(GL_COLOR_BUFFER_BIT); glDrawArrays(GL_TRIANGLE_STRIP,0,4);
    glReadPixels(32,32,1,1,GL_RGBA,GL_UNSIGNED_BYTE,warm); assert(warm[0]==0 && warm[1]==0 && warm[2]>250);
    assert(glGetError()==GL_NO_ERROR);
    wglMakeCurrent(nullptr,nullptr); wglDeleteContext(context); ReleaseDC(window,dc); DestroyWindow(window);
    std::cout<<"Native GPU: local thermal gradient, CPU/GPU parity, white-hot, alpha mask, opacity, palette motion and original material passed.\n";
}
