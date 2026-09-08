// GPL-3.0. Portable OpenGL 2.1 rasterizer for the recovered nX scenes.
#include "renderer.h"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <GLFW/glfw3.h>
#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h"

namespace nxng {
namespace {
constexpr int clamp_edge=0x812f;
struct DrawFace {
    const Triangle* triangle;
    const Surface* surface;
    const Mesh* mesh;
    const Matrix* world;
    float alpha,depth;
    bool fog;
};
struct RenderLight {Vec3 position,direction,color;float range,cone,edge;int type;};
float saturate(float v) {return std::clamp(v,0.f,1.f);}
Vec3 illumination(Vec3 position,Vec3 normal,const Surface& surface,Vec3 ambient,
                  const std::vector<RenderLight>& lights) {
    if(surface.unlit||!(surface.flags&4))return {1,1,1};
    Vec3 color=ambient+Vec3{surface.luminosity,surface.luminosity,surface.luminosity};
    for(const auto& l:lights) {
        // nX's face normals point inward: ModelLight uses vertex minus light.
        Vec3 delta=position-l.position;float d=std::sqrt(dot(delta,delta));
        Vec3 direction=l.type==0?l.direction:normalized(delta);
        float strength=std::max(0.f,dot(normal,direction))*surface.diffuse;
        if(l.type!=0&&l.range>0)strength*=std::max(0.f,1-d/l.range);
        if(l.type==2) {
            float angle=std::acos(std::clamp(dot(direction,l.direction),-1.f,1.f))*57.2957795f;
            strength*=l.edge>0?saturate((l.cone+l.edge-angle)/l.edge):(angle<l.cone?1.f:0.f);
        }
        color=color+l.color*strength;
    }
    return {saturate(color.x),saturate(color.y),saturate(color.z)};
}
void quad(float x,float y,float w,float h) {
    glBegin(GL_QUADS);
    glTexCoord2f(0,0);glVertex2f(x,y);
    glTexCoord2f(1,0);glVertex2f(x+w,y);
    glTexCoord2f(1,1);glVertex2f(x+w,y+h);
    glTexCoord2f(0,1);glVertex2f(x,y+h);glEnd();
}
}
struct Renderer::Impl {
    GLFWwindow* window=nullptr;
    std::unordered_map<std::string,GLuint> textures;
    GLuint spark=0,flare=0;
    GLuint texture(const std::string& path,bool chroma=false,bool negative=false,bool alpha_only=false) {
        if(path.empty())return 0;
        auto key=path+(chroma?"|chroma":"")+(negative?"|negative":"")+(alpha_only?"|alpha":"");
        auto it=textures.find(key);if(it!=textures.end())return it->second;
        auto data=read_file(fs::u8path(path));int w,h,c;
        auto* pixels=stbi_load_from_memory(data.data(),static_cast<int>(data.size()),&w,&h,&c,4);
        if(!pixels)throw std::runtime_error("Cannot decode image: "+path);
        for(int i=0;i<w*h;++i) {
            auto* p=pixels+i*4;
            if(negative){p[0]=255-p[0];p[1]=255-p[1];p[2]=255-p[2];}
            if(chroma)p[3]=std::max({p[0],p[1],p[2]})<16?0:255;
            if(alpha_only) {p[3]=static_cast<unsigned char>(255-(p[0]*.299f+p[1]*.587f+p[2]*.114f));p[0]=p[1]=p[2]=255;}
        }
        GLuint id;glGenTextures(1,&id);glBindTexture(GL_TEXTURE_2D,id);
        glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_S,clamp_edge);
        glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_T,clamp_edge);
        glTexImage2D(GL_TEXTURE_2D,0,GL_RGBA,w,h,0,GL_RGBA,GL_UNSIGNED_BYTE,pixels);
        stbi_image_free(pixels);textures.emplace(key,id);return id;
    }
};
Renderer::Renderer(int width,int height,bool hidden,bool fullscreen):impl(std::make_unique<Impl>()) {
    glfwSetErrorCallback([](int,const char* error){std::cerr<<"GLFW: "<<error<<'\n';});
    if(!glfwInit())throw std::runtime_error("GLFW initialization failed");
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR,2);glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR,1);
    glfwWindowHint(GLFW_VISIBLE,hidden?GLFW_FALSE:GLFW_TRUE);
    if(fullscreen) {
        auto* mode=glfwGetVideoMode(glfwGetPrimaryMonitor());width=mode->width;height=mode->height;
    }
    impl->window=glfwCreateWindow(width,height,"FreeStyle - Condense / Syndrome",fullscreen?glfwGetPrimaryMonitor():nullptr,nullptr);
    if(!impl->window){glfwTerminate();throw std::runtime_error("Cannot create OpenGL window");}
    glfwMakeContextCurrent(impl->window);glfwSwapInterval(hidden?0:1);
    stbi_set_flip_vertically_on_load(1);glPixelStorei(GL_PACK_ALIGNMENT,1);
    std::cout<<"OpenGL: "<<glGetString(GL_RENDERER)<<'\n';
}
Renderer::~Renderer() {
    if(impl->window) {
        glfwMakeContextCurrent(impl->window);
        for(const auto& p:impl->textures)glDeleteTextures(1,&p.second);
        glfwDestroyWindow(impl->window);
    }
    glfwTerminate();
}
GLFWwindow* Renderer::window() const {return impl->window;}
void Renderer::preload(const std::vector<Scene>& scenes,Assets& assets) {
    for(const auto& scene:scenes) {
        impl->texture(scene.background);
        for(const auto& obj:scene.objects)if(obj.mesh)for(const auto& s:obj.mesh->surfaces) {
            impl->texture(s.texture_path,s.chroma,(s.texture_flags&16)!=0,s.alpha_only);
            impl->texture(s.reflection_path);
        }
    }
    impl->spark=impl->texture(assets.resolve("D:/Devellop/nX_Pics/Spark.jpg").u8string());
    impl->flare=impl->texture(assets.resolve("D:/Devellop/nX_Pics/Flare.jpg").u8string());
}
void Renderer::draw(const Scene& scene,double frame,int width,int height) {
    glDisable(GL_SCISSOR_TEST);glDepthMask(GL_TRUE);glClearColor(0,0,0,1);
    glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT);
    // Preserve the original 640x480 canvas; CustomSize defines cinematic bars within it.
    float scale=std::min(width/640.f,height/480.f);
    int cw=static_cast<int>(640*scale),ch=static_cast<int>(480*scale),cx=(width-cw)/2,cy=(height-ch)/2;
    int vw=static_cast<int>(std::min(640,scene.width)*scale);
    int vh=static_cast<int>(std::min(480,scene.height)*scale);
    int vx=cx+(cw-vw)/2,vy=cy+(ch-vh)/2;
    if(!scene.background.empty()) {
        glEnable(GL_SCISSOR_TEST);glScissor(cx,cy,cw,ch);
        glClearColor(scene.backdrop.x,scene.backdrop.y,scene.backdrop.z,1);glClear(GL_COLOR_BUFFER_BIT);
    }
    glViewport(vx,vy,vw,vh);glEnable(GL_SCISSOR_TEST);glScissor(vx,vy,vw,vh);
    glClearColor(scene.backdrop.x,scene.backdrop.y,scene.backdrop.z,1);glClear(GL_COLOR_BUFFER_BIT);
    glDisable(GL_DEPTH_TEST);glDisable(GL_BLEND);glDisable(GL_CULL_FACE);glDisable(GL_FOG);glDisable(GL_ALPHA_TEST);
    glMatrixMode(GL_PROJECTION);glLoadIdentity();glOrtho(0,1,0,1,-1,1);
    glMatrixMode(GL_MODELVIEW);glLoadIdentity();
    if(!scene.background.empty()) {
        glEnable(GL_TEXTURE_2D);glBindTexture(GL_TEXTURE_2D,impl->texture(scene.background));glColor4f(1,1,1,1);quad(0,0,1,1);
    }
    auto worlds=world_matrices(scene,frame);auto view=camera_matrix(scene.camera.evaluate(frame));
    std::vector<RenderLight> lights;
    for(const auto& l:scene.lights) {
        auto w=object_matrix(l.motion.evaluate(frame));
        if(l.parent>0&&static_cast<std::size_t>(l.parent)<=worlds.size())w=worlds[l.parent-1]*w;
        lights.push_back({w.point({}),normalized(w.vector({0,0,1})),l.color*l.intensity.scalar(frame),l.range,l.cone,l.edge,l.type});
    }
    Vec3 ambient=scene.ambient_color*scene.ambient.scalar(frame);
    float zoom=std::max(.001f,scene.zoom.scalar(frame));
    double nearz=.01,farz=200000.;double top=nearz/zoom;
    glMatrixMode(GL_PROJECTION);glLoadIdentity();glFrustum(-top*vw/vh,top*vw/vh,-top,top,nearz,farz);
    // Convert nX's Y-down, Z-forward camera space to OpenGL camera space.
    glScalef(1,-1,-1);glMultMatrixf(view.m.data());
    glMatrixMode(GL_MODELVIEW);glLoadIdentity();glEnable(GL_DEPTH_TEST);glDepthFunc(GL_LEQUAL);
    glFrontFace(GL_CW);glCullFace(GL_BACK);
    std::vector<DrawFace> opaque,transparent;
    for(std::size_t i=0;i<scene.objects.size();++i) {
        const auto& o=scene.objects[i];if(!o.mesh||o.name.find("[SPARK_")!=std::string::npos)continue;
        float dissolve=saturate(o.dissolve.scalar(frame));
        for(const auto& t:o.mesh->triangles) {
            if(t.surface_index<0)continue;const auto& s=o.mesh->surfaces[t.surface_index];
            float alpha=(1-dissolve)*(1-s.transparency);if(alpha<=.001f)continue;
            Vec3 center=(o.mesh->vertices[t.i0]+o.mesh->vertices[t.i1]+o.mesh->vertices[t.i2])*(1/3.f);
            DrawFace f{&t,&s,o.mesh.get(),&worlds[i],alpha,view.point(worlds[i].point(center)).z,!o.unaffected_by_fog};
            ((alpha<.999f||s.additive||s.alpha_only)?transparent:opaque).push_back(f);
        }
    }
    std::stable_sort(transparent.begin(),transparent.end(),[](const DrawFace& a,const DrawFace& b){return a.depth>b.depth;});
    auto draw_faces=[&](const std::vector<DrawFace>& faces,bool blend) {
        glDepthMask(blend?GL_FALSE:GL_TRUE);
        for(const auto& f:faces) {
            const auto& s=*f.surface;const auto& tri=*f.triangle;
            if(s.flags&256)glDisable(GL_CULL_FACE);else glEnable(GL_CULL_FACE);
            if(blend||s.chroma) {glEnable(GL_BLEND);glBlendFunc(GL_SRC_ALPHA,s.additive?GL_ONE:GL_ONE_MINUS_SRC_ALPHA);}
            else glDisable(GL_BLEND);
            if(s.chroma){glEnable(GL_ALPHA_TEST);glAlphaFunc(GL_GREATER,.1f);}else glDisable(GL_ALPHA_TEST);
            // Original D3D path did not implement software-only scene fog.
            glDisable(GL_FOG);
            bool reflection=s.texture_path.empty()&&!s.reflection_path.empty();
            GLuint tex=reflection?impl->texture(s.reflection_path):impl->texture(s.texture_path,s.chroma,(s.texture_flags&16)!=0,s.alpha_only);
            if(tex) {
                glEnable(GL_TEXTURE_2D);glBindTexture(GL_TEXTURE_2D,tex);
                glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_S,s.wrap_u==2&&!reflection?GL_REPEAT:clamp_edge);
                glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_T,s.wrap_v==2&&!reflection?GL_REPEAT:clamp_edge);
            } else glDisable(GL_TEXTURE_2D);
            glBegin(GL_TRIANGLES);
            std::array<std::uint32_t,3> indices{tri.i0,tri.i1,tri.i2};
            for(int i=0;i<3;++i) {
                Vec3 p=f.world->point(f.mesh->vertices[indices[i]]);
                Vec3 n=normalized(f.world->vector(tri.normals[i]));
                auto color=illumination(p,n,s,ambient,lights);
                if(!tex||s.alpha_only) {color.x*=s.color_r/255.f;color.y*=s.color_g/255.f;color.z*=s.color_b/255.f;}
                glColor4f(color.x,color.y,color.z,f.alpha);
                Vec2 uv=tri.uv[i];
                if(reflection){auto vn=normalized(view.vector(n));uv={.5f+vn.x*.5f,.5f-vn.y*.5f};}
                else {uv.u+=s.velocity.x*static_cast<float>(frame);uv.v+=s.velocity.y*static_cast<float>(frame);}
                glTexCoord2f(uv.u,uv.v);glVertex3f(p.x,p.y,p.z);
            }
            glEnd();
        }
    };
    draw_faces(opaque,false);draw_faces(transparent,true);
    glDisable(GL_ALPHA_TEST);glDisable(GL_CULL_FACE);glDisable(GL_FOG);
    glEnable(GL_BLEND);glBlendFunc(GL_SRC_ALPHA,GL_ONE);glDepthMask(GL_FALSE);
    glEnable(GL_TEXTURE_2D);glBindTexture(GL_TEXTURE_2D,impl->spark);
    glMatrixMode(GL_PROJECTION);glLoadIdentity();glFrustum(-top*vw/vh,top*vw/vh,-top,top,nearz,farz);glScalef(1,-1,-1);
    auto sprite=[&](Vec3 world,float radius,Vec3 color,float alpha) {
        Vec3 p=view.point(world);if(p.z<=nearz)return;
        glColor4f(color.x,color.y,color.z,alpha);
        glBegin(GL_QUADS);
        glTexCoord2f(0,1);glVertex3f(p.x-radius,p.y-radius,p.z);
        glTexCoord2f(1,1);glVertex3f(p.x+radius,p.y-radius,p.z);
        glTexCoord2f(1,0);glVertex3f(p.x+radius,p.y+radius,p.z);
        glTexCoord2f(0,0);glVertex3f(p.x-radius,p.y+radius,p.z);glEnd();
    };
    for(std::size_t i=0;i<scene.objects.size();++i) {
        const auto& o=scene.objects[i];if(o.name.find("[SPARK_")==std::string::npos)continue;
        auto at=[&](double f) {
            Matrix w=object_matrix(o.motion.evaluate(f),o.pivot);int parent=o.parent;
            while(parent) {const auto& p=scene.objects[parent-1];w=object_matrix(p.motion.evaluate(f),p.pivot)*w;parent=p.parent;}
            return w.point({});
        };
        std::uint32_t seed=1;
        auto random=[&](){seed=seed*214013u+2531011u;return (seed>>16)&32767u;};
        auto v=o.motion.evaluate(frame);
        // Fixed seeds and analytical aging make seeks/captures independent of playback FPS.
        // Initial delays, lifetime and velocities follow nX's 150-particle emitter.
        for(int j=0;j<150;++j) {
            double delay=random()/80.;float rx=(static_cast<int>(random())-16383)/655360.f;
            float rz=(static_cast<int>(random())-16383)/655360.f;
            if(frame<delay)continue;float age=static_cast<float>(std::fmod(frame-delay,60.));
            Vec3 p=at(frame-age)+Vec3{rx*v[6]/3,.02f*v[7]/1.5f,rz*v[8]/3}*age;
            p.y+=.0005f*age*age*v[7]*scene.grid_size/30.f;
            sprite(p,.32f,{1,.85f,1},1);
        }
        sprite(worlds[i].point({}),.65f,{1,.9f,1},1);
    }
    glBindTexture(GL_TEXTURE_2D,impl->flare);
    for(std::size_t i=0;i<scene.lights.size();++i)if(scene.lights[i].flare>0)
        sprite(lights[i].position,scene.lights[i].flare*.5f,lights[i].color,1);
    glDepthMask(GL_TRUE);glDisable(GL_SCISSOR_TEST);
}
void Renderer::capture(const fs::path& file,int width,int height) {
    std::vector<unsigned char> pixels(static_cast<std::size_t>(width)*height*3);
    glReadBuffer(GL_BACK);glReadPixels(0,0,width,height,GL_RGB,GL_UNSIGNED_BYTE,pixels.data());
    GLenum error=glGetError();if(error!=GL_NO_ERROR)throw std::runtime_error("OpenGL capture error: "+std::to_string(error));
    for(int y=0;y<height/2;++y) {
        auto a=pixels.begin()+static_cast<std::size_t>(y)*width*3;
        auto b=pixels.begin()+static_cast<std::size_t>(height-1-y)*width*3;
        std::swap_ranges(a,a+width*3,b);
    }
    if(!file.parent_path().empty())fs::create_directories(file.parent_path());
    std::ofstream output(file,std::ios::binary);
    auto write=[](void* context,void* data,int size){static_cast<std::ofstream*>(context)->write(static_cast<char*>(data),size);};
    if(!stbi_write_png_to_func(write,&output,width,height,3,pixels.data(),width*3)||!output)
        throw std::runtime_error("Cannot write PNG: "+file.u8string());
}
}
