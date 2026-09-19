#include "ui/game_zombie_mesh.hpp"

#include <Windows.h>
#include <gl/GL.h>
#include <wincodec.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <cfloat>
#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <regex>
#include <sstream>
#include <string>
#include <unordered_map>
#include <vector>

namespace pztrainer::ui {
namespace {

struct Vec3 { float x = 0.0f; float y = 0.0f; float z = 0.0f; };
struct Quaternion { float w = 1.0f; float x = 0.0f; float y = 0.0f; float z = 0.0f; };

struct Matrix {
    float value[4][4]{};
};

struct Influence { int bone = -1; float weight = 0.0f; };
struct Vertex {
    Vec3 position;
    float u = 0.0f;
    float v = 0.0f;
    std::array<Influence, 4> influences{};
};
struct Triangle { std::array<std::uint32_t, 3> indices{}; };
struct VecKey { float time = 0.0f; Vec3 value; };
struct QuatKey { float time = 0.0f; Quaternion value; };
struct BoneTrack { std::vector<VecKey> scale; std::vector<QuatKey> rotation; std::vector<VecKey> translation; };
struct AnimationClip { std::unordered_map<std::string, BoneTrack> tracks; float duration = 4800.0f; float ticks_per_second = 4800.0f; };
struct Bone { std::string name; int parent = -1; Matrix bind_local{}; Matrix offset{}; };
struct ProjectedVertex { ImVec2 position; ImVec2 uv; float depth = 0.0f; };
struct ProjectedTriangle { std::array<ProjectedVertex, 3> vertices; float depth = 0.0f; float light = 1.0f; };

ImVec4 MultiplyLight(const ImVec4& color, float light) {
    return ImVec4(
        std::clamp(color.x * light, 0.0f, 1.0f),
        std::clamp(color.y * light, 0.0f, 1.0f),
        std::clamp(color.z * light, 0.0f, 1.0f),
        color.w);
}

ImVec4 Mix(const ImVec4& first, const ImVec4& second, float amount) {
    amount = std::clamp(amount, 0.0f, 1.0f);
    return ImVec4(
        first.x + (second.x - first.x) * amount,
        first.y + (second.y - first.y) * amount,
        first.z + (second.z - first.z) * amount,
        first.w + (second.w - first.w) * amount);
}

ImVec4 Hsv(float hue, float saturation, float value, float alpha) {
    float red = 1.0f;
    float green = 1.0f;
    float blue = 1.0f;
    hue -= std::floor(hue);
    ImGui::ColorConvertHSVtoRGB(hue, saturation, value, red, green, blue);
    return ImVec4(red, green, blue, alpha);
}

ImVec2 Expanded(const ImVec2& point, const ImVec2& center, float amount) {
    return ImVec2(center.x + (point.x - center.x) * amount,
                  center.y + (point.y - center.y) * amount);
}

void DrawSolidPass(ImDrawList* draw, const std::vector<ProjectedTriangle>& triangles,
                   const ImVec4& color, const ImVec2& center, float expansion) {
    const ImU32 packed = ImGui::GetColorU32(color);
    for (const ProjectedTriangle& triangle : triangles) {
        draw->AddTriangleFilled(
            Expanded(triangle.vertices[0].position, center, expansion),
            Expanded(triangle.vertices[1].position, center, expansion),
            Expanded(triangle.vertices[2].position, center, expansion), packed);
    }
}

constexpr std::array<const char*, 4> kAnimationFiles{{
    "Zombie_Idle.x", "Zombie_Walk.x", "Zombie_Sprint.x", "Zombie_Idle_Lunge.x"
}};
constexpr std::array<const char*, 4> kPlayerAnimationFiles{{
    "Bob_Idle.X", "Bob_Walk.x", "Bob_Run.X", "Bob_Sprint.X"
}};

const std::array<std::pair<const char*, const char*>, 28> kBoneHierarchy{{
    {"Bip01_Spine", "Bip01_Pelvis"}, {"Bip01_Spine1", "Bip01_Spine"},
    {"Bip01_Neck", "Bip01_Spine1"}, {"Bip01_Head", "Bip01_Neck"},
    {"Bip01_Pelvis", "Bip01"}, {"Bip01_R_Hand", "Bip01_R_Forearm"},
    {"Bip01_R_Thigh", "Bip01_Pelvis"}, {"Bip01_R_Forearm", "Bip01_R_UpperArm"},
    {"Bip01_L_Calf", "Bip01_L_Thigh"}, {"Bip01_R_Calf", "Bip01_R_Thigh"},
    {"Bip01_L_Foot", "Bip01_L_Calf"}, {"Bip01_R_Finger1", "Bip01_R_Hand"},
    {"Bip01_BackPack", "Bip01_Spine1"}, {"Bip01_R_UpperArm", "Bip01_R_Clavicle"},
    {"Bip01_R_Finger0", "Bip01_R_Hand"}, {"Bip01_L_Thigh", "Bip01_Pelvis"},
    {"Bip01_R_Clavicle", "Bip01_Neck"}, {"Bip01_L_Clavicle", "Bip01_Neck"},
    {"Bip01_L_UpperArm", "Bip01_L_Clavicle"}, {"Bip01_L_Forearm", "Bip01_L_UpperArm"},
    {"Bip01_L_Finger1", "Bip01_L_Hand"}, {"Bip01_L_Finger0", "Bip01_L_Hand"},
    {"Bip01_L_Hand", "Bip01_L_Forearm"}, {"Bip01_R_Foot", "Bip01_R_Calf"},
    {"Bip01_DressBack", "Bip01_Pelvis"}, {"Bip01_DressBack02", "Bip01_DressBack"},
    {"Bip01_DressFront02", "Bip01_DressFront"}, {"Bip01_DressFront", "Bip01_Pelvis"},
}};

Matrix Identity() {
    Matrix result{};
    for (int index = 0; index < 4; ++index) result.value[index][index] = 1.0f;
    return result;
}

Matrix Multiply(const Matrix& left, const Matrix& right) {
    Matrix result{};
    for (int row = 0; row < 4; ++row) {
        for (int column = 0; column < 4; ++column) {
            for (int index = 0; index < 4; ++index) {
                result.value[row][column] += left.value[row][index] * right.value[index][column];
            }
        }
    }
    return result;
}

Vec3 Transform(const Vec3& point, const Matrix& matrix) {
    return {
        point.x * matrix.value[0][0] + point.y * matrix.value[1][0] +
            point.z * matrix.value[2][0] + matrix.value[3][0],
        point.x * matrix.value[0][1] + point.y * matrix.value[1][1] +
            point.z * matrix.value[2][1] + matrix.value[3][1],
        point.x * matrix.value[0][2] + point.y * matrix.value[1][2] +
            point.z * matrix.value[2][2] + matrix.value[3][2],
    };
}

Matrix Compose(const Vec3& scale, Quaternion rotation, const Vec3& translation) {
    const float length = std::sqrt(rotation.w * rotation.w + rotation.x * rotation.x +
                                   rotation.y * rotation.y + rotation.z * rotation.z);
    if (length > 0.00001f) {
        rotation.w /= length; rotation.x /= length; rotation.y /= length; rotation.z /= length;
    }
    const float w = rotation.w, x = rotation.x, y = rotation.y, z = rotation.z;
    Matrix result = Identity();
    result.value[0][0] = (1.0f - 2.0f * (y * y + z * z)) * scale.x;
    result.value[0][1] = (2.0f * (x * y - z * w)) * scale.x;
    result.value[0][2] = (2.0f * (x * z + y * w)) * scale.x;
    result.value[1][0] = (2.0f * (x * y + z * w)) * scale.y;
    result.value[1][1] = (1.0f - 2.0f * (x * x + z * z)) * scale.y;
    result.value[1][2] = (2.0f * (y * z - x * w)) * scale.y;
    result.value[2][0] = (2.0f * (x * z - y * w)) * scale.z;
    result.value[2][1] = (2.0f * (y * z + x * w)) * scale.z;
    result.value[2][2] = (1.0f - 2.0f * (x * x + y * y)) * scale.z;
    result.value[3][0] = translation.x;
    result.value[3][1] = translation.y;
    result.value[3][2] = translation.z;
    return result;
}

std::filesystem::path GameDirectory() {
    wchar_t executable[MAX_PATH]{};
    const DWORD length = GetModuleFileNameW(nullptr, executable, MAX_PATH);
    return length == 0 ? std::filesystem::path{} : std::filesystem::path(executable).parent_path();
}

bool ReadFile(const std::filesystem::path& path, std::string& result) {
    std::ifstream input(path, std::ios::binary);
    if (!input) return false;
    std::ostringstream stream;
    stream << input.rdbuf();
    result = stream.str();
    return !result.empty();
}

bool ExtractBlockAt(const std::string& text, std::size_t marker_position, std::string& block,
                    std::size_t* block_end = nullptr) {
    const std::size_t opening = text.find('{', marker_position);
    if (opening == std::string::npos) return false;
    int depth = 1;
    for (std::size_t index = opening + 1; index < text.size(); ++index) {
        if (text[index] == '{') ++depth;
        if (text[index] == '}' && --depth == 0) {
            block.assign(text, opening + 1, index - opening - 1);
            if (block_end != nullptr) *block_end = index + 1;
            return true;
        }
    }
    return false;
}

bool ExtractBlock(const std::string& text, const std::string& marker, std::string& block) {
    const std::size_t position = text.find(marker);
    return position != std::string::npos && ExtractBlockAt(text, position, block);
}

std::vector<float> ParseNumbers(const std::string& text) {
    static const std::regex pattern(R"([-+]?(?:\d+\.?\d*|\.\d+)(?:[eE][-+]?\d+)?)");
    std::vector<float> result;
    for (std::sregex_iterator it(text.begin(), text.end(), pattern), end; it != end; ++it) {
        const std::string token = (*it)[0].str();
        char* parse_end = nullptr;
        const float value = std::strtof(token.c_str(), &parse_end);
        if (parse_end == token.c_str() || !std::isfinite(value)) return {};
        result.push_back(value);
    }
    return result;
}

bool ParseMatrix(const std::string& text, const std::string& frame_name, Matrix& matrix) {
    const std::string marker = "Frame " + frame_name + " {";
    const std::size_t frame = text.find(marker);
    if (frame == std::string::npos) return false;
    const std::size_t transform = text.find("FrameTransformMatrix", frame);
    std::string block;
    if (transform == std::string::npos || !ExtractBlockAt(text, transform, block)) return false;
    const std::vector<float> numbers = ParseNumbers(block);
    if (numbers.size() < 16) return false;
    for (int index = 0; index < 16; ++index) matrix.value[index / 4][index % 4] = numbers[index];
    return true;
}

bool ParseMesh(const std::filesystem::path& path, std::vector<Vertex>& vertices,
               std::vector<Triangle>& triangles, std::vector<Bone>& bones,
               std::unordered_map<std::string, int>& bone_indices) {
    std::string text, mesh_block, uv_block;
    if (!ReadFile(path, text) || !ExtractBlock(text, "Mesh Body", mesh_block) ||
        !ExtractBlock(text, "MeshTextureCoords c1", uv_block)) return false;

    const std::regex vertex_pattern(R"(([-+0-9.eE]+)\s*;\s*([-+0-9.eE]+)\s*;\s*([-+0-9.eE]+)\s*;\s*[,;])");
    const std::regex face_pattern(R"(3\s*;\s*(\d+)\s*,\s*(\d+)\s*,\s*(\d+)\s*;\s*[,;])");
    const std::regex uv_pattern(R"(([-+0-9.eE]+)\s*;\s*([-+0-9.eE]+)\s*;\s*[,;])");
    std::vector<Vec3> positions;
    for (std::sregex_iterator it(mesh_block.begin(), mesh_block.end(), vertex_pattern), end; it != end; ++it) {
        positions.push_back({std::stof((*it)[1].str()), std::stof((*it)[2].str()), std::stof((*it)[3].str())});
        if (positions.size() == 617) break;
    }
    for (std::sregex_iterator it(mesh_block.begin(), mesh_block.end(), face_pattern), end; it != end; ++it) {
        triangles.push_back({{{static_cast<std::uint32_t>(std::stoul((*it)[1].str())),
                               static_cast<std::uint32_t>(std::stoul((*it)[2].str())),
                               static_cast<std::uint32_t>(std::stoul((*it)[3].str()))}}});
        if (triangles.size() == 916) break;
    }
    std::vector<std::array<float, 2>> texture_coordinates;
    for (std::sregex_iterator it(uv_block.begin(), uv_block.end(), uv_pattern), end; it != end; ++it) {
        texture_coordinates.push_back({std::stof((*it)[1].str()), std::stof((*it)[2].str())});
        if (texture_coordinates.size() == 617) break;
    }
    if (positions.size() != 617 || texture_coordinates.size() != positions.size() || triangles.size() != 916) return false;
    vertices.resize(positions.size());
    for (std::size_t index = 0; index < positions.size(); ++index) {
        vertices[index].position = positions[index];
        vertices[index].u = texture_coordinates[index][0];
        vertices[index].v = texture_coordinates[index][1];
    }

    std::unordered_map<std::string, std::string> parents;
    parents["Bip01"] = "Dummy01";
    parents["Bip01_Pelvis"] = "Bip01";
    for (const auto& entry : kBoneHierarchy) parents[entry.first] = entry.second;
    const std::array<const char*, 30> ordered_names{{
        "Dummy01", "Bip01", "Bip01_Pelvis", "Bip01_Spine", "Bip01_Spine1", "Bip01_Neck", "Bip01_Head",
        "Bip01_L_Clavicle", "Bip01_L_UpperArm", "Bip01_L_Forearm", "Bip01_L_Hand", "Bip01_L_Finger0", "Bip01_L_Finger1",
        "Bip01_R_Clavicle", "Bip01_R_UpperArm", "Bip01_R_Forearm", "Bip01_R_Hand", "Bip01_R_Finger0", "Bip01_R_Finger1",
        "Bip01_L_Thigh", "Bip01_L_Calf", "Bip01_L_Foot", "Bip01_R_Thigh", "Bip01_R_Calf", "Bip01_R_Foot",
        "Bip01_BackPack", "Bip01_DressFront", "Bip01_DressFront02", "Bip01_DressBack", "Bip01_DressBack02"
    }};
    for (const char* name : ordered_names) {
        Bone bone{};
        bone.name = name;
        bone.bind_local = Identity();
        bone.offset = Identity();
        ParseMatrix(text, name, bone.bind_local);
        const auto parent = parents.find(name);
        if (parent != parents.end()) {
            const auto found = bone_indices.find(parent->second);
            bone.parent = found == bone_indices.end() ? -1 : found->second;
        }
        bone_indices[bone.name] = static_cast<int>(bones.size());
        bones.push_back(bone);
    }

    std::size_t position = 0;
    while ((position = mesh_block.find("SkinWeights", position)) != std::string::npos) {
        std::string block;
        std::size_t end = position;
        if (!ExtractBlockAt(mesh_block, position, block, &end)) break;
        const std::size_t quote_start = block.find('"');
        const std::size_t quote_end = block.find('"', quote_start + 1);
        if (quote_start != std::string::npos && quote_end != std::string::npos) {
            const std::string name = block.substr(quote_start + 1, quote_end - quote_start - 1);
            const auto bone_it = bone_indices.find(name);
            const std::vector<float> numbers = ParseNumbers(block.substr(quote_end + 1));
            if (bone_it != bone_indices.end() && !numbers.empty()) {
                const int count = static_cast<int>(numbers[0]);
                if (count >= 0 && numbers.size() >= static_cast<std::size_t>(1 + count * 2 + 16)) {
                    Bone& bone = bones[bone_it->second];
                    const std::size_t matrix_start = 1 + count * 2;
                    for (int index = 0; index < 16; ++index) bone.offset.value[index / 4][index % 4] = numbers[matrix_start + index];
                    for (int index = 0; index < count; ++index) {
                        const int vertex_index = static_cast<int>(numbers[1 + index]);
                        if (vertex_index < 0 || vertex_index >= static_cast<int>(vertices.size())) continue;
                        for (Influence& influence : vertices[vertex_index].influences) {
                            if (influence.bone < 0) {
                                influence = {bone_it->second, numbers[1 + count + index]};
                                break;
                            }
                        }
                    }
                }
            }
        }
        position = end;
    }
    return true;
}

template <typename Key, typename Value, typename Interpolator>
Value Sample(const std::vector<Key>& keys, float time, const Value& fallback, Interpolator interpolate) {
    if (keys.empty()) return fallback;
    if (keys.size() == 1 || time <= keys.front().time) return keys.front().value;
    for (std::size_t index = 1; index < keys.size(); ++index) {
        if (time <= keys[index].time) {
            const float span = keys[index].time - keys[index - 1].time;
            const float amount = span <= 0.0f ? 0.0f : (time - keys[index - 1].time) / span;
            return interpolate(keys[index - 1].value, keys[index].value, amount);
        }
    }
    return keys.back().value;
}

Vec3 LerpVec(const Vec3& left, const Vec3& right, float amount) {
    return {left.x + (right.x - left.x) * amount, left.y + (right.y - left.y) * amount,
            left.z + (right.z - left.z) * amount};
}

Quaternion LerpQuat(Quaternion left, Quaternion right, float amount) {
    const float dot = left.w * right.w + left.x * right.x + left.y * right.y + left.z * right.z;
    if (dot < 0.0f) { right.w = -right.w; right.x = -right.x; right.y = -right.y; right.z = -right.z; }
    return {left.w + (right.w - left.w) * amount, left.x + (right.x - left.x) * amount,
            left.y + (right.y - left.y) * amount, left.z + (right.z - left.z) * amount};
}

void ParseAnimationKey(const std::string& block, char kind, BoneTrack& track, float& duration) {
    const std::string marker = std::string("AnimationKey ") + kind;
    const std::size_t position = block.find(marker);
    std::string key_block;
    if (position == std::string::npos || !ExtractBlockAt(block, position, key_block)) return;
    const std::vector<float> numbers = ParseNumbers(key_block);
    if (numbers.size() < 2) return;
    const int count = static_cast<int>(numbers[1]);
    std::size_t cursor = 2;
    for (int index = 0; index < count && cursor + 1 < numbers.size(); ++index) {
        const float key_time = numbers[cursor++];
        const int value_count = static_cast<int>(numbers[cursor++]);
        if (cursor + value_count > numbers.size()) break;
        duration = std::max(duration, key_time);
        if (kind == 'R' && value_count >= 4) {
            track.rotation.push_back({key_time, {numbers[cursor], numbers[cursor + 1], numbers[cursor + 2], numbers[cursor + 3]}});
        } else if (value_count >= 3) {
            VecKey key{key_time, {numbers[cursor], numbers[cursor + 1], numbers[cursor + 2]}};
            (kind == 'S' ? track.scale : track.translation).push_back(key);
        }
        cursor += value_count;
    }
}

bool ParseAnimation(const std::filesystem::path& path, AnimationClip& clip) {
    std::string text;
    if (!ReadFile(path, text)) return false;
    const std::size_t ticks = text.find("AnimTicksPerSecond");
    if (ticks != std::string::npos) {
        std::string block;
        if (ExtractBlockAt(text, ticks, block)) {
            const std::vector<float> numbers = ParseNumbers(block);
            if (!numbers.empty() && numbers[0] > 0.0f) clip.ticks_per_second = numbers[0];
        }
    }
    std::size_t position = 0;
    while ((position = text.find("Animation {", position)) != std::string::npos) {
        std::string block;
        std::size_t end = position;
        if (!ExtractBlockAt(text, position, block, &end)) break;
        std::smatch target;
        if (std::regex_search(block, target, std::regex(R"(\{\s*([A-Za-z0-9_]+)\s*\})"))) {
            BoneTrack& track = clip.tracks[target[1].str()];
            ParseAnimationKey(block, 'R', track, clip.duration);
            ParseAnimationKey(block, 'S', track, clip.duration);
            ParseAnimationKey(block, 'T', track, clip.duration);
        }
        position = end;
    }
    return !clip.tracks.empty();
}

bool LoadTexture(const std::filesystem::path& path, GLuint& texture) {
    IWICImagingFactory* factory = nullptr; IWICBitmapDecoder* decoder = nullptr;
    IWICBitmapFrameDecode* frame = nullptr; IWICFormatConverter* converter = nullptr;
    const HRESULT initialization = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
    const bool uninitialize = SUCCEEDED(initialization);
    HRESULT result = CoCreateInstance(CLSID_WICImagingFactory, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&factory));
    if (SUCCEEDED(result)) result = factory->CreateDecoderFromFilename(path.c_str(), nullptr, GENERIC_READ, WICDecodeMetadataCacheOnLoad, &decoder);
    if (SUCCEEDED(result)) result = decoder->GetFrame(0, &frame);
    if (SUCCEEDED(result)) result = factory->CreateFormatConverter(&converter);
    if (SUCCEEDED(result)) result = converter->Initialize(frame, GUID_WICPixelFormat32bppRGBA, WICBitmapDitherTypeNone, nullptr, 0.0, WICBitmapPaletteTypeCustom);
    UINT width = 0, height = 0;
    if (SUCCEEDED(result)) result = converter->GetSize(&width, &height);
    std::vector<std::uint8_t> pixels;
    if (SUCCEEDED(result) && width > 0 && height > 0) {
        pixels.resize(static_cast<std::size_t>(width) * height * 4);
        result = converter->CopyPixels(nullptr, width * 4, static_cast<UINT>(pixels.size()), pixels.data());
    }
    if (SUCCEEDED(result)) {
        glGenTextures(1, &texture); glBindTexture(GL_TEXTURE_2D, texture);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, static_cast<GLsizei>(width), static_cast<GLsizei>(height), 0, GL_RGBA, GL_UNSIGNED_BYTE, pixels.data());
        glBindTexture(GL_TEXTURE_2D, 0);
    }
    if (converter) converter->Release(); if (frame) frame->Release();
    if (decoder) decoder->Release(); if (factory) factory->Release();
    if (uninitialize) CoUninitialize();
    return SUCCEEDED(result) && texture != 0;
}

