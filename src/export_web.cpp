// GPL-3.0; exports native preservation data without changing its interpretation.
#include "scene.h"
#include <fstream>
#include <iomanip>
#include <iostream>
#include <map>
#include <stdexcept>
#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h"
using namespace nxng;
namespace {
void str(std::ostream& o,const std::string& s) {
    o<<'"';for(unsigned char c:s) {
        if(c=='"'||c=='\\')o<<'\\'<<c;
        else if(c<32)o<<"\\u"<<std::hex<<std::setw(4)<<std::setfill('0')<<int(c)<<std::dec;
        else o<<c;
    }o<<'"';
}
template<class T> void array(std::ostream& o,const T& values) {
    o<<'[';bool first=true;for(auto v:values){if(!first)o<<',';first=false;o<<v;}o<<']';
}
void vec(std::ostream& o,Vec3 v) {array(o,std::array<float,3>{v.x,v.y,v.z});}
void motion(std::ostream& o,const Motion& m) {
    o<<"{\"channels\":"<<m.channels<<",\"end\":"<<m.end_behavior<<",\"offset\":"<<m.offset<<",\"constant\":";
    array(o,m.constant);o<<",\"keys\":[";
    for(std::size_t i=0;i<m.keys.size();++i){if(i)o<<',';const auto& k=m.keys[i];
        o<<"{\"frame\":"<<k.frame<<",\"linear\":"<<(k.linear?"true":"false")<<",\"tension\":"<<k.tension
         <<",\"continuity\":"<<k.continuity<<",\"bias\":"<<k.bias<<",\"value\":";array(o,k.value);o<<'}';}
    o<<"]}";
}
struct Export {
    fs::path output;
    std::map<std::string,std::string> textures;
    std::map<const Mesh*,int> mesh_ids;
    std::vector<std::shared_ptr<Mesh>> meshes;
    std::string texture(const std::string& source) {
        if(source.empty())return "";
        auto found=textures.find(source);if(found!=textures.end())return found->second;
        auto name="textures/"+std::to_string(textures.size())+".png";
        auto bytes=read_file(fs::u8path(source));int w,h,c;
        auto* pixels=stbi_load_from_memory(bytes.data(),static_cast<int>(bytes.size()),&w,&h,&c,4);
        if(!pixels)throw std::runtime_error("Texture decode failed: "+source);
        std::ofstream file(output/fs::u8path(name),std::ios::binary);
        auto write=[](void* context,void* data,int size){static_cast<std::ofstream*>(context)->write(static_cast<char*>(data),size);};
        bool ok=stbi_write_png_to_func(write,&file,w,h,4,pixels,w*4)!=0;stbi_image_free(pixels);
        if(!ok||!file)throw std::runtime_error("Texture export failed");
        textures.emplace(source,name);return name;
    }
    void surface(std::ostream& o,const Surface& s) {
        o<<"{\"texture\":";str(o,texture(s.texture_path));o<<",\"reflection\":";str(o,texture(s.reflection_path));
        o<<",\"flags\":"<<s.flags<<",\"textureFlags\":"<<s.texture_flags<<",\"color\":";
        array(o,std::array<int,3>{s.color_r,s.color_g,s.color_b});
        o<<",\"velocity\":";vec(o,s.velocity);o<<",\"wrapU\":"<<s.wrap_u<<",\"wrapV\":"<<s.wrap_v
         <<",\"luminosity\":"<<s.luminosity<<",\"diffuse\":"<<s.diffuse<<",\"transparency\":"<<s.transparency
         <<",\"chroma\":"<<(s.chroma?"true":"false")<<",\"unlit\":"<<(s.unlit?"true":"false")
         <<",\"additive\":"<<(s.additive?"true":"false")<<",\"alphaOnly\":"<<(s.alpha_only?"true":"false")<<'}';
    }
    void mesh(std::ostream& o,const Mesh& m) {
        o<<"{\"name\":";str(o,m.source_name);o<<",\"surfaces\":[";
        for(std::size_t i=0;i<m.surfaces.size();++i){if(i)o<<',';surface(o,m.surfaces[i]);}
        o<<"],\"corners\":[";bool first=true;
        for(const auto& t:m.triangles)for(int c=0;c<3;++c) {
            const auto& p=m.vertices[c==0?t.i0:c==1?t.i1:t.i2];const auto& n=t.normals[c];
            for(float v:{p.x,p.y,p.z,n.x,n.y,n.z,t.uv[c].u,t.uv[c].v}){if(!first)o<<',';first=false;o<<v;}
        }
        o<<"],\"materials\":[";first=true;
        for(const auto& t:m.triangles){if(!first)o<<',';first=false;o<<t.surface_index;}o<<"]}";
    }
    void scene(std::ostream& o,const Scene& s,const Cue& cue) {
        o<<"{\"name\":";str(o,s.name);o<<",\"start\":"<<cue.start<<",\"end\":"<<cue.end<<",\"hold\":"<<cue.hold_after
         <<",\"fps\":"<<s.fps<<",\"first\":"<<s.first_frame<<",\"last\":"<<s.last_frame
         <<",\"width\":"<<s.width<<",\"height\":"<<s.height<<",\"gridSize\":"<<s.grid_size<<",\"background\":";
        str(o,texture(s.background));o<<",\"backdrop\":";vec(o,s.backdrop);o<<",\"ambientColor\":";vec(o,s.ambient_color);
        o<<",\"camera\":";motion(o,s.camera);o<<",\"zoom\":";motion(o,s.zoom);o<<",\"ambient\":";motion(o,s.ambient);
        o<<",\"objects\":[";
        for(std::size_t i=0;i<s.objects.size();++i) {
            if(i)o<<',';const auto& v=s.objects[i];
            o<<"{\"name\":";str(o,cp1252_to_utf8(v.name));o<<",\"mesh\":"<<(v.mesh?mesh_ids.at(v.mesh.get()):-1)
             <<",\"parent\":"<<v.parent<<",\"pivot\":";vec(o,v.pivot);o<<",\"motion\":";motion(o,v.motion);
            o<<",\"dissolve\":";motion(o,v.dissolve);o<<'}';
        }
        o<<"],\"lights\":[";
        for(std::size_t i=0;i<s.lights.size();++i) {
            if(i)o<<',';const auto& l=s.lights[i];o<<"{\"motion\":";motion(o,l.motion);o<<",\"intensity\":";motion(o,l.intensity);
            o<<",\"color\":";vec(o,l.color);o<<",\"type\":"<<l.type<<",\"parent\":"<<l.parent<<",\"range\":"<<l.range
             <<",\"cone\":"<<l.cone<<",\"edge\":"<<l.edge<<",\"flare\":"<<l.flare<<'}';
        }o<<"]}";
    }
};
}
int main(int argc,char** argv) {
    try {
        if(argc!=3)throw std::runtime_error("Usage: freestyle_export_web ASSET_ROOT OUTPUT_DIRECTORY");
        Assets assets(fs::u8path(argv[1]));Export e;e.output=fs::u8path(argv[2]);
        fs::create_directories(e.output/"textures");std::vector<Scene> scenes;
        for(const auto& cue:timeline) {
            scenes.push_back(load_scene(assets,cue.file));
            for(const auto& o:scenes.back().objects)if(o.mesh&&!e.mesh_ids.count(o.mesh.get())) {
                e.mesh_ids[o.mesh.get()]=static_cast<int>(e.meshes.size());e.meshes.push_back(o.mesh);
            }
        }
        std::ofstream out(e.output/"demo.json");out<<std::setprecision(9);
        out<<"{\"version\":1,\"duration\":"<<duration<<",\"spark\":";
        str(out,e.texture(assets.resolve("D:/Devellop/nX_Pics/Spark.jpg").u8string()));out<<",\"flare\":";
        str(out,e.texture(assets.resolve("D:/Devellop/nX_Pics/Flare.jpg").u8string()));out<<",\"scenes\":[";
        for(std::size_t i=0;i<scenes.size();++i){if(i)out<<',';e.scene(out,scenes[i],timeline[i]);}
        out<<"],\"meshes\":[";
        for(std::size_t i=0;i<e.meshes.size();++i){if(i)out<<',';e.mesh(out,*e.meshes[i]);}out<<"]}\n";
        if(!out)throw std::runtime_error("Cannot write demo.json");
        // Cross-language oracle: native transforms, fractional frames and holds.
        std::ofstream test(e.output/"motion-reference.json");test<<std::setprecision(9)<<'[';bool first=true;
        for(std::size_t si=0;si<scenes.size();++si) {
            const auto& s=scenes[si];const auto& cue=timeline[si];
            for(double time=cue.start;time<cue.end;time+=.731) {
                if(!first)test<<',';first=false;auto frame=scene_frame(s,cue,time);
                test<<"{\"scene\":"<<si<<",\"time\":"<<time<<",\"frame\":"<<frame<<",\"camera\":";
                array(test,camera_matrix(s.camera.evaluate(frame)).m);test<<",\"worlds\":[";
                auto worlds=world_matrices(s,frame);for(std::size_t i=0;i<worlds.size();++i){if(i)test<<',';array(test,worlds[i].m);}
                test<<"]}";
            }
        }test<<"]\n";if(!test)throw std::runtime_error("Cannot write motion-reference.json");
        std::cout<<scenes.size()<<" scenes, "<<e.meshes.size()<<" meshes, "<<e.textures.size()<<" lossless textures exported\n";
        return 0;
    }catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}
}
