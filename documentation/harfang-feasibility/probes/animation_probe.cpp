// GPL-3.0. Feasibility measurements using the actual nxNG and HARFANG evaluators.
// This builds HARFANG's header-defined animation evaluator, not its full renderer.
#include "scene.h"
#include "engine/animation.h"
#include <json/json.hpp>
#include <cmath>
#include <fstream>
#include <iostream>
#include <map>
using json = nlohmann::json;

struct Error {
    double maximum=0, sum=0;
    size_t count=0;
    void add(double e) { maximum=std::max(maximum,std::abs(e));sum+=e*e;++count; }
    json report() const { return {{"max_abs",maximum},{"rms",std::sqrt(sum/std::max<size_t>(1,count))},{"samples",count}}; }
};

int main(int argc,char **argv) {
    if(argc!=3) {std::cerr<<"animation_probe <asset-root> <report.json>\n";return 2;}
    nxng::Assets assets(argv[1]);
    json report;
    report["scope"]="Actual native loader; HARFANG v3.3.0 EvaluateHermite<float> compiled from pinned headers. Camera channel errors, not image or world-matrix parity.";
    std::map<std::string,Error> totals;
    size_t objects=0,lights=0,keys=0,continuity=0,nonzero_tb=0,linear=0,cuts=0;
    std::map<std::string,bool> meshes;
    for(const auto &cue:nxng::timeline) {
        const auto scene=nxng::load_scene(assets,cue.file);
        json s={{"name",scene.name},{"objects",scene.objects.size()},{"lights",scene.lights.size()},
                {"fps",scene.fps},{"first_frame",scene.first_frame},{"last_frame",scene.last_frame},
                {"duration",cue.end-cue.start},{"width",scene.width},{"height",scene.height}};
        std::vector<const nxng::Motion*> motions{&scene.camera,&scene.zoom,&scene.ambient};
        size_t parented=0,pivots=0,animated_objects=0,emitters=0,dissolves=0;
        for(const auto &o:scene.objects) {
            motions.push_back(&o.motion);motions.push_back(&o.dissolve);
            parented+=o.parent!=0;pivots+=nxng::dot(o.pivot,o.pivot)!=0;
            animated_objects+=o.motion.keys.size()>1;dissolves+=!o.dissolve.keys.empty();
            emitters+=o.name.find("Emit_")!=std::string::npos;
            if(o.mesh)meshes[o.mesh->source_name]=true;
        }
        for(const auto &l:scene.lights) {motions.push_back(&l.motion);motions.push_back(&l.intensity);}
        for(const auto *m:motions)for(size_t i=0;i<m->keys.size();++i) {
            const auto &k=m->keys[i];++keys;continuity+=k.continuity!=0;nonzero_tb+=k.tension!=0||k.bias!=0;linear+=k.linear;
            if(i && k.frame-m->keys[i-1].frame==1)++cuts;
        }
        s["parented_objects"]=parented;s["nonzero_pivots"]=pivots;s["animated_objects"]=animated_objects;
        s["emitters"]=emitters;s["animated_dissolves"]=dissolves;
        s["camera_one_frame_intervals"]=0;
        for(size_t k=1;k<scene.camera.keys.size();++k)
            if(scene.camera.keys[k].frame-scene.camera.keys[k-1].frame==1)
                s["camera_one_frame_intervals"]=s["camera_one_frame_intervals"].get<int>()+1;
        for(int rate:{0,30,60,120}) {
            std::map<std::string,Error> errors;
            for(int c=0;c<6;++c) {
                hg::AnimTrackHermiteT<float> track;
                if(rate==0) {
                    for(const auto &k:scene.camera.keys)
                        track.keys.push_back({hg::time_from_sec_d((k.frame+scene.camera.offset-scene.first_frame)/scene.fps),k.value[c],k.tension,k.bias});
                } else {
                    const int count=int(std::ceil((cue.end-cue.start)*rate));
                    for(int i=0;i<=count;++i) {
                        double t=std::min(double(i)/rate,cue.end-cue.start);
                        track.keys.push_back({hg::time_from_sec_d(t),
                            scene.camera.evaluate(nxng::scene_frame(scene,cue,cue.start+t))[c],0,0});
                    }
                }
                for(int i=0;i<257;++i) {
                    const double t=(cue.end-cue.start)*(i+.317)/257.;
                    float actual=0;hg::Evaluate(track,hg::time_from_sec_d(t),actual);
                    const float expected=scene.camera.evaluate(nxng::scene_frame(scene,cue,cue.start+t))[c];
                    const std::string group=c<3?"position_units":"rotation_degrees";
                    errors[group].add(actual-expected);
                    totals[std::to_string(rate)+"_"+group].add(actual-expected);
                }
            }
            for(const auto &e:errors)s["camera_comparison"][std::to_string(rate)][e.first]=e.second.report();
        }
        objects+=scene.objects.size();lights+=scene.lights.size();report["scenes"].push_back(s);
    }
    report["inventory"]={{"scene_object_instances",objects},{"scene_lights",lights},{"unique_loaded_meshes",meshes.size()},
        {"motion_keys",keys},{"keys_with_continuity",continuity},{"keys_with_tension_or_bias",nonzero_tb},
        {"linear_keys",linear},{"one_frame_intervals_all_tracks",cuts}};
    for(const auto &p:totals)report["camera_totals"][p.first]=p.second.report();
    std::ofstream(argv[2])<<report.dump(2)<<"\n";
    std::cout<<report["inventory"].dump()<<"\n"<<report["camera_totals"].dump(2)<<"\n";
}