ProjectedVertex Project(const Vec3& vertex, const ImVec2& center, float scale, float yaw, const ImVec2& uv = {}) {
    const float cosine = std::cos(yaw), sine = std::sin(yaw);
    const float rotated_x = vertex.x * cosine - vertex.z * sine;
    const float rotated_z = vertex.x * sine + vertex.z * cosine;
    const float perspective = 2.5f / (2.5f + rotated_z);
    return {ImVec2(center.x + rotated_x * scale * perspective,
                   center.y - (vertex.y - 0.49f) * scale * perspective), uv, rotated_z};
}

}  // namespace

struct GameZombieMesh::Impl {
    std::vector<Vertex> vertices;
    std::vector<Triangle> triangles;
    std::vector<Bone> bones;
    std::unordered_map<std::string, int> bone_indices;
    std::array<AnimationClip, 4> animations;
    GLuint texture = 0;
    int cached_frame = -1;
    int cached_animation = -1;
    std::vector<Matrix> cached_globals;
    std::vector<Vec3> cached_vertices;
};

GameZombieMesh& GameZombieMesh::Instance() { static GameZombieMesh mesh; return mesh; }
GameZombieMesh& GameZombieMesh::PlayerInstance() {
    static GameZombieMesh mesh(true);
    return mesh;
}
GameZombieMesh::~GameZombieMesh() {
    delete impl_;
}

