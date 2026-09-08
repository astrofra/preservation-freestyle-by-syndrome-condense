// Freestyle reconstruction. GPL-3.0; derived in part from nXng (see vendor/nxng).
#pragma once
#include <array>
#include <cstdint>
#include <filesystem>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

namespace nxng {
namespace fs = std::filesystem;
struct Vec3 { float x=0, y=0, z=0; };
struct Vec2 { float u=0, v=0; };
Vec3 operator+(Vec3 a, Vec3 b);
Vec3 operator-(Vec3 a, Vec3 b);
Vec3 operator*(Vec3 a, float b);
float dot(Vec3 a, Vec3 b);
Vec3 cross(Vec3 a, Vec3 b);
Vec3 normalized(Vec3 a);
struct Matrix {
    std::array<float,16> m{1,0,0,0, 0,1,0,0, 0,0,1,0, 0,0,0,1};
    Vec3 point(Vec3 v) const;
    Vec3 vector(Vec3 v) const;
};
Matrix operator*(const Matrix& a, const Matrix& b);
struct Key {
    std::array<float,9> value{};
    float frame=0, tension=0, continuity=0, bias=0;
    bool linear=false;
};
struct Motion {
    std::vector<Key> keys;
    int channels=1, end_behavior=1;
    float offset=0;
    std::array<float,9> constant{};
    std::array<float,9> evaluate(double frame) const;
    float scalar(double frame) const { return evaluate(frame)[0]; }
};
Matrix object_matrix(const std::array<float,9>& values, Vec3 pivot={});
Matrix camera_matrix(const std::array<float,9>& values);
enum class TextureProjection { kNone, kPlanar, kCylindrical, kCubic, kSpherical };
struct Surface {
    std::string name, texture_path, reflection_path;
    Vec3 texture_center{}, texture_size{1,1,1}, velocity{};
    std::uint16_t texture_flags=0, flags=0;
    std::uint8_t color_r=200, color_g=200, color_b=200;
    int wrap_u=1, wrap_v=1;
    float luminosity=0, diffuse=1, transparency=0, smoothing=1.56207f;
    bool chroma=false, unlit=false, additive=false, alpha_only=false;
    TextureProjection projection=TextureProjection::kNone;
};
struct Triangle {
    std::uint32_t i0=0,i1=0,i2=0;
    std::int32_t surface_index=-1;
    Vec3 normal{};
    std::array<Vec3,3> normals{};
    std::array<Vec2,3> uv{};
};
struct Mesh {
    std::string source_name, source_path;
    std::vector<Vec3> vertices;
    std::vector<Triangle> triangles;
    std::vector<Surface> surfaces;
};
struct Object {
    std::string name;
    std::shared_ptr<Mesh> mesh;
    Motion motion, dissolve;
    Vec3 pivot{};
    int parent=0;
    bool unaffected_by_fog=false;
};
struct Light {
    Motion motion, intensity;
    Vec3 color{1,1,1};
    int type=0,parent=0;
    float range=0, cone=45, edge=0, flare=0;
};
struct Scene {
    std::string name, background;
    std::vector<Object> objects;
    std::vector<Light> lights;
    Motion camera, zoom, ambient;
    Vec3 backdrop{}, ambient_color{1,1,1}, fog_color{};
    float fps=30, first_frame=1, last_frame=600, grid_size=1;
    float fog_min=0, fog_max=100;
    int width=640,height=480,fog_type=0;
};
class Assets {
public:
    explicit Assets(fs::path root);
    fs::path resolve(const std::string& legacy) const;
    std::shared_ptr<Mesh> mesh(const std::string& legacy);
    const fs::path root;
private:
    std::unordered_map<std::string,fs::path> paths;
    std::unordered_map<std::string,std::shared_ptr<Mesh>> meshes;
};
std::string cp1252_to_utf8(const std::string& value);
std::vector<unsigned char> read_file(const fs::path& path);
bool load_mesh(const fs::path& path, Assets& assets, Mesh* mesh, std::string* error);
Scene load_scene(Assets& assets,const std::string& name);
std::vector<Matrix> world_matrices(const Scene& scene,double frame);
struct Cue { const char* file; double start,end; double hold_after=-1; };
extern const std::array<Cue,11> timeline;
constexpr double duration=210.0;
std::size_t cue_at(double seconds);
double scene_frame(const Scene& scene,const Cue& cue,double seconds);
}
