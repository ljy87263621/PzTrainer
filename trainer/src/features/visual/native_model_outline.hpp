#pragma once
#include <Windows.h>
#include <GL/gl.h>
#include <imgui.h>
#include <algorithm>
#include <array>
#include <cstdint>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include "features/visual/model_composite_shader.hpp"

namespace pztrainer::features::visual {
class NativeModelOutline {
    using Gen=void(APIENTRY*)(GLsizei,GLuint*);
    using Bind=void(APIENTRY*)(GLenum,GLuint);
    using AttachTexture=void(APIENTRY*)(GLenum,GLenum,GLenum,GLuint,GLint);
    using Status=GLenum(APIENTRY*)(GLenum);
    using CreateShader=GLuint(APIENTRY*)(GLenum);
    using CreateProgram=GLuint(APIENTRY*)();
    using Source=void(APIENTRY*)(GLuint,GLsizei,const char* const*,const GLint*);
    using Action=void(APIENTRY*)(GLuint);
    using Attach=void(APIENTRY*)(GLuint,GLuint);
    using Query=void(APIENTRY*)(GLuint,GLenum,GLint*);
    using Log=void(APIENTRY*)(GLuint,GLsizei,GLsizei*,char*);
    using Location=GLint(APIENTRY*)(GLuint,const char*);
    using Uniform1=void(APIENTRY*)(GLint,GLint);
    using Uniform2=void(APIENTRY*)(GLint,GLfloat,GLfloat);
    using Uniform3=void(APIENTRY*)(GLint,GLfloat,GLfloat,GLfloat);
    using Uniform4=void(APIENTRY*)(GLint,GLfloat,GLfloat,GLfloat,GLfloat);
    using Active=void(APIENTRY*)(GLenum);
    using Blend=void(APIENTRY*)(GLenum,GLenum,GLenum,GLenum);
    using Equation=void(APIENTRY*)(GLenum,GLenum);
    using QueryIndex=void(APIENTRY*)(GLenum,GLuint,GLint*);
    Gen gen_fbo_=nullptr,gen_vao_=nullptr;
    Bind bind_fbo_=nullptr,bind_sampler_=nullptr;
    Action bind_vao_=nullptr,use_=nullptr,compile_=nullptr,link_=nullptr,delete_shader_=nullptr,delete_program_=nullptr;
    AttachTexture attach_texture_=nullptr;
    Status status_=nullptr;
    CreateShader shader_=nullptr;
    CreateProgram program_=nullptr;
    Source source_=nullptr;
    Attach attach_=nullptr;
    Query shader_iv_=nullptr,program_iv_=nullptr;
    Log shader_log_=nullptr,program_log_=nullptr;
    Location location_=nullptr;
    Uniform1 i1_=nullptr;
    Uniform2 f2_=nullptr;
    Uniform3 f3_=nullptr;
    Uniform4 f4_=nullptr;
    Active active_=nullptr;
    Blend blend_=nullptr;
    Equation equation_=nullptr;
    QueryIndex indexed_=nullptr;
    struct Context {
        GLuint program=0,vao=0,fbo=0,texture=0;
        int width=0,height=0;
        GLint mask=-1,fill=-1,enabled=-1,edge=-1,pixel=-1,panini=-1,tan=-1;
        GLint straight=-1,exclude=-1,exclude_enabled=-1;
        std::string failure;
    };
    std::unordered_map<HGLRC,Context> contexts_;
    Context* current_=nullptr;
    GLint saved_fbo_=0;
    std::array<GLint,4> saved_viewport_{};
    std::array<GLfloat,4> saved_clear_{};
    std::array<GLboolean,4> saved_mask_{};
    GLboolean saved_blend_=GL_FALSE;
    GLboolean saved_alpha_=GL_FALSE;
    GLint saved_alpha_func_=GL_ALWAYS;
    GLfloat saved_alpha_ref_=0;
    bool drawing_=false;
    bool initialized_=false;
    template<class T> static T Function(const char* name) {
        auto value=wglGetProcAddress(name);
        const auto raw=reinterpret_cast<std::uintptr_t>(value);
        if(!value || raw<=3 || raw==static_cast<std::uintptr_t>(-1))throw std::runtime_error(std::string("Missing GL function: ")+name);
        return reinterpret_cast<T>(value);
    }
    void Initialize() {
        if(initialized_)return;
        gen_fbo_=Function<Gen>("glGenFramebuffers"); gen_vao_=Function<Gen>("glGenVertexArrays");
        bind_fbo_=Function<Bind>("glBindFramebuffer"); bind_sampler_=Function<Bind>("glBindSampler");
        bind_vao_=Function<Action>("glBindVertexArray"); use_=Function<Action>("glUseProgram");
        attach_texture_=Function<AttachTexture>("glFramebufferTexture2D"); status_=Function<Status>("glCheckFramebufferStatus");
        shader_=Function<CreateShader>("glCreateShader"); program_=Function<CreateProgram>("glCreateProgram");
        source_=Function<Source>("glShaderSource"); compile_=Function<Action>("glCompileShader"); link_=Function<Action>("glLinkProgram");
        delete_shader_=Function<Action>("glDeleteShader"); delete_program_=Function<Action>("glDeleteProgram");
        attach_=Function<Attach>("glAttachShader"); shader_iv_=Function<Query>("glGetShaderiv"); program_iv_=Function<Query>("glGetProgramiv");
        shader_log_=Function<Log>("glGetShaderInfoLog"); program_log_=Function<Log>("glGetProgramInfoLog");
        location_=Function<Location>("glGetUniformLocation"); i1_=Function<Uniform1>("glUniform1i");
        f2_=Function<Uniform2>("glUniform2f"); f3_=Function<Uniform3>("glUniform3f"); f4_=Function<Uniform4>("glUniform4f");
        active_=Function<Active>("glActiveTexture"); blend_=Function<Blend>("glBlendFuncSeparate");
        equation_=Function<Equation>("glBlendEquationSeparate"); indexed_=Function<QueryIndex>("glGetIntegeri_v");
        initialized_=true;
    }
    GLuint Compile(GLenum type,const char* text) {
        GLuint shader=shader_(type); source_(shader,1,&text,nullptr); compile_(shader);
        GLint valid=0; shader_iv_(shader,0x8B81,&valid);
        if(!valid) {
            GLint size=0; shader_iv_(shader,0x8B84,&size);
            std::string message(static_cast<std::size_t>(std::max(1,size)), '\0'); shader_log_(shader,size,nullptr,message.data());
            delete_shader_(shader); throw std::runtime_error(message);
        }
        return shader;
    }
    void Prepare(Context& c) {
        if(c.program)return;
        if(!c.failure.empty())throw std::runtime_error(c.failure);
        GLuint vs=0,fs=0,id=0;
        try {
            vs=Compile(0x8B31,kModelCompositeVertex); fs=Compile(0x8B30,kModelCompositeFragment);
            id=program_(); attach_(id,vs); attach_(id,fs); link_(id);
            GLint valid=0; program_iv_(id,0x8B82,&valid);
            if(!valid) {
                GLint size=0; program_iv_(id,0x8B84,&size);
                std::string message(static_cast<std::size_t>(std::max(1,size)),'\0'); program_log_(id,size,nullptr,message.data());
                throw std::runtime_error(message);
            }
            c.program=id; gen_vao_(1,&c.vao); gen_fbo_(1,&c.fbo); glGenTextures(1,&c.texture);
            c.mask=location_(id,"maskTexture"); c.fill=location_(id,"fillTexture"); c.enabled=location_(id,"fillEnabled");
            c.edge=location_(id,"edgeColour"); c.pixel=location_(id,"pixelSize");
            c.panini=location_(id,"panini"); c.tan=location_(id,"paniniTan");
            c.straight=location_(id,"fillStraight");c.exclude=location_(id,"excludeTexture");
            c.exclude_enabled=location_(id,"excludeEnabled");
        } catch(const std::exception& failure) {
            if(id)delete_program_(id); c.program=0; c.failure=failure.what();
            if(vs)delete_shader_(vs); if(fs)delete_shader_(fs); throw;
        }
        delete_shader_(vs); delete_shader_(fs);
    }
public:
    std::string error;
    bool Begin(int width,int height) {
        if(drawing_ || width<=0 || height<=0 || !wglGetCurrentContext())return false;
        GLint old_texture=0; glGetIntegerv(GL_TEXTURE_BINDING_2D,&old_texture);
        glGetIntegerv(0x8CA6,&saved_fbo_); glGetIntegerv(GL_VIEWPORT,saved_viewport_.data());
        glGetFloatv(GL_COLOR_CLEAR_VALUE,saved_clear_.data()); glGetBooleanv(GL_COLOR_WRITEMASK,saved_mask_.data());
        saved_blend_=glIsEnabled(GL_BLEND);
        saved_alpha_=glIsEnabled(GL_ALPHA_TEST);glGetIntegerv(GL_ALPHA_TEST_FUNC,&saved_alpha_func_);
        glGetFloatv(GL_ALPHA_TEST_REF,&saved_alpha_ref_);
        try {
            Initialize(); current_=&contexts_[wglGetCurrentContext()]; Prepare(*current_);
            Context& c=*current_;
            bind_fbo_(0x8CA9,c.fbo);
            if(c.width!=width || c.height!=height) {
                glBindTexture(GL_TEXTURE_2D,c.texture);
                glTexImage2D(GL_TEXTURE_2D,0,GL_RGBA,width,height,0,GL_RGBA,GL_UNSIGNED_BYTE,nullptr);
                glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_LINEAR); glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_LINEAR);
                glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_S,0x812F); glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_T,0x812F);
                attach_texture_(0x8CA9,0x8CE0,GL_TEXTURE_2D,c.texture,0); glDrawBuffer(0x8CE0);
                if(status_(0x8CA9)!=0x8CD5)throw std::runtime_error("Model outline framebuffer incomplete");
                c.width=width; c.height=height;
            }
            glBindTexture(GL_TEXTURE_2D,static_cast<GLuint>(old_texture));
            glViewport(0,0,width,height); glColorMask(GL_TRUE,GL_TRUE,GL_TRUE,GL_TRUE);
            glClearColor(0,0,0,0); glClear(GL_COLOR_BUFFER_BIT); glDisable(GL_BLEND);
            // The game shader writes zero-alpha clothing pixels instead of discarding them.
            glEnable(GL_ALPHA_TEST);glAlphaFunc(GL_GREATER,0.0f);
            drawing_=true; error.clear(); return true;
        } catch(const std::exception& failure) {
            error=failure.what(); if(bind_fbo_)bind_fbo_(0x8CA9,static_cast<GLuint>(saved_fbo_));
            glBindTexture(GL_TEXTURE_2D,static_cast<GLuint>(old_texture)); return false;
        }
    }
    void End() {
        if(!drawing_)return;
        bind_fbo_(0x8CA9,static_cast<GLuint>(saved_fbo_));
        glViewport(saved_viewport_[0],saved_viewport_[1],saved_viewport_[2],saved_viewport_[3]);
        glClearColor(saved_clear_[0],saved_clear_[1],saved_clear_[2],saved_clear_[3]);
        glColorMask(saved_mask_[0],saved_mask_[1],saved_mask_[2],saved_mask_[3]);
        if(saved_blend_)glEnable(GL_BLEND); else glDisable(GL_BLEND);
        glAlphaFunc(static_cast<GLenum>(saved_alpha_func_),saved_alpha_ref_);
        if(saved_alpha_)glEnable(GL_ALPHA_TEST);else glDisable(GL_ALPHA_TEST);
        drawing_=false;
    }
    GLuint Texture() const { return !drawing_ && current_?current_->texture:0; }
    bool Composite(const ImVec4& edge,bool fill=false,GLuint exclusion=0) {
        if(drawing_ || !current_ || !current_->program)return false;
        if(edge.w<=0 && !fill)return true;
        GLint program=0,vao=0,unit=0,texture=0,sampler=0;
        GLint exclude_texture=0,exclude_sampler=0;
        GLint src_rgb=0,dst_rgb=0,src_alpha=0,dst_alpha=0,eq_rgb=0,eq_alpha=0;
        glGetIntegerv(0x8B8D,&program); glGetIntegerv(0x85B5,&vao); glGetIntegerv(0x84E0,&unit);
        active_(0x84C0); glGetIntegerv(GL_TEXTURE_BINDING_2D,&texture); indexed_(0x8919,0,&sampler);
        if(exclusion) {
            active_(0x84C1);glGetIntegerv(GL_TEXTURE_BINDING_2D,&exclude_texture);indexed_(0x8919,1,&exclude_sampler);
            bind_sampler_(1,0);glBindTexture(GL_TEXTURE_2D,exclusion);active_(0x84C0);
        }
        glGetIntegerv(0x80C9,&src_rgb); glGetIntegerv(0x80C8,&dst_rgb); glGetIntegerv(0x80CB,&src_alpha); glGetIntegerv(0x80CA,&dst_alpha);
        glGetIntegerv(0x8009,&eq_rgb); glGetIntegerv(0x883D,&eq_alpha);
        const GLboolean blend=glIsEnabled(GL_BLEND),depth=glIsEnabled(GL_DEPTH_TEST),cull=glIsEnabled(GL_CULL_FACE),scissor=glIsEnabled(GL_SCISSOR_TEST);
        std::array<GLint,4> viewport{}; glGetIntegerv(GL_VIEWPORT,viewport.data());
        Context& c=*current_; use_(c.program); bind_vao_(c.vao); bind_sampler_(0,0);
        glBindTexture(GL_TEXTURE_2D,c.texture); glViewport(0,0,c.width,c.height);
        i1_(c.mask,0); i1_(c.fill,0); i1_(c.enabled,fill?1:0);i1_(c.straight,fill?1:0);
        i1_(c.exclude,1);i1_(c.exclude_enabled,exclusion?1:0);f4_(c.edge,edge.x,edge.y,edge.z,edge.w);
        f2_(c.pixel,1.0f/c.width,1.0f/c.height); f3_(c.panini,0,1,1); f2_(c.tan,1,1);
        glDisable(GL_DEPTH_TEST); glDisable(GL_CULL_FACE); glDisable(GL_SCISSOR_TEST); glEnable(GL_BLEND);
        equation_(0x8006,0x8006); blend_(GL_ONE,GL_ONE_MINUS_SRC_ALPHA,GL_ONE,GL_ONE_MINUS_SRC_ALPHA);
        glDrawArrays(GL_TRIANGLES,0,3);
        use_(static_cast<GLuint>(program)); bind_vao_(static_cast<GLuint>(vao)); bind_sampler_(0,static_cast<GLuint>(sampler));
        glBindTexture(GL_TEXTURE_2D,static_cast<GLuint>(texture));
        if(exclusion) {
            active_(0x84C1);bind_sampler_(1,static_cast<GLuint>(exclude_sampler));glBindTexture(GL_TEXTURE_2D,static_cast<GLuint>(exclude_texture));
        }
        active_(static_cast<GLenum>(unit));
        blend_(src_rgb,dst_rgb,src_alpha,dst_alpha); equation_(eq_rgb,eq_alpha);
        glViewport(viewport[0],viewport[1],viewport[2],viewport[3]);
        if(blend)glEnable(GL_BLEND);else glDisable(GL_BLEND); if(depth)glEnable(GL_DEPTH_TEST);else glDisable(GL_DEPTH_TEST);
        if(cull)glEnable(GL_CULL_FACE);else glDisable(GL_CULL_FACE); if(scissor)glEnable(GL_SCISSOR_TEST);else glDisable(GL_SCISSOR_TEST);
        return true;
    }
};
}
