// GPL-3.0. Motion interpolation and matrices adapted from nXng's Cmotion.cpp,
// Cenvelop.cpp, nX_Entity.cpp and Nx_scene.cpp. See vendor/nxng/LICENSE.
#include "scene.h"
#include <algorithm>
#include <cmath>
#include <fstream>
#include <functional>
#include <sstream>
#include <stdexcept>

namespace nxng {
Vec3 operator+(Vec3 a,Vec3 b) { return {a.x+b.x,a.y+b.y,a.z+b.z}; }
Vec3 operator-(Vec3 a,Vec3 b) { return {a.x-b.x,a.y-b.y,a.z-b.z}; }
Vec3 operator*(Vec3 a,float b) { return {a.x*b,a.y*b,a.z*b}; }
float dot(Vec3 a,Vec3 b) { return a.x*b.x+a.y*b.y+a.z*b.z; }
Vec3 cross(Vec3 a,Vec3 b) { return {a.y*b.z-a.z*b.y,a.z*b.x-a.x*b.z,a.x*b.y-a.y*b.x}; }
Vec3 normalized(Vec3 a) { float l=std::sqrt(dot(a,a)); return l>1e-12f?a*(1/l):Vec3{0,1,0}; }
Vec3 Matrix::vector(Vec3 v) const {
    return {m[0]*v.x+m[4]*v.y+m[8]*v.z,m[1]*v.x+m[5]*v.y+m[9]*v.z,m[2]*v.x+m[6]*v.y+m[10]*v.z};
}
Vec3 Matrix::point(Vec3 v) const { return vector(v)+Vec3{m[12],m[13],m[14]}; }
Matrix operator*(const Matrix& a,const Matrix& b) {
    Matrix r; r.m.fill(0);
    for(int j=0;j<4;++j) for(int i=0;i<4;++i) for(int k=0;k<4;++k)
        r.m[j*4+i]+=a.m[k*4+i]*b.m[j*4+k];
    return r;
}

std::array<float,9> Motion::evaluate(double frame) const {
    if(keys.empty()) return constant;
    if(keys.size()==1) return keys.front().value;
    frame-=offset;
    if(end_behavior==2 && keys.back().frame>0) {
        frame=std::fmod(frame,keys.back().frame);
        if(frame<0) frame+=keys.back().frame;
    }
    if(frame<=keys.front().frame) return keys.front().value;
    if(frame>=keys.back().frame) return keys.back().value;
    const auto next=std::lower_bound(keys.begin()+1,keys.end(),frame,
        [](const Key& k,double f){return k.frame<f;});
    const auto index=static_cast<std::size_t>(next-keys.begin());
    const Key& a=keys[index-1]; const Key& b=*next;
    // nX treats consecutive keys as camera cuts, including fractional frames.
    if(b.frame-a.frame==1) return b.value;
    float length=b.frame-a.frame,t=static_cast<float>((frame-a.frame)/length);
    float t2=t*t,t3=t2*t,h1=1-3*t2+2*t3,h2=3*t2-2*t3,h3=t3-2*t2+t,h4=t3-t2;
    float aa=(1-a.tension)*(1+a.continuity)*(1+a.bias);
    float ab=(1-a.tension)*(1-a.continuity)*(1-a.bias);
    float ba=(1-b.tension)*(1-b.continuity)*(1+b.bias);
    float bb=(1-b.tension)*(1+b.continuity)*(1-b.bias);
    std::array<float,9> r{};
    for(int c=0;c<channels;++c) {
        float d=b.value[c]-a.value[c];
        if(b.linear) { r[c]=a.value[c]+t*d; continue; }
        float dd=index==1 ? .5f*(aa+ab)*d : length/(b.frame-keys[index-2].frame)*
            (aa*(a.value[c]-keys[index-2].value[c])+ab*d);
        float ds=index+1==keys.size() ? .5f*(ba+bb)*d : length/(keys[index+1].frame-a.frame)*
            (ba*d+bb*(keys[index+1].value[c]-b.value[c]));
        r[c]=a.value[c]*h1+b.value[c]*h2+dd*h3+ds*h4;
    }
    return r;
}
Matrix object_matrix(const std::array<float,9>& v,Vec3 p) {
    constexpr float rad=3.14159265358979323846f/180;
    float x=-v[4]*rad,y=v[3]*rad,z=-v[5]*rad;
    float sx=std::sin(x),cx=std::cos(x),sy=std::sin(y),cy=std::cos(y),sz=std::sin(z),cz=std::cos(z);
    Matrix r;
    r.m={v[6]*(cz*cy+sz*sx*sy),v[6]*sz*cx,v[6]*(sz*sx*cy-cz*sy),0,
         v[7]*(cz*sx*sy-sz*cy),v[7]*cz*cx,v[7]*(cz*sx*cy+sz*sy),0,
         v[8]*cx*sy,-v[8]*sx,v[8]*cx*cy,0,0,0,0,1};
    Vec3 t=r.vector({-p.x,p.y,-p.z})+Vec3{v[0],-v[1],v[2]};
    r.m[12]=t.x;r.m[13]=t.y;r.m[14]=t.z;
    return r;
}
Matrix camera_matrix(const std::array<float,9>& v) {
    // Rigid inverse of nX's HPB camera; camera coordinates use Y down, Z forward.
    auto c=v;c[6]=c[7]=c[8]=1;
    Matrix w=object_matrix(c),r;
    for(int j=0;j<3;++j) for(int i=0;i<3;++i) r.m[j*4+i]=w.m[i*4+j];
    Vec3 p=r.vector({-v[0],v[1],-v[2]});
    r.m[12]=p.x;r.m[13]=p.y;r.m[14]=p.z;return r;
}

namespace {
std::string lower(std::string s) {
    std::replace(s.begin(),s.end(),'\\','/');
    for(char& c:s) if(c>='A'&&c<='Z') c+=32;
    return s;
}
std::string trim(const std::string& s) {
    auto a=s.find_first_not_of(" \t\r\n");
    return a==std::string::npos?"":s.substr(a,s.find_last_not_of(" \t\r\n")-a+1);
}
Vec3 vec(const std::string& s) { Vec3 v;std::istringstream(s)>>v.x>>v.y>>v.z;return v; }
Motion parse_motion(const std::vector<std::string>& lines,std::size_t& i) {
    Motion m;
    m.channels=std::stoi(lines.at(++i));
    int n=std::stoi(lines.at(++i));
    if(m.channels<1||m.channels>9||n<1||n>100000) throw std::runtime_error("Invalid LWS motion");
    for(int k=0;k<n;++k) {
        Key key;std::istringstream values(lines.at(++i));
        for(int c=0;c<m.channels;++c) if(!(values>>key.value[c])) throw std::runtime_error("Truncated LWS key");
        int linear=0;
        if(!(std::istringstream(lines.at(++i))>>key.frame>>linear>>key.tension>>key.continuity>>key.bias))
            throw std::runtime_error("Invalid LWS key metadata");
        key.linear=linear!=0;
        if(!m.keys.empty()&&key.frame<=m.keys.back().frame) throw std::runtime_error("Unordered LWS keys");
        m.keys.push_back(key);
    }
    if(i+1<lines.size()&&lines[i+1].rfind("FrameOffset ",0)==0) m.offset=std::stof(lines[++i].substr(12));
    if(i+1<lines.size()&&lines[i+1].rfind("EndBehavior ",0)==0) m.end_behavior=std::stoi(lines[++i].substr(12));
    return m;
}
Motion scalar(const std::string& value,const std::vector<std::string>& lines,std::size_t& i) {
    if(value.find('(')!=std::string::npos) return parse_motion(lines,i);
    Motion m;m.constant[0]=std::stof(value);return m;
}
}
std::string cp1252_to_utf8(const std::string& s) {
    static constexpr unsigned table[32]={0x20ac,0x81,0x201a,0x192,0x201e,0x2026,0x2020,0x2021,
        0x2c6,0x2030,0x160,0x2039,0x152,0x8d,0x17d,0x8f,0x90,0x2018,0x2019,0x201c,
        0x201d,0x2022,0x2013,0x2014,0x2dc,0x2122,0x161,0x203a,0x153,0x9d,0x17e,0x178};
    std::string out;
    for(unsigned char b:s) {
        unsigned c=b>=0x80&&b<0xa0?table[b-0x80]:b;
        if(c<128) out+=static_cast<char>(c);
        else if(c<2048) {out+=static_cast<char>(0xc0|(c>>6));out+=static_cast<char>(0x80|(c&63));}
        else {out+=static_cast<char>(0xe0|(c>>12));out+=static_cast<char>(0x80|((c>>6)&63));out+=static_cast<char>(0x80|(c&63));}
    }
    return out;
}
std::vector<unsigned char> read_file(const fs::path& path) {
    std::ifstream f(path,std::ios::binary);
    if(!f) throw std::runtime_error("Cannot read "+path.u8string());
    return {std::istreambuf_iterator<char>(f),{}};
}
Assets::Assets(fs::path path):root(fs::absolute(path)) {
    if(!fs::is_directory(root)) throw std::runtime_error("Assets directory missing: "+root.u8string());
    for(const auto& e:fs::recursive_directory_iterator(root)) if(e.is_regular_file()) {
        auto relative=lower(e.path().lexically_relative(root).u8string());
        paths.emplace(relative,e.path());
        paths.emplace(lower(e.path().filename().u8string()),e.path());
    }
}
fs::path Assets::resolve(const std::string& legacy) const {
    std::string s=lower(cp1252_to_utf8(legacy));
    if(s.size()>2&&s[1]==':') s.erase(1,1);
    auto it=paths.find(s);
    if(it==paths.end()) it=paths.find(s.substr(s.find_last_of('/')+1));
    if(it==paths.end()) throw std::runtime_error("Missing asset: "+cp1252_to_utf8(legacy));
    return it->second;
}
std::shared_ptr<Mesh> Assets::mesh(const std::string& legacy) {
    auto path=resolve(legacy);auto key=path.u8string();auto it=meshes.find(key);
    if(it!=meshes.end()) return it->second;
    auto m=std::make_shared<Mesh>();m->source_name=path.filename().u8string();m->source_path=key;
    std::string error;if(!load_mesh(path,*this,m.get(),&error)) throw std::runtime_error(error);
    meshes.emplace(key,m);return m;
}
Scene load_scene(Assets& assets,const std::string& name) {
    Scene s;s.name=name;s.zoom.constant[0]=1.6f;s.ambient.constant[0]=.25f;
    auto data=read_file(assets.resolve(name));std::istringstream file(std::string(data.begin(),data.end()));
    std::vector<std::string> lines;std::string line;
    while(std::getline(file,line)) lines.push_back(trim(line));
    if(lines.size()<2||lines[0]!="LWSC"||lines[1]!="1") throw std::runtime_error("Expected LWSC version 1: "+name);
    Object* obj=nullptr;Light* light=nullptr;bool camera=false;
    for(std::size_t i=2;i<lines.size();++i) {
        auto split=lines[i].find(' ');auto key=lines[i].substr(0,split);
        auto value=split==std::string::npos?"":trim(lines[i].substr(split+1));
        if(key=="LoadObject"||key=="AddNullObject") {
            s.objects.emplace_back();obj=&s.objects.back();light=nullptr;camera=false;obj->name=value;
            obj->motion.channels=9;obj->motion.constant[6]=obj->motion.constant[7]=obj->motion.constant[8]=1;
            if(key=="LoadObject") obj->mesh=assets.mesh(value);
        } else if(key=="AddLight") {
            s.lights.emplace_back();light=&s.lights.back();obj=nullptr;camera=false;light->intensity.constant[0]=1;
        } else if(key=="ShowCamera") {obj=nullptr;light=nullptr;camera=true;}
        else if(key=="ObjectMotion"&&obj) obj->motion=parse_motion(lines,i);
        else if(key=="LightMotion"&&light) light->motion=parse_motion(lines,i);
        else if(key=="CameraMotion") {s.camera=parse_motion(lines,i);obj=nullptr;light=nullptr;camera=true;}
        else if(key=="PivotPoint"&&obj) obj->pivot=vec(value);
        else if(key=="ParentObject") {
            if(obj)obj->parent=std::stoi(value);else if(light) light->parent=std::stoi(value);
        } else if(key=="ObjDissolve"&&obj) obj->dissolve=scalar(value,lines,i);
        else if(key=="UnaffectedByFog"&&obj) obj->unaffected_by_fog=std::stoi(value)!=0;
        else if(key=="LightType"&&light) light->type=std::stoi(value);
        else if(key=="LightColor"&&light) light->color=vec(value)*(1/255.f);
        else if(key=="LgtIntensity"&&light) light->intensity=scalar(value,lines,i);
        else if(key=="LightRange"&&light) light->range=std::stof(value);
        else if(key=="LightConeAngle"&&light) light->cone=std::stof(value);
        else if(key=="LightEdgeAngle"&&light) light->edge=std::stof(value);
        else if(key=="FlareIntensity"&&light) light->flare=std::stof(value);
        else if(key=="PreviewFirstFrame") s.first_frame=std::stof(value);
        else if(key=="PreviewLastFrame") s.last_frame=std::stof(value);
        else if(key=="FramesPerSecond") s.fps=std::stof(value);
        else if(key=="ZoomFactor"&&camera) s.zoom=scalar(value,lines,i);
        else if(key=="CustomSize") std::istringstream(value)>>s.width>>s.height;
        else if(key=="BGImage") s.background=assets.resolve(value).u8string();
        else if(key=="BackdropColor") s.backdrop=vec(value)*(1/255.f);
        else if(key=="AmbientColor") s.ambient_color=vec(value)*(1/255.f);
        else if(key=="AmbIntensity") s.ambient=scalar(value,lines,i);
        else if(key=="FogType") s.fog_type=std::stoi(value);
        else if(key=="FogMinDist") s.fog_min=std::stof(value);
        else if(key=="FogMaxDist") s.fog_max=std::stof(value);
        else if(key=="FogColor") s.fog_color=vec(value)*(1/255.f);
        else if(key=="GridSize") s.grid_size=std::stof(value);
    }
    if(s.camera.keys.empty()) throw std::runtime_error("No camera: "+name);
    (void)world_matrices(s,0); // validates parent indices and detects cycles.
    return s;
}
std::vector<Matrix> world_matrices(const Scene& scene,double frame) {
    std::vector<Matrix> result(scene.objects.size());std::vector<int> state(result.size());
    std::function<void(std::size_t)> resolve=[&](std::size_t i){
        if(state[i]==2)return;
        if(state[i]==1)throw std::runtime_error("Cyclic parent in "+scene.name);
        state[i]=1;const auto& o=scene.objects[i];result[i]=object_matrix(o.motion.evaluate(frame),o.pivot);
        if(o.parent) {
            if(o.parent<1||static_cast<std::size_t>(o.parent)>result.size()) throw std::runtime_error("Invalid parent in "+scene.name);
            auto p=static_cast<std::size_t>(o.parent-1);resolve(p);result[i]=result[p]*result[i];
        }
        state[i]=2;
    };
    for(std::size_t i=0;i<result.size();++i) resolve(i);
    return result;
}

// Absolute GetTickCount offsets recovered from freestyle.exe, not script.txt.
const std::array<Cue,11> timeline{{
    {"0_sceneTunnelAnim.lws",0,24}, {"1_ZomBie.lws",24,37},
    {"2_LogoFREESTYLE.lws",37,45,2}, {"3_Colline.lws",45,73},
    {"4_SceneChampi.lws",73,96}, {"5_ChampiGIRLgfx.lws",96,103,1.5},
    {"55_ToTheManor.lws",103,131}, {"6_HOUSEofTHEkitCH.lws",131,147},
    {"7_TheTRAP!.lws",147,167,19}, {"8_Wgon.lws",167,200},
    {"9_SyndromeFINAL.lws",200,210}
}};
std::size_t cue_at(double t) {
    for(std::size_t i=0;i<timeline.size();++i) if(t<timeline[i].end) return i;
    return timeline.size()-1;
}
double scene_frame(const Scene& scene,const Cue& cue,double time) {
    double local=std::max(0.,time-cue.start);
    if(cue.hold_after>=0) local=std::min(local,cue.hold_after);
    return std::min<double>(scene.last_frame,scene.first_frame+local*scene.fps);
}
}
