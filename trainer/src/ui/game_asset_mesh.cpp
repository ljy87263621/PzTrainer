#include "ui/game_asset_mesh.hpp"

#include <Windows.h>
#include <GL/gl.h>
#include <wincodec.h>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <limits>
#include <regex>
#include <sstream>
#include <string>
#include <vector>

namespace pztrainer::ui {
namespace {

struct Vec3 {
    float x = 0.0f, y = 0.0f, z = 0.0f;
    float u = 0.0f, v = 0.0f;
};
struct Triangle { std::uint32_t a = 0, b = 0, c = 0; };
struct Projected { ImVec2 point{}; ImVec2 uv{}; float depth = 0.0f; };

std::filesystem::path GameDirectory() {
    wchar_t path[MAX_PATH]{};
    GetModuleFileNameW(nullptr, path, MAX_PATH);
    return std::filesystem::path(path).parent_path();
}

bool ReadFile(const std::filesystem::path& path, std::string& text) {
    std::ifstream stream(path, std::ios::binary);
    if (!stream) return false;
    std::ostringstream buffer;
    buffer << stream.rdbuf();
    text = buffer.str();
    return !text.empty();
}

std::vector<float> ParseFloats(const std::string& text) {
    static const std::regex pattern(
        R"([-+]?(?:\d+\.?\d*|\.\d+)(?:[eE][-+]?\d+)?)");
    std::vector<float> values;
    for (std::sregex_iterator it(text.begin(), text.end(), pattern), end;
         it != end; ++it) {
        values.push_back(std::stof((*it)[0].str()));
    }
    return values;
}

bool ExtractBraceBlock(const std::string& text, std::size_t marker,
                       std::string& block) {
    const std::size_t open = text.find('{', marker);
    if (open == std::string::npos) return false;
    int depth = 0;
    for (std::size_t index = open; index < text.size(); ++index) {
        if (text[index] == '{') ++depth;
        else if (text[index] == '}' && --depth == 0) {
            block = text.substr(open + 1, index - open - 1);
            return true;
        }
    }
    return false;
}

bool ParseXMesh(const std::filesystem::path& path,
                std::vector<Vec3>& vertices,
                std::vector<Triangle>& triangles) {
    std::string text;
    if (!ReadFile(path, text)) return false;
    std::smatch match;
    const std::regex mesh_pattern(R"(\n\s*Mesh\s+[A-Za-z0-9_]+\s*\{)");
    if (!std::regex_search(text, match, mesh_pattern)) return false;
    const std::size_t marker = static_cast<std::size_t>(match.position());
    std::string block;
    if (!ExtractBraceBlock(text, marker, block)) return false;

    const std::regex count_pattern(R"(^\s*(\d+)\s*;)");
    std::smatch count_match;
    if (!std::regex_search(block, count_match, count_pattern)) return false;
    const std::size_t vertex_count = static_cast<std::size_t>(
        std::stoul(count_match[1].str()));
    const std::string after_count = block.substr(
        static_cast<std::size_t>(count_match.position() + count_match.length()));
    const std::regex vertex_pattern(
        R"(([-+0-9.eE]+)\s*;\s*([-+0-9.eE]+)\s*;\s*([-+0-9.eE]+)\s*;\s*[,;])");
    std::size_t last_vertex_end = 0;
    for (std::sregex_iterator it(after_count.begin(), after_count.end(), vertex_pattern), end;
         it != end && vertices.size() < vertex_count; ++it) {
        vertices.push_back({std::stof((*it)[1].str()), std::stof((*it)[2].str()),
                            std::stof((*it)[3].str()), 0.0f, 0.0f});
        last_vertex_end = static_cast<std::size_t>(it->position() + it->length());
    }
    if (vertices.size() != vertex_count) return false;
    const std::string face_section = after_count.substr(last_vertex_end);
    const std::regex face_count_pattern(R"(^\s*(\d+)\s*;)");
    if (!std::regex_search(face_section, count_match, face_count_pattern)) return false;
    const std::size_t face_count = static_cast<std::size_t>(
        std::stoul(count_match[1].str()));
    const std::string faces = face_section.substr(
        static_cast<std::size_t>(count_match.position() + count_match.length()));
    const std::regex face_pattern(
        R"((\d+)\s*;\s*([0-9,\s]+)\s*;\s*[,;])");
    std::size_t parsed_faces = 0;
    for (std::sregex_iterator it(faces.begin(), faces.end(), face_pattern), end;
         it != end && parsed_faces < face_count; ++it, ++parsed_faces) {
        const int count = std::stoi((*it)[1].str());
        const std::vector<float> indices = ParseFloats((*it)[2].str());
        if (count < 3 || indices.size() < static_cast<std::size_t>(count)) continue;
        const std::uint32_t first = static_cast<std::uint32_t>(indices[0]);
        for (int index = 1; index + 1 < count; ++index) {
            triangles.push_back({first,
                static_cast<std::uint32_t>(indices[index]),
                static_cast<std::uint32_t>(indices[index + 1])});
        }
    }
    const std::size_t texture_marker = block.find("MeshTextureCoords");
    std::string texture_block;
    if (texture_marker != std::string::npos &&
        ExtractBraceBlock(block, texture_marker, texture_block)) {
        const std::vector<float> texture_values = ParseFloats(texture_block);
        if (texture_values.size() >= 1 + vertex_count * 2) {
            for (std::size_t index = 0; index < vertex_count; ++index) {
                vertices[index].u = texture_values[1 + index * 2];
                vertices[index].v = texture_values[2 + index * 2];
            }
        }
    }
    return parsed_faces == face_count && !triangles.empty();
}

bool ExtractFbxArray(const std::string& text, const char* marker,
                     std::string& array) {
    const std::size_t start = text.find(marker);
    if (start == std::string::npos) return false;
    const std::size_t data = text.find("a:", start);
    const std::size_t end = data == std::string::npos ? std::string::npos
                                                       : text.find('}', data);
    if (data == std::string::npos || end == std::string::npos) return false;
    array = text.substr(data + 2, end - data - 2);
    return true;
}

bool ParseFbxMesh(const std::filesystem::path& path,
                  std::vector<Vec3>& vertices,
                  std::vector<Triangle>& triangles) {
    std::string text, vertex_array, index_array, uv_array, uv_index_array;
    if (!ReadFile(path, text) ||
        !ExtractFbxArray(text, "Vertices:", vertex_array) ||
        !ExtractFbxArray(text, "PolygonVertexIndex:", index_array) ||
        !ExtractFbxArray(text, "UV:", uv_array) ||
        !ExtractFbxArray(text, "UVIndex:", uv_index_array)) return false;
    const std::vector<float> values = ParseFloats(vertex_array);
    const std::vector<float> uv_values = ParseFloats(uv_array);
    if (values.size() < 9 || values.size() % 3 != 0) return false;
    for (std::size_t index = 0; index < values.size(); index += 3) {
        vertices.push_back({values[index], values[index + 2], values[index + 1],
                            0.0f, 0.0f});
    }
    static const std::regex integer_pattern(R"(-?\d+)");
    std::vector<int> uv_indices;
    for (std::sregex_iterator it(uv_index_array.begin(), uv_index_array.end(),
                                 integer_pattern), end; it != end; ++it) {
        uv_indices.push_back(std::stoi((*it)[0].str()));
    }
    const std::vector<Vec3> source_vertices = vertices;
    vertices.clear();
    std::vector<std::uint32_t> polygon;
    std::size_t polygon_vertex = 0;
    for (std::sregex_iterator it(index_array.begin(), index_array.end(), integer_pattern), end;
         it != end; ++it) {
        int value = std::stoi((*it)[0].str());
        const bool polygon_end = value < 0;
        if (polygon_end) value = -value - 1;
        if (value < 0 || value >= static_cast<int>(source_vertices.size())) return false;
        Vec3 vertex = source_vertices[static_cast<std::size_t>(value)];
        if (polygon_vertex < uv_indices.size()) {
            const int uv_index = uv_indices[polygon_vertex];
            if (uv_index >= 0 &&
                static_cast<std::size_t>(uv_index * 2 + 1) < uv_values.size()) {
                vertex.u = uv_values[static_cast<std::size_t>(uv_index) * 2];
                vertex.v = 1.0f - uv_values[static_cast<std::size_t>(uv_index) * 2 + 1];
            }
        }
        ++polygon_vertex;
        polygon.push_back(static_cast<std::uint32_t>(vertices.size()));
        vertices.push_back(vertex);
        if (polygon_end) {
            for (std::size_t index = 1; index + 1 < polygon.size(); ++index) {
                triangles.push_back({polygon[0], polygon[index], polygon[index + 1]});
            }
            polygon.clear();
        }
    }
    return !vertices.empty() && !triangles.empty();
}

bool LoadTexture(const std::filesystem::path& path, GLuint& texture) {
    IWICImagingFactory* factory = nullptr;
    IWICBitmapDecoder* decoder = nullptr;
    IWICBitmapFrameDecode* frame = nullptr;
    IWICFormatConverter* converter = nullptr;
    const HRESULT initialization = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
    const bool uninitialize = SUCCEEDED(initialization);
    HRESULT result = CoCreateInstance(
        CLSID_WICImagingFactory, nullptr, CLSCTX_INPROC_SERVER,
        IID_PPV_ARGS(&factory));
    if (SUCCEEDED(result)) result = factory->CreateDecoderFromFilename(
        path.c_str(), nullptr, GENERIC_READ, WICDecodeMetadataCacheOnLoad, &decoder);
    if (SUCCEEDED(result)) result = decoder->GetFrame(0, &frame);
    if (SUCCEEDED(result)) result = factory->CreateFormatConverter(&converter);
    if (SUCCEEDED(result)) result = converter->Initialize(
        frame, GUID_WICPixelFormat32bppRGBA, WICBitmapDitherTypeNone,
        nullptr, 0.0f, WICBitmapPaletteTypeCustom);
    UINT width = 0, height = 0;
    if (SUCCEEDED(result)) result = converter->GetSize(&width, &height);
    std::vector<std::uint8_t> pixels;
    if (SUCCEEDED(result) && width > 0 && height > 0) {
        pixels.resize(static_cast<std::size_t>(width) * height * 4);
        result = converter->CopyPixels(
            nullptr, width * 4, static_cast<UINT>(pixels.size()), pixels.data());
    }
    if (SUCCEEDED(result)) {
        glGenTextures(1, &texture);
        glBindTexture(GL_TEXTURE_2D, texture);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, static_cast<GLsizei>(width),
                     static_cast<GLsizei>(height), 0, GL_RGBA,
                     GL_UNSIGNED_BYTE, pixels.data());
        glBindTexture(GL_TEXTURE_2D, 0);
    }
    if (converter != nullptr) converter->Release();
    if (frame != nullptr) frame->Release();
    if (decoder != nullptr) decoder->Release();
    if (factory != nullptr) factory->Release();
    if (uninitialize) CoUninitialize();
    return SUCCEEDED(result) && texture != 0;
}

void Normalize(std::vector<Vec3>& vertices) {
    Vec3 minimum{std::numeric_limits<float>::max(), std::numeric_limits<float>::max(),
                 std::numeric_limits<float>::max()};
    Vec3 maximum{-minimum.x, -minimum.y, -minimum.z};
    for (const Vec3& vertex : vertices) {
        minimum.x = std::min(minimum.x, vertex.x);
        minimum.y = std::min(minimum.y, vertex.y);
        minimum.z = std::min(minimum.z, vertex.z);
        maximum.x = std::max(maximum.x, vertex.x);
        maximum.y = std::max(maximum.y, vertex.y);
        maximum.z = std::max(maximum.z, vertex.z);
    }
    const Vec3 center{(minimum.x + maximum.x) * 0.5f,
                      (minimum.y + maximum.y) * 0.5f,
                      (minimum.z + maximum.z) * 0.5f};
    const float extent = std::max({maximum.x - minimum.x, maximum.y - minimum.y,
                                   maximum.z - minimum.z, 0.001f});
    for (Vec3& vertex : vertices) {
        vertex.x = (vertex.x - center.x) / extent;
        vertex.y = (vertex.y - center.y) / extent;
        vertex.z = (vertex.z - center.z) / extent;
    }
}

ImVec4 EffectColor(ImVec4 color,
                   features::visual::ZombieModelEffect effect,
                   float shade, std::size_t triangle) {
    const float time = static_cast<float>(ImGui::GetTime());
    if (effect == features::visual::ZombieModelEffect::Iridescent) {
        color.x = 0.52f + 0.42f * std::sin(time * 1.5f + triangle * 0.013f);
        color.y = 0.52f + 0.42f * std::sin(time * 1.5f + triangle * 0.013f + 2.1f);
        color.z = 0.52f + 0.42f * std::sin(time * 1.5f + triangle * 0.013f + 4.2f);
    } else if (effect == features::visual::ZombieModelEffect::WaterFlow) {
        const float wave = 0.72f + 0.28f * std::sin(time * 2.4f + triangle * 0.025f);
        color.x *= wave; color.y *= wave; color.z = std::min(1.0f, color.z * 1.18f);
    } else if (effect == features::visual::ZombieModelEffect::Glow ||
               effect == features::visual::ZombieModelEffect::GlowOutline) {
        shade = std::max(shade, 0.86f);
    } else if (effect == features::visual::ZombieModelEffect::Solid) {
        shade = 1.0f;
    } else if (effect == features::visual::ZombieModelEffect::Glossy) {
        shade = std::min(1.18f, shade + 0.22f);
    }
    color.x = std::clamp(color.x * shade, 0.0f, 1.0f);
    color.y = std::clamp(color.y * shade, 0.0f, 1.0f);
    color.z = std::clamp(color.z * shade, 0.0f, 1.0f);
    if (effect == features::visual::ZombieModelEffect::Disabled) color.w *= 0.72f;
    return color;
}

}  // namespace

