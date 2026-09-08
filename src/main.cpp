// GPL-3.0; see src/LICENSE and vendor/nxng/LICENSE.
#include "scene.h"
#include "renderer.h"
#include "audio.h"
#include <GLFW/glfw3.h>
#include <algorithm>
#include <cmath>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <stdexcept>

namespace {
using namespace nxng;
struct Options {
    fs::path assets,capture_dir="captures",wav;
    std::vector<double> captures;
    double time=0,run_duration=-1,capture_fps=0;
    int width=960,height=720;
    bool hidden=false,fullscreen=false,mute=false,verify=false;
};
void usage() {
    std::cout<<"FreeStyle reconstruction (Condense / Syndrome)\n"
        "  --assets DIR               Extracted cds-freestyle directory\n"
        "  --time SECONDS             Seek on the original 0..210s timeline\n"
        "  --duration SECONDS         Exit after this much playback\n"
        "  --size WIDTHxHEIGHT        Window/capture size (default 960x720)\n"
        "  --fullscreen --hidden --mute\n"
        "  --capture-times 5,25,50    Deterministic PNGs at exact demo times\n"
        "  --capture-fps FPS          Capture fixed steps; use --time and --duration\n"
        "  --capture-dir DIR          PNGs and frame timing manifest\n"
        "  --audio-wav FILE           Export Mush.xm as 48kHz stereo PCM\n"
        "  --verify-assets            Load and validate all scenes, then exit\n"
        "Controls: Escape quit, Space pause, Left/Right seek 5s, Home restart, F12 PNG\n";
}
double number(const std::string& s) {
    std::size_t n=0;double v=std::stod(s,&n);
    if(n!=s.size()||!std::isfinite(v))throw std::runtime_error("Invalid number: "+s);
    return v;
}
fs::path find_assets(const char* argv0) {
    std::vector<fs::path> bases{fs::current_path(),fs::absolute(fs::u8path(argv0)).parent_path()};
    for(const auto& base:bases)for(auto p=base;!p.empty();) {
        for(const auto& relative:{"assets","demo-assets/cds-freestyle"})
            if(fs::is_regular_file(p/relative/"D/FreeStyle/0_sceneTunnelAnim.lws"))return p/relative;
        auto next=p.parent_path();if(next==p)break;p=next;
    }
    throw std::runtime_error("Cannot locate assets. Pass --assets <demo-assets/cds-freestyle>.");
}
std::string capture_name(double time,std::size_t index) {
    std::ostringstream s;s<<"frame_"<<std::setfill('0')<<std::setw(6)<<index<<"_"<<std::setw(9)<<std::llround(time*1000)<<"ms.png";return s.str();
}
int run(int argc,char** argv) {
    Options o;
    for(int i=1;i<argc;++i) {
        std::string a=argv[i];auto value=[&](){if(i+1>=argc)throw std::runtime_error("Missing value for "+a);return std::string(argv[++i]);};
        if(a=="--help"||a=="-h"){usage();return 0;}
        else if(a=="--assets")o.assets=fs::u8path(value());
        else if(a=="--capture-dir")o.capture_dir=fs::u8path(value());
        else if(a=="--audio-wav")o.wav=fs::u8path(value());
        else if(a=="--time")o.time=number(value());
        else if(a=="--duration")o.run_duration=number(value());
        else if(a=="--capture-fps")o.capture_fps=number(value());
        else if(a=="--capture-times") {std::istringstream s(value());std::string t;while(std::getline(s,t,','))o.captures.push_back(number(t));}
        else if(a=="--size") {auto size=value();auto x=size.find('x');if(x==std::string::npos)throw std::runtime_error("Expected WIDTHxHEIGHT");o.width=static_cast<int>(number(size.substr(0,x)));o.height=static_cast<int>(number(size.substr(x+1)));}
        else if(a=="--hidden")o.hidden=true;
        else if(a=="--fullscreen")o.fullscreen=true;
        else if(a=="--mute")o.mute=true;
        else if(a=="--verify-assets")o.verify=true;
        else throw std::runtime_error("Unknown option: "+a);
    }
    if(o.width<64||o.height<64||o.width>8192||o.height>8192||o.time<0||o.time>=duration||o.capture_fps<0||o.capture_fps>240)
        throw std::runtime_error("Invalid size, time or capture rate");
    if(o.capture_fps>0) {
        double end=std::min(duration,o.time+(o.run_duration>=0?o.run_duration:duration));
        for(std::uint64_t n=0;o.time+n/o.capture_fps<end;++n)o.captures.push_back(o.time+n/o.capture_fps);
    }
    for(double t:o.captures)if(t<0||t>=duration)throw std::runtime_error("Capture time must be in [0,210)");
    if(o.assets.empty())o.assets=find_assets(argv[0]);
    Assets assets(o.assets);std::vector<Scene> scenes;
    for(const auto& cue:timeline) {
        scenes.push_back(load_scene(assets,cue.file));
        const auto& s=scenes.back();std::size_t triangles=0;for(const auto& ob:s.objects)if(ob.mesh)triangles+=ob.mesh->triangles.size();
        std::cout<<cue.start<<".."<<cue.end<<"s "<<cue.file<<": "<<s.objects.size()<<" objects, "<<triangles<<" triangles, "<<s.fps<<" fps\n";
    }
    std::cout.flush();if(o.verify)return 0;
    Audio audio;
    bool need_audio=(!o.mute&&o.captures.empty())||!o.wav.empty();
    if(need_audio)audio.load(assets.resolve("D:/FreeStyle/BGM/Mush.xm"));
    if(!o.wav.empty()) {
        if(!o.wav.parent_path().empty())fs::create_directories(o.wav.parent_path());
        audio.save_wav(o.wav);std::cout<<"Audio capture: "<<o.wav.u8string()<<'\n';
        if(o.captures.empty())return 0;
    }
    Renderer renderer(o.width,o.height,o.hidden,o.fullscreen);renderer.preload(scenes,assets);
    auto* window=renderer.window();
    if(!o.captures.empty()) {
        fs::create_directories(o.capture_dir);std::ofstream manifest(o.capture_dir/"frames.csv");
        manifest<<"file,demo_seconds,scene,scene_frame\n"<<std::fixed<<std::setprecision(6);
        for(std::size_t i=0;i<o.captures.size();++i) {
            double t=o.captures[i];std::size_t index=cue_at(t);double frame=scene_frame(scenes[index],timeline[index],t);
            int w,h;glfwGetFramebufferSize(window,&w,&h);renderer.draw(scenes[index],frame,w,h);
            auto name=capture_name(t,i);renderer.capture(o.capture_dir/name,w,h);
            manifest<<name<<','<<t<<','<<timeline[index].file<<','<<frame<<'\n';
            glfwSwapBuffers(window);glfwPollEvents();
        }
        if(!manifest)throw std::runtime_error("Could not write capture manifest");
        std::cout<<o.captures.size()<<" frames captured to "<<o.capture_dir.u8string()<<'\n';return 0;
    }
    bool sound=!o.mute&&audio.start(o.time);
    if(!o.mute&&!sound)std::cerr<<"Audio device unavailable; continuing on monotonic clock\n";
    double anchor=glfwGetTime(),start=anchor,offset=o.time,t=o.time;
    bool paused=false;std::array<bool,GLFW_KEY_LAST+1> keys{};std::size_t last_scene=timeline.size(),snap=0;
    auto pressed=[&](int key){bool down=glfwGetKey(window,key)==GLFW_PRESS;bool edge=down&&!keys[key];keys[key]=down;return edge;};
    while(!glfwWindowShouldClose(window)) {
        glfwPollEvents();double now=glfwGetTime();
        if(!paused)t=sound?audio.seconds():offset+now-anchor;
        if(t>=duration||(o.run_duration>=0&&now-start>=o.run_duration))break;
        if(pressed(GLFW_KEY_ESCAPE))break;
        if(pressed(GLFW_KEY_SPACE)){paused=!paused;audio.pause(paused);offset=t;anchor=now;}
        double seek=t;
        if(pressed(GLFW_KEY_LEFT))seek-=5;
        if(pressed(GLFW_KEY_RIGHT))seek+=5;
        if(pressed(GLFW_KEY_HOME))seek=0;
        if(seek!=t) {t=std::clamp(seek,0.,duration-.001);offset=t;anchor=now;if(sound)audio.seek(t);}
        auto index=cue_at(t);double frame=scene_frame(scenes[index],timeline[index],t);
        if(index!=last_scene){std::cout<<"Playing "<<timeline[index].file<<" at "<<t<<"s\n";last_scene=index;}
        int w,h;glfwGetFramebufferSize(window,&w,&h);if(w<1||h<1){glfwWaitEventsTimeout(.02);continue;}
        renderer.draw(scenes[index],frame,w,h);
        if(pressed(GLFW_KEY_F12))renderer.capture(o.capture_dir/capture_name(t,snap++),w,h);
        glfwSwapBuffers(window);
        if(o.hidden)glfwWaitEventsTimeout(.001);
    }
    return 0;
}
}
int main(int argc,char** argv) {
    try{return run(argc,argv);}catch(const std::exception& e){std::cerr<<"FreeStyle: "<<e.what()<<'\n';return 1;}
}