bool GameZombieMesh::EnsureLoaded() {
    if (impl_) return true;
    if (attempted_) return false;
    attempted_ = true;
    Impl* loaded = new Impl();
    const std::filesystem::path root = GameDirectory();
    const std::filesystem::path texture = player_variant_
        ? root / L"media/textures/Body/MaleBody01.png"
        : root / L"media/textures/Body/M_ZedBody01.png";
    bool succeeded = ParseMesh(root / L"media/models_X/Skinned/MaleBody.x", loaded->vertices,
                               loaded->triangles, loaded->bones, loaded->bone_indices) &&
                     LoadTexture(texture, loaded->texture);
    for (std::size_t index = 0; succeeded && index < kAnimationFiles.size(); ++index) {
        const std::filesystem::path animation = player_variant_
            ? root / L"media/anims_X/Bob" / kPlayerAnimationFiles[index]
            : root / L"media/anims_X/Zombie" / kAnimationFiles[index];
        succeeded = ParseAnimation(animation, loaded->animations[index]);
    }
    if (!succeeded) {
        if (loaded->texture != 0) glDeleteTextures(1, &loaded->texture);
        delete loaded;
        return false;
    }
    impl_ = loaded;
    return true;
}

bool GameZombieMesh::Draw(ImDrawList* draw, const ImVec2& center, float scale, float yaw,
                          int animation, const ZombieMeshStyle& style, bool show_skeleton,
                          const ImVec4& skeleton_color, ImVec2& bounds_min,
                          ImVec2& bounds_max) const {
    if (!impl_ || impl_->texture == 0) return false;
    const int safe_animation = std::clamp(animation, 0, 3);
    const AnimationClip& clip = impl_->animations[safe_animation];
    const float seconds = static_cast<float>(ImGui::GetTime());
    if (impl_->cached_frame != ImGui::GetFrameCount() ||
        impl_->cached_animation != safe_animation) {
        const float tick = std::fmod(
            seconds * clip.ticks_per_second, std::max(clip.duration, 1.0f));
        impl_->cached_globals.assign(impl_->bones.size(), Identity());
        for (std::size_t index = 0; index < impl_->bones.size(); ++index) {
            const Bone& bone = impl_->bones[index];
            Matrix local = bone.bind_local;
            const auto found = clip.tracks.find(bone.name);
            if (found != clip.tracks.end()) {
                const BoneTrack& track = found->second;
                const Vec3 scale_value = Sample(
                    track.scale, tick, Vec3{1.0f, 1.0f, 1.0f}, LerpVec);
                const Quaternion rotation = Sample(track.rotation, tick, Quaternion{}, LerpQuat);
                const Vec3 translation = Sample(
                    track.translation, tick,
                    Vec3{bone.bind_local.value[3][0], bone.bind_local.value[3][1],
                         bone.bind_local.value[3][2]}, LerpVec);
                local = Compose(scale_value, rotation, translation);
            }
            impl_->cached_globals[index] = bone.parent >= 0
                ? Multiply(local, impl_->cached_globals[bone.parent]) : local;
        }
        impl_->cached_vertices.clear();
        impl_->cached_vertices.reserve(impl_->vertices.size());
        for (const Vertex& vertex : impl_->vertices) {
            Vec3 animated{};
            float total_weight = 0.0f;
            for (const Influence& influence : vertex.influences) {
                if (influence.bone < 0 || influence.weight <= 0.0f) continue;
                const Matrix skin = Multiply(
                    impl_->bones[influence.bone].offset,
                    impl_->cached_globals[influence.bone]);
                const Vec3 transformed = Transform(vertex.position, skin);
                animated.x += transformed.x * influence.weight;
                animated.y += transformed.y * influence.weight;
                animated.z += transformed.z * influence.weight;
                total_weight += influence.weight;
            }
            if (total_weight <= 0.0f) animated = vertex.position;
            impl_->cached_vertices.push_back(animated);
        }
        impl_->cached_frame = ImGui::GetFrameCount();
        impl_->cached_animation = safe_animation;
    }

    const std::vector<Matrix>& globals = impl_->cached_globals;
    std::vector<ProjectedVertex> projected_vertices;
    projected_vertices.reserve(impl_->vertices.size());
    bounds_min = ImVec2(FLT_MAX, FLT_MAX);
    bounds_max = ImVec2(-FLT_MAX, -FLT_MAX);
    for (std::size_t index = 0; index < impl_->vertices.size(); ++index) {
        const Vertex& vertex = impl_->vertices[index];
        const ProjectedVertex projected = Project(
            impl_->cached_vertices[index], center, scale, yaw, ImVec2(vertex.u, vertex.v));
        bounds_min.x = std::min(bounds_min.x, projected.position.x);
        bounds_min.y = std::min(bounds_min.y, projected.position.y);
        bounds_max.x = std::max(bounds_max.x, projected.position.x);
        bounds_max.y = std::max(bounds_max.y, projected.position.y);
        projected_vertices.push_back(projected);
    }

    std::vector<ProjectedTriangle> projected_triangles;
    projected_triangles.reserve(impl_->triangles.size());
    for (const Triangle& triangle : impl_->triangles) {
        ProjectedTriangle result{};
        for (int index = 0; index < 3; ++index) {
            result.vertices[index] = projected_vertices[triangle.indices[index]];
            result.depth += result.vertices[index].depth;
        }
        result.depth /= 3.0f;
        const ImVec2 edge_a(result.vertices[1].position.x - result.vertices[0].position.x,
                            result.vertices[1].position.y - result.vertices[0].position.y);
        const ImVec2 edge_b(result.vertices[2].position.x - result.vertices[0].position.x,
                            result.vertices[2].position.y - result.vertices[0].position.y);
        result.light = std::clamp(0.76f + (edge_a.x * edge_b.y - edge_a.y * edge_b.x) * 0.00004f, 0.60f, 1.0f);
        projected_triangles.push_back(result);
    }
    std::sort(projected_triangles.begin(), projected_triangles.end(),
              [](const ProjectedTriangle& left, const ProjectedTriangle& right) { return left.depth > right.depth; });
    using features::visual::ZombieModelEffect;
    const ZombieModelEffect effect = style.effect;
    const ImVec2 mesh_center((bounds_min.x + bounds_max.x) * 0.5f,
                             (bounds_min.y + bounds_max.y) * 0.5f);
    if (style.edge_glow) {
        ImVec4 outer = style.edge_color;
        outer.w *= 0.16f;
        DrawSolidPass(draw, projected_triangles, outer, mesh_center, 1.10f);
        ImVec4 inner = style.edge_color;
        inner.w *= 0.42f;
        DrawSolidPass(draw, projected_triangles, inner, mesh_center, 1.045f);
    }
    if (effect == ZombieModelEffect::Glow || effect == ZombieModelEffect::GlowOutline) {
        ImVec4 outer = style.color;
        outer.w *= 0.10f;
        DrawSolidPass(draw, projected_triangles, outer, mesh_center, 1.12f);
        outer.w = style.color.w * 0.22f;
        DrawSolidPass(draw, projected_triangles, outer, mesh_center, 1.055f);
    }

    const bool draw_textured =
        (effect == ZombieModelEffect::Disabled && style.draw_base_if_disabled) ||
        effect == ZombieModelEffect::Shaded || effect == ZombieModelEffect::GlowOutline ||
        effect == ZombieModelEffect::WaterFlow || effect == ZombieModelEffect::Glossy;
    const bool draw_solid = effect == ZombieModelEffect::Solid ||
        effect == ZombieModelEffect::Glow || effect == ZombieModelEffect::Iridescent;
    if (draw_textured) {
        draw->PushTexture(ImTextureRef(static_cast<ImTextureID>(impl_->texture)));
        for (const ProjectedTriangle& triangle : projected_triangles) {
            ImVec4 color{};
            if (effect == ZombieModelEffect::Disabled) {
                color = ImVec4(triangle.light, triangle.light, triangle.light, 1.0f);
            } else if (effect == ZombieModelEffect::WaterFlow) {
                const float y = (triangle.vertices[0].position.y + triangle.vertices[1].position.y +
                                 triangle.vertices[2].position.y) / 3.0f;
                const float wave = 0.5f + 0.5f * std::sin(seconds * 3.1f + y * 0.075f + triangle.depth * 2.0f);
                color = MultiplyLight(Mix(style.color, ImVec4(0.08f, 0.88f, 1.0f, style.color.w),
                                          0.22f + wave * 0.52f), 0.72f + triangle.light * 0.28f);
            } else if (effect == ZombieModelEffect::Glossy) {
                const float y = (triangle.vertices[0].position.y + triangle.vertices[1].position.y +
                                 triangle.vertices[2].position.y) / 3.0f;
                const float sweep = 0.5f + 0.5f * std::sin(seconds * 1.7f + y * 0.052f - triangle.depth * 2.4f);
                const float highlight = std::pow(sweep, 9.0f) * 0.78f;
                color = Mix(MultiplyLight(style.color, 0.68f + triangle.light * 0.32f),
                            ImVec4(1.0f, 1.0f, 1.0f, style.color.w), highlight);
            } else {
                color = MultiplyLight(style.color, 0.64f + triangle.light * 0.36f);
            }
            const ImU32 packed = ImGui::GetColorU32(color);
            draw->PrimReserve(3, 3);
            for (const ProjectedVertex& vertex : triangle.vertices) {
                draw->PrimVtx(vertex.position, vertex.uv, packed);
            }
        }
        draw->PopTexture();
    } else if (draw_solid) {
        for (const ProjectedTriangle& triangle : projected_triangles) {
            ImVec4 color = style.color;
            if (effect == ZombieModelEffect::Iridescent) {
                const float y = (triangle.vertices[0].position.y + triangle.vertices[1].position.y +
                                 triangle.vertices[2].position.y) / 3.0f;
                color = Hsv(seconds * 0.10f + y * 0.0045f + triangle.depth * 0.08f,
                            0.72f, 1.0f, style.color.w);
            }
            draw->AddTriangleFilled(triangle.vertices[0].position,
                                    triangle.vertices[1].position,
                                    triangle.vertices[2].position,
                                    ImGui::GetColorU32(color));
        }
    }

    if (show_skeleton) {
        const ImU32 shadow = ImGui::GetColorU32(ImVec4(0.02f, 0.03f, 0.05f, 0.90f));
        const ImU32 color = ImGui::GetColorU32(skeleton_color);
        const std::array<std::pair<const char*, const char*>, 16> links{{
            {"Bip01_Pelvis", "Bip01_Spine"}, {"Bip01_Spine", "Bip01_Spine1"},
            {"Bip01_Spine1", "Bip01_Neck"}, {"Bip01_Neck", "Bip01_Head"},
            {"Bip01_Neck", "Bip01_L_Clavicle"}, {"Bip01_L_Clavicle", "Bip01_L_UpperArm"},
            {"Bip01_L_UpperArm", "Bip01_L_Forearm"}, {"Bip01_L_Forearm", "Bip01_L_Hand"},
            {"Bip01_Neck", "Bip01_R_Clavicle"}, {"Bip01_R_Clavicle", "Bip01_R_UpperArm"},
            {"Bip01_R_UpperArm", "Bip01_R_Forearm"}, {"Bip01_R_Forearm", "Bip01_R_Hand"},
            {"Bip01_Pelvis", "Bip01_L_Thigh"}, {"Bip01_L_Thigh", "Bip01_L_Calf"},
            {"Bip01_Pelvis", "Bip01_R_Thigh"}, {"Bip01_R_Thigh", "Bip01_R_Calf"},
        }};
        for (const auto& link : links) {
            const int from_index = impl_->bone_indices.at(link.first);
            const int to_index = impl_->bone_indices.at(link.second);
            const Vec3 from_world{globals[from_index].value[3][0], globals[from_index].value[3][1], globals[from_index].value[3][2]};
            const Vec3 to_world{globals[to_index].value[3][0], globals[to_index].value[3][1], globals[to_index].value[3][2]};
            const ImVec2 from = Project(from_world, center, scale, yaw).position;
            const ImVec2 to = Project(to_world, center, scale, yaw).position;
            draw->AddLine(from, to, shadow, 4.0f);
            draw->AddLine(from, to, color, 1.6f);
            draw->AddCircleFilled(to, 1.8f, color);
        }
        for (const char* foot : {"Bip01_L_Foot", "Bip01_R_Foot"}) {
            const int foot_index = impl_->bone_indices.at(foot);
            const Vec3 foot_world{globals[foot_index].value[3][0], globals[foot_index].value[3][1], globals[foot_index].value[3][2]};
            draw->AddCircleFilled(Project(foot_world, center, scale, yaw).position, 1.8f, color);
        }
    }
    return true;
}

bool GameZombieMesh::ProjectBone(const char* bone_name, const ImVec2& center,
                                 float scale, float yaw,
                                 ImVec2& screen_position) const {
    if (!impl_ || bone_name == nullptr || impl_->cached_globals.empty()) return false;
    const auto found = impl_->bone_indices.find(bone_name);
    if (found == impl_->bone_indices.end()) return false;
    const Matrix& global = impl_->cached_globals[static_cast<std::size_t>(found->second)];
    const Vec3 world{global.value[3][0], global.value[3][1], global.value[3][2]};
    screen_position = Project(world, center, scale, yaw).position;
    return true;
}

}  // namespace pztrainer::ui
