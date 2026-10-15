#pragma once
#include <Windows.h>
#include <GL/gl.h>
#include <unordered_map>
#include <cstdint>
#include <array>
#include "features/visual/thermal_material.hpp"

namespace pztrainer::features::visual {
// All objects are created lazily on the capture context. Nothing modifies or
// relinks the game's shader, and replay's existing state guard restores binding.
class NativeModelMaterial {
public:
    struct Program {
        GLuint id = 0;
        GLint projection=-1, palette=-1, transform=-1, scale=-1, colour=-1, texture=-1, mode=-1;
        std::string failure;
        GLuint retainer = 0;
        std::array<GLuint,8> shaders{};
        GLsizei shader_count = 0;
    };
    const Program* Get(GLuint original) {
        if (!Initialize()) return nullptr;
        auto& programs = contexts_[wglGetCurrentContext()];
        GLuint shaders[8]{}; GLsizei count=0;
        attached_(original,8,&count,shaders);
        auto found = programs.find(original);
        if(found != programs.end() && (found->second.shader_count != count ||
            !std::equal(shaders,shaders+count,found->second.shaders.begin()))) {
            if(found->second.id) delete_program_(found->second.id);
            if(found->second.retainer) delete_program_(found->second.retainer);
            programs.erase(found); found=programs.end();
        }
        if (found != programs.end()) {
            error=found->second.failure;
            return found->second.id ? &found->second : nullptr;
        }
        Program result;
        result.shader_count=count;
        std::copy(shaders,shaders+count,result.shaders.begin());
        try {
            std::string vertex,fragment;
            for (int i=0;i<count;++i) {
                GLint type=0,length=0;
                shader_iv_(shaders[i],0x8B4F,&type); // SHADER_TYPE
                shader_iv_(shaders[i],0x8B88,&length); // SHADER_SOURCE_LENGTH
                std::string source(static_cast<std::size_t>(std::max(1,length)), '\0');
                GLsizei written=0; shader_source_(shaders[i],length,&written,source.data()); source.resize(written);
                if (type==0x8B31) vertex=source;
                else if (type==0x8B30) fragment=source;
            }
            auto sources=BuildThermalSources(vertex,fragment,"vertex");
            GLuint vs=Compile(0x8B31,sources.vertex), fs=0;
            try { fs=Compile(0x8B30,sources.fragment); }
            catch (...) { delete_shader_(vs); throw; }
            result.id=create_program_();
            attach_(result.id,vs); attach_(result.id,fs); link_(result.id);
            delete_shader_(vs); delete_shader_(fs);
            GLint linked=0; program_iv_(result.id,0x8B82,&linked);
            if (!linked) {
                const std::string log=Log(result.id,false); delete_program_(result.id); result.id=0;
                throw std::runtime_error(log);
            }
            result.projection=location_(result.id,"ModelViewProjection");
            result.palette=location_(result.id,"MatrixPalette[0]");
            result.transform=location_(result.id,"transform");
            result.scale=location_(result.id,"FinalScale");
            result.colour=location_(result.id,"u_color");
            result.texture=location_(result.id,"Texture");
            result.mode=location_(result.id,"pzsaThermalMode");
            result.retainer=create_program_();
            for(int i=0;i<count;i++) attach_(result.retainer,shaders[i]);
            error.clear();
        } catch (const std::exception& failure) { error=failure.what(); result.failure=error; }
        auto inserted=programs.emplace(original,result);
        return result.id ? &inserted.first->second : nullptr;
    }
    std::string error;
private:
    template<class T> static T Resolve(const char* name) {
        auto address=wglGetProcAddress(name);
        auto value=reinterpret_cast<std::uintptr_t>(address);
        return value>3 && value!=static_cast<std::uintptr_t>(-1) ? reinterpret_cast<T>(address) : nullptr;
    }
    using CreateShader=GLuint(APIENTRY*)(GLenum);
    using CreateProgram=GLuint(APIENTRY*)();
    using ShaderSource=void(APIENTRY*)(GLuint,GLsizei,const char* const*,const GLint*);
    using Query=void(APIENTRY*)(GLuint,GLenum,GLint*);
    using GetSource=void(APIENTRY*)(GLuint,GLsizei,GLsizei*,char*);
    using Attached=void(APIENTRY*)(GLuint,GLsizei,GLsizei*,GLuint*);
    using Action=void(APIENTRY*)(GLuint);
    using Attach=void(APIENTRY*)(GLuint,GLuint);
    using Location=GLint(APIENTRY*)(GLuint,const char*);
    CreateShader create_shader_=nullptr;
    CreateProgram create_program_=nullptr;
    ShaderSource source_=nullptr;
    Query shader_iv_=nullptr,program_iv_=nullptr;
    GetSource shader_source_=nullptr,shader_log_=nullptr,program_log_=nullptr;
    Attached attached_=nullptr;
    Action compile_=nullptr,link_=nullptr,delete_shader_=nullptr,delete_program_=nullptr;
    Attach attach_=nullptr;
    Location location_=nullptr;
    std::unordered_map<HGLRC,std::unordered_map<GLuint,Program>> contexts_;
    bool Initialize() {
        if (create_shader_) return true;
        create_shader_=Resolve<CreateShader>("glCreateShader"); create_program_=Resolve<CreateProgram>("glCreateProgram");
        source_=Resolve<ShaderSource>("glShaderSource"); shader_iv_=Resolve<Query>("glGetShaderiv");
        program_iv_=Resolve<Query>("glGetProgramiv"); shader_source_=Resolve<GetSource>("glGetShaderSource");
        shader_log_=Resolve<GetSource>("glGetShaderInfoLog"); program_log_=Resolve<GetSource>("glGetProgramInfoLog");
        attached_=Resolve<Attached>("glGetAttachedShaders"); compile_=Resolve<Action>("glCompileShader");
        link_=Resolve<Action>("glLinkProgram"); delete_shader_=Resolve<Action>("glDeleteShader");
        delete_program_=Resolve<Action>("glDeleteProgram"); attach_=Resolve<Attach>("glAttachShader");
        location_=Resolve<Location>("glGetUniformLocation");
        if (create_shader_ && create_program_ && source_ && shader_iv_ && program_iv_ && shader_source_ && shader_log_ && program_log_ && attached_ && compile_ && link_ && delete_shader_ && delete_program_ && attach_ && location_) return true;
        create_shader_=nullptr; error="Thermal shader OpenGL functions unavailable"; return false;
    }
    std::string Log(GLuint object,bool shader) {
        GLint length=0; (shader?shader_iv_:program_iv_)(object,0x8B84,&length);
        std::string log(static_cast<std::size_t>(std::max(1,length)), '\0');
        (shader?shader_log_:program_log_)(object,length,nullptr,log.data()); return log;
    }
    GLuint Compile(GLenum type,const std::string& source) {
        GLuint shader=create_shader_(type); const char* text=source.c_str(); source_(shader,1,&text,nullptr); compile_(shader);
        GLint ok=0; shader_iv_(shader,0x8B81,&ok);
        if (!ok) { std::string log=Log(shader,true); delete_shader_(shader); throw std::runtime_error(log); }
        return shader;
    }
};
}
