// GPL-3.0; see src/LICENSE and vendor/nxng/LICENSE.
#pragma once
#include "scene.h"
#include <memory>
struct GLFWwindow;
namespace nxng {
class Renderer {
public:
    Renderer(int width,int height,bool hidden,bool fullscreen);
    ~Renderer();
    void preload(const std::vector<Scene>& scenes,Assets& assets);
    void draw(const Scene& scene,double frame,int width,int height);
    void capture(const fs::path& file,int width,int height);
    GLFWwindow* window() const;
private:
    struct Impl;
    std::unique_ptr<Impl> impl;
};
}
