// Regression checks for the original animation conventions and extracted scenes.
#include "scene.h"
#include <cmath>
#include <iostream>
#include <stdexcept>
using namespace nxng;
void require(bool ok,const char* message) {if(!ok)throw std::runtime_error(message);}
void close(float a,float b) {require(std::abs(a-b)<.0001f,"Numeric regression");}
int main(int argc,char** argv) {
    try {
        Motion m;m.channels=1;
        Key a,b,c;a.frame=0;a.value[0]=2;b.frame=10;b.value[0]=12;b.linear=true;
        m.keys={a,b};close(m.scalar(5),7);close(m.scalar(-1),2);close(m.scalar(15),12);
        m.end_behavior=2;close(m.scalar(15),7);close(m.scalar(-5),7);
        m.offset=3;close(m.scalar(8),7);
        // Real camera cuts must jump immediately within one-frame intervals.
        m.offset=0;m.end_behavior=1;b.frame=1;m.keys={a,b};close(m.scalar(.1),12);
        // Zero-TCB two-key Hermite preserves constant-speed interpolation.
        b.frame=10;b.linear=false;m.keys={a,b};close(m.scalar(2.5),4.5f);
        // Tension 1 produces an eased transition, not a linear interpolation.
        a.tension=b.tension=1;m.keys={a,b};close(m.scalar(2.5),3.5625f);
        std::array<float,9> v{3,5,7,0,0,0,2,3,4};
        auto w=object_matrix(v,{1,2,3});auto p=w.point({1,-2,3});
        close(p.x,3);close(p.y,-5);close(p.z,7);
        v={3,5,7,90,0,0,1,1,1};w=object_matrix(v);p=w.vector({0,0,1});
        close(p.x,1);close(p.y,0);close(p.z,0);
        auto identity=camera_matrix(v)*w;p=identity.point({2,-4,6});
        close(p.x,2);close(p.y,-4);close(p.z,6);
        require(cue_at(23.999)==0&&cue_at(24)==1&&cue_at(199.999)==9&&cue_at(200)==10,"Timeline cut regression");
        Scene hold;hold.last_frame=600;hold.fps=30;
        close(static_cast<float>(scene_frame(hold,timeline[5],100)),46);
        require(argc==2,"Expected assets path");Assets assets(fs::u8path(argv[1]));
        auto name=std::string("D:/FREESTYLE/KitchCar - Porti")+char(0xe9)+"re.jpg";
        require(fs::exists(assets.resolve(name)),"CP1252/case-insensitive asset resolution");
        for(const auto& cue:timeline) {
            auto s=load_scene(assets,cue.file);
            for(double t=cue.start;t<cue.end;t+=.5) {
                auto matrices=world_matrices(s,scene_frame(s,cue,t));
                for(const auto& matrix:matrices)for(float f:matrix.m)require(std::isfinite(f),"Non-finite animation matrix");
            }
        }
        std::cout<<"Motion, camera cuts, hierarchy, paths and complete timeline sampling passed\n";return 0;
    } catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
}
