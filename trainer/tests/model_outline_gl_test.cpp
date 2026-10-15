#include <cassert>
#include <cstdlib>
#include <iostream>
#include "features/visual/native_model_outline.hpp"
using namespace pztrainer::features::visual;
int main() {
    _set_error_mode(_OUT_TO_STDERR); _set_abort_behavior(0,_WRITE_ABORT_MSG|_CALL_REPORTFAULT);
    HWND window=CreateWindowW(L"STATIC",L"Model outline test",WS_OVERLAPPED,0,0,64,64,nullptr,nullptr,GetModuleHandleW(nullptr),nullptr);
    assert(window); HDC dc=GetDC(window);
    PIXELFORMATDESCRIPTOR format{sizeof(format),1};
    format.dwFlags=PFD_DRAW_TO_WINDOW|PFD_SUPPORT_OPENGL|PFD_DOUBLEBUFFER;
    format.iPixelType=PFD_TYPE_RGBA; format.cColorBits=32; format.cDepthBits=24;
    int pixel=ChoosePixelFormat(dc,&format); assert(pixel && SetPixelFormat(dc,pixel,&format));
    HGLRC context=wglCreateContext(dc); assert(context && wglMakeCurrent(dc,context));
    auto gen=reinterpret_cast<void(APIENTRY*)(GLsizei,GLuint*)>(wglGetProcAddress("glGenFramebuffers"));
    auto bind=reinterpret_cast<void(APIENTRY*)(GLenum,GLuint)>(wglGetProcAddress("glBindFramebuffer"));
    auto attach=reinterpret_cast<void(APIENTRY*)(GLenum,GLenum,GLenum,GLuint,GLint)>(wglGetProcAddress("glFramebufferTexture2D"));
    GLuint output=0,target=0; gen(1,&output); bind(0x8D40,output); glGenTextures(1,&target); glBindTexture(GL_TEXTURE_2D,target);
    glTexImage2D(GL_TEXTURE_2D,0,GL_RGBA,64,64,0,GL_RGBA,GL_UNSIGNED_BYTE,nullptr);
    attach(0x8D40,0x8CE0,GL_TEXTURE_2D,target,0); glDrawBuffer(0x8CE0); glReadBuffer(0x8CE0);
    NativeModelOutline outline;
    glViewport(0,0,64,64); glClearColor(0,0,0,1); glClear(GL_COLOR_BUFFER_BIT);
    assert(outline.Begin(64,64));
    glEnable(GL_SCISSOR_TEST); glScissor(16,16,32,32); glClearColor(1,1,1,1); glClear(GL_COLOR_BUFFER_BIT);
    glDisable(GL_SCISSOR_TEST); outline.End();
    assert(outline.Composite(ImVec4(0,1,0,1)));
    unsigned char inside[4]{},edge[4]{},outside[4]{};
    glReadPixels(32,32,1,1,GL_RGBA,GL_UNSIGNED_BYTE,inside);
    glReadPixels(14,32,1,1,GL_RGBA,GL_UNSIGNED_BYTE,edge);
    glReadPixels(8,32,1,1,GL_RGBA,GL_UNSIGNED_BYTE,outside);
    assert(inside[0]==0 && inside[1]==0 && inside[2]==0);
    assert(edge[1]>20 && edge[0]==0);
    assert(outside[0]==0 && outside[1]==0 && outside[2]==0);
    GLint framebuffer=0; glGetIntegerv(0x8CA6,&framebuffer); assert(framebuffer==static_cast<GLint>(output));
    assert(glGetError()==GL_NO_ERROR);
    // Turning the edge off must leave an existing red body fill untouched.
    glClearColor(0.6f,0,0,1); glClear(GL_COLOR_BUFFER_BIT);
    assert(outline.Composite(ImVec4(0,1,0,0)));
    glReadPixels(32,32,1,1,GL_RGBA,GL_UNSIGNED_BYTE,inside);
    assert(inside[0]>=150 && inside[1]==0 && inside[2]==0);
    glClearColor(0,0,0,1); glClear(GL_COLOR_BUFFER_BIT);
    glEnable(GL_SCISSOR_TEST); glScissor(16,16,32,32); glClearColor(0.6f,0,0,1); glClear(GL_COLOR_BUFFER_BIT);
    glDisable(GL_SCISSOR_TEST);
    assert(outline.Composite(ImVec4(0,1,0,1)));
    glReadPixels(32,32,1,1,GL_RGBA,GL_UNSIGNED_BYTE,inside);
    glReadPixels(14,32,1,1,GL_RGBA,GL_UNSIGNED_BYTE,edge);
    assert(inside[0]>=150 && inside[1]==0 && edge[1]>20 && edge[0]==0);
    NativeModelOutline self,fill;
    assert(self.Begin(64,64));
    glEnable(GL_SCISSOR_TEST);glScissor(28,0,8,64);glClearColor(1,1,1,1);glClear(GL_COLOR_BUFFER_BIT);
    glDisable(GL_SCISSOR_TEST);self.End();
    glClearColor(0,0,0,1);glClear(GL_COLOR_BUFFER_BIT);
    assert(fill.Begin(64,64));
    glEnable(GL_SCISSOR_TEST);glScissor(16,16,32,32);glClearColor(1,0,0,0.6f);glClear(GL_COLOR_BUFFER_BIT);
    glDisable(GL_SCISSOR_TEST);
    // The game's outline shader outputs alpha=0 outside a clothing cutout; it does not discard.
    glColor4f(0,0,0,0);glRectf(-1,-1,1,1);
    fill.End();
    assert(fill.Composite(ImVec4(0,0,0,0),true,self.Texture()));
    glReadPixels(32,32,1,1,GL_RGBA,GL_UNSIGNED_BYTE,inside);
    glReadPixels(22,32,1,1,GL_RGBA,GL_UNSIGNED_BYTE,outside);
    assert(inside[0]==0 && inside[1]==0 && outside[0]>=150 && outside[0]<=156);
    assert(outline.Composite(ImVec4(0,1,0,1),false,self.Texture()));
    glReadPixels(32,14,1,1,GL_RGBA,GL_UNSIGNED_BYTE,inside);
    glReadPixels(22,14,1,1,GL_RGBA,GL_UNSIGNED_BYTE,edge);
    assert(inside[1]==0 && edge[1]>20);
    std::cout<<"Native GPU: independent rim/fill, exact opacity, local-player pixels excluded from both layers, framebuffer/state restored\n";
    wglMakeCurrent(nullptr,nullptr); wglDeleteContext(context); ReleaseDC(window,dc); DestroyWindow(window);
}
