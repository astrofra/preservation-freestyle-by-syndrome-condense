// GPL-3.0; see src/LICENSE and vendor/nxng/LICENSE.
#pragma once
#include <filesystem>
#include <memory>
#include <vector>
namespace nxng {
class Audio {
public:
    Audio();
    ~Audio();
    void load(const std::filesystem::path& xm);
    bool start(double seconds);
    void seek(double seconds);
    void pause(bool paused);
    double seconds() const;
    void save_wav(const std::filesystem::path& file) const;
private:
    struct Impl;
    std::unique_ptr<Impl> impl;
};
}
