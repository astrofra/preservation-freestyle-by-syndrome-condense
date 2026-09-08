// GPL-3.0; see src/LICENSE and vendor/nxng/LICENSE.
#include "audio.h"
#include "scene.h"
#include <algorithm>
#include <atomic>
#include <cmath>
#include <fstream>
#include <stdexcept>
extern "C" {
#include "xm.h"
}
#define MA_NO_DECODING
#define MA_NO_ENCODING
#define MA_NO_RESOURCE_MANAGER
#define MA_NO_NODE_GRAPH
#define MA_NO_ENGINE
#define MINIAUDIO_IMPLEMENTATION
#include "miniaudio.h"
namespace nxng {
struct Audio::Impl {
    std::vector<float> samples;
    std::atomic<std::uint64_t> cursor{0};
    std::atomic<bool> paused{false};
    ma_device device{};
    bool initialized=false;
    static void callback(ma_device* d,void* output,const void*,ma_uint32 count) {
        auto& a=*static_cast<Impl*>(d->pUserData);
        auto* out=static_cast<float*>(output);
        std::fill(out,out+count*2,0.f);
        if(a.paused.load())return;
        std::uint64_t pos=a.cursor.load();
        std::uint64_t n=std::min<std::uint64_t>(count,a.samples.size()/2-std::min<std::uint64_t>(pos,a.samples.size()/2));
        if(n)std::copy_n(a.samples.data()+pos*2,n*2,out);
        a.cursor.store(pos+n);
    }
};
Audio::Audio():impl(std::make_unique<Impl>()) {}
Audio::~Audio() {if(impl->initialized)ma_device_uninit(&impl->device);}
void Audio::load(const std::filesystem::path& path) {
    auto data=read_file(path);xm_context_t* ctx=nullptr;
    if(xm_create_context_safe(&ctx,reinterpret_cast<const char*>(data.data()),data.size(),48000)!=0)
        throw std::runtime_error("Cannot decode Mush.xm");
    // Decode once: seeking and deterministic captures use the identical PCM clock.
    impl->samples.resize(static_cast<std::size_t>(duration*48000)*2);
    xm_set_max_loop_count(ctx,1);
    xm_generate_samples(ctx,impl->samples.data(),impl->samples.size()/2);
    xm_free_context(ctx);
}
bool Audio::start(double seconds) {
    auto config=ma_device_config_init(ma_device_type_playback);
    config.playback.format=ma_format_f32;config.playback.channels=2;config.sampleRate=48000;
    config.periodSizeInMilliseconds=10;config.dataCallback=Impl::callback;config.pUserData=impl.get();
    if(ma_device_init(nullptr,&config,&impl->device)!=MA_SUCCESS)return false;
    impl->initialized=true;seek(seconds);
    return ma_device_start(&impl->device)==MA_SUCCESS;
}
void Audio::seek(double seconds) {
    bool running=impl->initialized&&ma_device_is_started(&impl->device);
    if(running)ma_device_stop(&impl->device);
    impl->cursor.store(static_cast<std::uint64_t>(std::clamp(seconds,0.,duration)*48000));
    if(running)ma_device_start(&impl->device);
}
void Audio::pause(bool p) {impl->paused.store(p);}
double Audio::seconds() const {return impl->cursor.load()/48000.;}
void Audio::save_wav(const std::filesystem::path& path) const {
    std::ofstream f(path,std::ios::binary);
    auto u16=[&](std::uint16_t n){f.put(static_cast<char>(n));f.put(static_cast<char>(n>>8));};
    auto u32=[&](std::uint32_t n){u16(static_cast<std::uint16_t>(n));u16(static_cast<std::uint16_t>(n>>16));};
    auto size=static_cast<std::uint32_t>(impl->samples.size()*2);
    f.write("RIFF",4);u32(size+36);f.write("WAVEfmt ",8);u32(16);u16(1);u16(2);u32(48000);u32(192000);u16(4);u16(16);
    f.write("data",4);u32(size);
    for(float v:impl->samples)u16(static_cast<std::uint16_t>(static_cast<std::int16_t>(std::clamp(v,-1.f,1.f)*32767)));
    if(!f)throw std::runtime_error("Cannot write audio capture: "+path.u8string());
}
}