struct GameAssetMesh::Impl {
    std::vector<Vec3> vertices;
    std::vector<Triangle> triangles;
    GLuint texture = 0;
};

GameAssetMesh::~GameAssetMesh() {
    delete impl_;
}

bool GameAssetMesh::EnsureLoaded() {
    if (impl_ != nullptr) return true;
    if (attempted_) return false;
    attempted_ = true;
    Impl* loaded = new Impl();
    const std::filesystem::path root = GameDirectory();
    bool succeeded = asset_ == Asset::Animal
        ? ParseXMesh(root / L"media/models_X/Skinned/CowBody.x",
                     loaded->vertices, loaded->triangles)
        : ParseFbxMesh(root / L"media/models_X/vehicles/Vehicles_CarNormal.fbx",
                       loaded->vertices, loaded->triangles);
    const std::filesystem::path texture = asset_ == Asset::Animal
        ? root / L"media/textures/Body/Cow_BW_01.png"
        : root / L"media/textures/Vehicles/vehicle_carnormalshell.png";
    succeeded = succeeded && LoadTexture(texture, loaded->texture);
    if (!succeeded) {
        delete loaded;
        return false;
    }
    Normalize(loaded->vertices);
    impl_ = loaded;
    return true;
}

bool GameAssetMesh::Draw(ImDrawList* draw, const ImVec2& center,
                         const ImVec2& size, float yaw, int animation,
                         features::visual::ZombieModelEffect effect,
                         const ImVec4& color, bool wireframe,
                         bool edge_glow, const ImVec4& edge_color,
                         ImVec2& bounds_min, ImVec2& bounds_max) const {
    if (impl_ == nullptr) return false;
    const float motion = asset_ == Asset::Animal
        ? std::sin(static_cast<float>(ImGui::GetTime()) *
            (animation == 2 ? 8.0f : animation == 1 ? 4.0f : 1.8f)) *
            (animation == 0 ? 0.004f : 0.015f)
        : 0.0f;
    const float cosine = std::cos(yaw), sine = std::sin(yaw);
    const float scale = std::min(size.x, size.y) *
        (asset_ == Asset::Animal ? 1.44f : 1.50f);
    std::vector<Projected> projected;
    projected.reserve(impl_->vertices.size());
    bounds_min = ImVec2(std::numeric_limits<float>::max(),
                        std::numeric_limits<float>::max());
    bounds_max = ImVec2(-bounds_min.x, -bounds_min.y);
    for (const Vec3& vertex : impl_->vertices) {
        const float x = vertex.x * cosine - vertex.z * sine;
        const float z = vertex.x * sine + vertex.z * cosine;
        const float perspective = 1.8f / (1.8f + z);
        Projected output{
            ImVec2(center.x + x * scale * perspective,
                   center.y - (vertex.y + motion) * scale * perspective),
            ImVec2(vertex.u, vertex.v), z};
        bounds_min.x = std::min(bounds_min.x, output.point.x);
        bounds_min.y = std::min(bounds_min.y, output.point.y);
        bounds_max.x = std::max(bounds_max.x, output.point.x);
        bounds_max.y = std::max(bounds_max.y, output.point.y);
        projected.push_back(output);
    }
    std::vector<std::size_t> order(impl_->triangles.size());
    for (std::size_t index = 0; index < order.size(); ++index) order[index] = index;
    std::sort(order.begin(), order.end(), [&](std::size_t left, std::size_t right) {
        const Triangle& a = impl_->triangles[left];
        const Triangle& b = impl_->triangles[right];
        return projected[a.a].depth + projected[a.b].depth + projected[a.c].depth >
               projected[b.a].depth + projected[b.b].depth + projected[b.c].depth;
    });
    const bool textured = effect == features::visual::ZombieModelEffect::Disabled ||
        effect == features::visual::ZombieModelEffect::Shaded ||
        effect == features::visual::ZombieModelEffect::GlowOutline ||
        effect == features::visual::ZombieModelEffect::WaterFlow ||
        effect == features::visual::ZombieModelEffect::Glossy;
    if (textured) {
        draw->PushTexture(ImTextureRef(static_cast<ImTextureID>(impl_->texture)));
    }
    for (std::size_t draw_index = 0; draw_index < order.size(); ++draw_index) {
        const Triangle& triangle = impl_->triangles[order[draw_index]];
        if (triangle.a >= projected.size() || triangle.b >= projected.size() ||
            triangle.c >= projected.size()) continue;
        const ImVec2 points[]{projected[triangle.a].point,
                             projected[triangle.b].point,
                             projected[triangle.c].point};
        const float cross = (points[1].x - points[0].x) *
                                (points[2].y - points[0].y) -
                            (points[1].y - points[0].y) *
                                (points[2].x - points[0].x);
        const float shade = 0.58f + 0.40f *
            std::clamp(std::abs(cross) / 180.0f, 0.0f, 1.0f);
        ImVec4 face_color = EffectColor(color, effect, shade, order[draw_index]);
        if (effect == features::visual::ZombieModelEffect::Disabled) {
            face_color = ImVec4(shade, shade, shade, 1.0f);
        }
        if (textured) {
            const ImU32 packed = ImGui::GetColorU32(face_color);
            draw->PrimReserve(3, 3);
            draw->PrimVtx(projected[triangle.a].point,
                          projected[triangle.a].uv, packed);
            draw->PrimVtx(projected[triangle.b].point,
                          projected[triangle.b].uv, packed);
            draw->PrimVtx(projected[triangle.c].point,
                          projected[triangle.c].uv, packed);
        } else {
            draw->AddTriangleFilled(points[0], points[1], points[2],
                                    ImGui::GetColorU32(face_color));
        }
        if (wireframe) {
            ImVec4 wire = color; wire.w = 0.38f;
            draw->AddTriangle(points[0], points[1], points[2],
                              ImGui::GetColorU32(wire), 0.65f);
        }
        if (edge_glow && draw_index % 3 == 0) {
            ImVec4 glow = edge_color; glow.w *= 0.38f;
            draw->AddTriangle(points[0], points[1], points[2],
                              ImGui::GetColorU32(glow), 1.0f);
        }
    }
    if (textured) draw->PopTexture();
    return true;
}

}  // namespace pztrainer::ui
