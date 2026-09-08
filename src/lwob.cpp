#include "scene.h"
#include <cmath>
// Adapted from nxng_cli scene_loader.cpp; GPL-3.0, see vendor/nxng/LICENSE.

#include <algorithm>
#include <cctype>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

namespace nxng {
namespace {

struct RawFace {
    std::vector<std::uint16_t> indices;
    std::uint16_t surface_index{0};
};

std::string trim(std::string s) {
    while (!s.empty() && (s.back() == '\r' || s.back() == '\n' || std::isspace(static_cast<unsigned char>(s.back())))) {
        s.pop_back();
    }
    std::size_t i = 0;
    while (i < s.size() && std::isspace(static_cast<unsigned char>(s[i]))) {
        ++i;
    }
    return s.substr(i);
}

bool starts_with(const std::string& s, const char* prefix) {
    const std::size_t n = std::strlen(prefix);
    return s.size() >= n && s.compare(0, n, prefix) == 0;
}

std::string normalize_legacy_path(std::string p) {
    std::replace(p.begin(), p.end(), '\\', '/');
    return p;
}

std::string basename_legacy(const std::string& path) {
    const auto normalized = normalize_legacy_path(path);
    const auto slash = normalized.find_last_of('/');
    return (slash == std::string::npos) ? normalized : normalized.substr(slash + 1);
}

std::vector<unsigned char> read_binary(const std::filesystem::path& path) {
    std::ifstream in(path, std::ios::binary);
    if (!in) {
        return {};
    }
    in.seekg(0, std::ios::end);
    const auto size = static_cast<std::size_t>(in.tellg());
    in.seekg(0, std::ios::beg);
    std::vector<unsigned char> data(size);
    if (size > 0) {
        in.read(reinterpret_cast<char*>(data.data()), static_cast<std::streamsize>(size));
        if (!in) {
            return {};
        }
    }
    return data;
}

std::uint16_t be16(const unsigned char* p) {
    return static_cast<std::uint16_t>((static_cast<std::uint16_t>(p[0]) << 8U) | p[1]);
}

std::uint32_t be32(const unsigned char* p) {
    return (static_cast<std::uint32_t>(p[0]) << 24U) |
           (static_cast<std::uint32_t>(p[1]) << 16U) |
           (static_cast<std::uint32_t>(p[2]) << 8U) |
           static_cast<std::uint32_t>(p[3]);
}

float be_f32(const unsigned char* p) {
    const std::uint32_t bits = be32(p);
    float out = 0.0f;
    std::memcpy(&out, &bits, sizeof(out));
    return out;
}

std::pair<std::string, std::size_t> read_cstr_even(const unsigned char* data, std::size_t size, std::size_t offset) {
    if (offset >= size) {
        return {"", size};
    }

    std::size_t end = offset;
    while (end < size && data[end] != 0) {
        ++end;
    }

    if (end >= size) {
        return {"", size};
    }

    std::string s(reinterpret_cast<const char*>(data + offset), end - offset);
    std::size_t next = end + 1;
    if (next & 1U) {
        ++next;
    }
    return {s, std::min(next, size)};
}

TextureProjection projection_from_string(const std::string& kind) {
    if (kind.find("Planar") != std::string::npos) {
        return TextureProjection::kPlanar;
    }
    if (kind.find("Cylindrical") != std::string::npos) {
        return TextureProjection::kCylindrical;
    }
    if (kind.find("Cubic") != std::string::npos) {
        return TextureProjection::kCubic;
    }
    return TextureProjection::kNone;
}

void parse_srfs_chunk(const unsigned char* data, std::size_t size, std::vector<std::string>* out_names) {
    std::size_t p = 0;
    while (p < size) {
        const auto [name, next] = read_cstr_even(data, size, p);
        if (next <= p) {
            break;
        }
        if (!name.empty()) {
            out_names->push_back(name);
        }
        p = next;
    }
}

void parse_surf_chunk(const unsigned char* data, std::size_t size, Assets& assets,
                      std::unordered_map<std::string, Surface>* out_surfaces) {
    const auto [name, after_name] = read_cstr_even(data,size,0);
    if(name.empty()) return;
    Surface s; s.name=name;
    s.chroma=name.find("CHROMA")!=std::string::npos;
    s.unlit=name.find("NOLIGHT")!=std::string::npos;
    int level=-1;
    for(std::size_t p=after_name;p+6<=size;) {
        std::string id(reinterpret_cast<const char*>(data+p),4);
        std::size_t n=be16(data+p+4),q=p+6;
        if(q+n>size) break;
        const auto* d=data+q;
        auto str=[&](){return std::string(reinterpret_cast<const char*>(d),
            static_cast<const unsigned char*>(std::memchr(d,0,n))?
            static_cast<const unsigned char*>(std::memchr(d,0,n))-d:n);};
        if(id=="COLR"&&n>=3) {s.color_r=d[0];s.color_g=d[1];s.color_b=d[2];}
        else if(id=="FLAG"&&n>=2) {s.flags=be16(d);s.additive=(s.flags&512)!=0;}
        else if(id=="VLUM"&&n>=4) s.luminosity=be_f32(d);
        else if(id=="VDIF"&&n>=4) s.diffuse=be_f32(d);
        else if(id=="VTRN"&&n>=4) s.transparency=be_f32(d);
        else if(id=="SMAN"&&n>=4) s.smoothing=be_f32(d);
        else if(id=="LUMI"&&n>=2) s.luminosity=be16(d)/256.f;
        else if(id=="DIFF"&&n>=2) s.diffuse=be16(d)/256.f;
        else if(id=="TRAN"&&n>=2) s.transparency=be16(d)/256.f;
        else if(id=="CTEX") {level=0;s.projection=projection_from_string(str());}
        else if(id=="RTEX") level=1;
        else if(id=="TTEX") {level=0;s.alpha_only=true;s.projection=projection_from_string(str());}
        else if(id=="BTEX"||id=="DTEX"||id=="STEX") level=-1;
        else if(id=="TIMG"&&n>0&&(level==0||level==1)) {
            auto path=str();
            if(path!="(none)"&&!path.empty()) {
                auto resolved=assets.resolve(path).u8string();
                if(level==0)s.texture_path=resolved;else s.reflection_path=resolved;
            }
        } else if(id=="RIMG"&&n>0&&str()!="(none)") s.reflection_path=assets.resolve(str()).u8string();
        else if(level==0) {
            if(id=="TFLG"&&n>=2)s.texture_flags=be16(d);
            else if(id=="TSIZ"&&n>=12)s.texture_size={be_f32(d),be_f32(d+4),be_f32(d+8)};
            else if(id=="TCTR"&&n>=12)s.texture_center={be_f32(d),-be_f32(d+4),be_f32(d+8)};
            else if(id=="TVEL"&&n>=12)s.velocity={be_f32(d),be_f32(d+4),be_f32(d+8)};
            else if(id=="TWRP"&&n>=4){s.wrap_u=be16(d);s.wrap_v=be16(d+2);}
        }
        p=q+n+(n&1);
    }
    (*out_surfaces)[name]=s;
}

void parse_pols_chunk(const unsigned char* data, std::size_t size, std::vector<RawFace>* out_faces) {
    std::size_t p = 0;
    while (p + 2 <= size) {
        const std::uint16_t nedge = be16(data + p);
        p += 2;
        if (nedge == 0) { break; }

        const std::size_t face_bytes = static_cast<std::size_t>(nedge) * 2;
        if (p + face_bytes + 2 > size) {
            break;
        }

        RawFace face;
        face.indices.reserve(nedge);
        for (std::uint16_t i = 0; i < nedge; ++i) {
            face.indices.push_back(be16(data + p));
            p += 2;
        }

        const std::int16_t surface_ref = static_cast<std::int16_t>(be16(data + p));
        p += 2;
        face.surface_index = (surface_ref < 0)
                                 ? static_cast<std::uint16_t>(-surface_ref)
                                 : static_cast<std::uint16_t>(surface_ref);

        if (face.indices.size() >= 3) {
            out_faces->push_back(std::move(face));
        }

        // LWOB detail polygons appear when the surface index is negative.
        if (surface_ref < 0 && p + 2 <= size) {
            const std::uint16_t ndetail = be16(data + p);
            p += 2;
            for (std::uint16_t d = 0; d < ndetail && p + 2 <= size; ++d) {
                const std::uint16_t dnedge = be16(data + p);
                p += 2;
                const std::size_t skip = static_cast<std::size_t>(dnedge) * 2 + 2;
                if (p + skip > size) {
                    p = size;
                    break;
                }
                p += skip;
            }
        }
    }
}

bool parse_lwob(const std::filesystem::path& path, Assets& assets, Mesh* mesh, std::string* error) {
    const auto bytes = read_binary(path);
    if (bytes.size() < 12) {
        if (error) {
            *error = "File too small for LWO: " + path.string();
        }
        return false;
    }

    if (std::memcmp(bytes.data(), "FORM", 4) != 0) {
        if (error) {
            *error = "Missing FORM header: " + path.string();
        }
        return false;
    }

    const std::string form_type(reinterpret_cast<const char*>(bytes.data() + 8), 4);
    if (form_type != "LWOB") {
        if (error) {
            *error = "Unsupported LWO form type '" + form_type + "' in " + path.string();
        }
        return false;
    }

    std::vector<std::string> srfs;
    std::unordered_map<std::string, Surface> parsed_surfaces;
    std::vector<RawFace> faces;

    std::size_t pos = 12;
    while (pos + 8 <= bytes.size()) {
        const char* id = reinterpret_cast<const char*>(bytes.data() + pos);
        const std::uint32_t chunk_size = be32(bytes.data() + pos + 4);
        const std::size_t data_pos = pos + 8;
        if (data_pos + chunk_size > bytes.size()) {
            if (error) {
                *error = "Corrupt chunk size in " + path.string();
            }
            return false;
        }

        if (std::memcmp(id, "PNTS", 4) == 0) {
            const std::size_t n = chunk_size / 12;
            mesh->vertices.reserve(mesh->vertices.size() + n);
            for (std::size_t i = 0; i < n; ++i) {
                const auto* p = bytes.data() + data_pos + i * 12;
                mesh->vertices.push_back({be_f32(p + 0), -be_f32(p + 4), be_f32(p + 8)});
            }
        } else if (std::memcmp(id, "SRFS", 4) == 0) {
            parse_srfs_chunk(bytes.data() + data_pos, chunk_size, &srfs);
        } else if (std::memcmp(id, "POLS", 4) == 0) {
            parse_pols_chunk(bytes.data() + data_pos, chunk_size, &faces);
        } else if (std::memcmp(id, "SURF", 4) == 0) {
            parse_surf_chunk(bytes.data() + data_pos, chunk_size, assets, &parsed_surfaces);
        }

        pos = data_pos + chunk_size + (chunk_size & 1U);
    }

    if (mesh->vertices.empty()) {
        if (error) {
            *error = "No geometry decoded from " + path.string();
        }
        return false;
    }

    std::unordered_map<std::string, std::int32_t> surface_name_to_index;
    for (const auto& name : srfs) {
        if (surface_name_to_index.find(name) != surface_name_to_index.end()) {
            continue;
        }
        Surface s;
        s.name = name;
        auto it = parsed_surfaces.find(name);
        if (it != parsed_surfaces.end()) {
            s = it->second;
            s.name = name;
        }
        const std::int32_t idx = static_cast<std::int32_t>(mesh->surfaces.size());
        surface_name_to_index.emplace(name, idx);
        mesh->surfaces.push_back(std::move(s));
    }

    for (const auto& [name, s] : parsed_surfaces) {
        if (surface_name_to_index.find(name) != surface_name_to_index.end()) {
            continue;
        }
        const std::int32_t idx = static_cast<std::int32_t>(mesh->surfaces.size());
        surface_name_to_index.emplace(name, idx);
        mesh->surfaces.push_back(s);
    }

    auto resolve_surface_index = [&](std::uint16_t srfs_index) -> std::int32_t {
        if (srfs_index == 0 || srfs_index > srfs.size()) {
            return -1;
        }
        const auto& name = srfs[srfs_index - 1];
        const auto it = surface_name_to_index.find(name);
        if (it == surface_name_to_index.end()) {
            return -1;
        }
        return it->second;
    };

    for (const auto& face : faces) {
        if (face.indices.size() < 3) {
            continue;
        }
        const std::int32_t tri_surface = resolve_surface_index(face.surface_index);
        for (std::size_t i = 1; i + 1 < face.indices.size(); ++i) {
            mesh->triangles.push_back(Triangle{
                static_cast<std::uint32_t>(face.indices[0]),
                static_cast<std::uint32_t>(face.indices[i]),
                static_cast<std::uint32_t>(face.indices[i + 1]),
                tri_surface,
            });
        }
    }

    return true;
}

} // namespace

namespace {
float safe(float s) { return std::abs(s)<1e-8f?1.f:s; }
Vec2 uv(Vec3 v, const Surface& s, Vec3 normal) {
    Vec3 p=v-s.texture_center;
    int axis=s.texture_flags&7;
    if(s.projection==TextureProjection::kCubic) {
        normal={std::abs(normal.x),std::abs(normal.y),std::abs(normal.z)};
        axis=normal.z>=normal.x&&normal.z>=normal.y?4:(normal.y>=normal.x?2:1);
    }
    float u=(axis==1?p.z/safe(s.texture_size.z):p.x/safe(s.texture_size.x))+.5f;
    // nX negates TSIZ.y as well as PNTS.y. With OpenGL's vertically flipped
    // image upload, the Y-axis projection therefore needs the opposite V sign.
    float vcoord=axis==2?.5f+p.z/safe(s.texture_size.z):.5f-p.y/safe(s.texture_size.y);
    if(s.projection==TextureProjection::kCylindrical) {
        constexpr float tau=6.28318530718f;
        float x=-p.x,z=p.z,t=-p.y/safe(s.texture_size.y)+.5f;
        if(axis==1){x=p.z;z=p.y;t=p.x/safe(s.texture_size.x)+.5f;}
        else if(axis==4){x=-p.x;z=p.y;t=p.z/safe(s.texture_size.z)+.5f;}
        // xyztoh from nX_Entity.cpp, with atan2 for the axis singularities.
        float h=std::atan2(-x,z);if(h<0)h+=tau;
        u=1-h/tau;vcoord=t;
    }
    return {u,vcoord};
}
}
bool load_mesh(const fs::path& path,Assets& assets,Mesh* mesh,std::string* error) {
    if(!parse_lwob(path,assets,mesh,error))return false;
    std::vector<std::vector<std::size_t>> adjacent(mesh->vertices.size());
    for(std::size_t i=0;i<mesh->triangles.size();++i) {
        auto& t=mesh->triangles[i];
        if(t.i0>=adjacent.size()||t.i1>=adjacent.size()||t.i2>=adjacent.size()) {
            *error="Invalid vertex index in "+path.u8string();return false;
        }
        t.normal=normalized(cross(mesh->vertices[t.i1]-mesh->vertices[t.i0],mesh->vertices[t.i2]-mesh->vertices[t.i0]));
        for(auto idx:{t.i0,t.i1,t.i2})adjacent[idx].push_back(i);
    }
    for(auto& t:mesh->triangles) {
        Surface fallback;const Surface& s=t.surface_index>=0?mesh->surfaces.at(t.surface_index):fallback;
        std::array<std::uint32_t,3> indices{t.i0,t.i1,t.i2};
        for(int i=0;i<3;++i) {
            t.uv[i]=uv(mesh->vertices[indices[i]],s,t.normal);
            Vec3 n=t.normal;
            if(s.flags&4) {
                n={};
                for(auto j:adjacent[indices[i]]) {
                    auto next=mesh->triangles[j].normal;
                    if(dot(next,t.normal)>=std::cos(s.smoothing)-1e-5f)n=n+next;
                }
            }
            t.normals[i]=normalized(n);
        }
        if(s.projection==TextureProjection::kCylindrical) {
            float lo=std::min({t.uv[0].u,t.uv[1].u,t.uv[2].u});
            float hi=std::max({t.uv[0].u,t.uv[1].u,t.uv[2].u});
            if(hi-lo>.5f)for(auto& v:t.uv)if(v.u<.5f)v.u+=1;
        }
    }
    return true;
}
} // namespace nxng
