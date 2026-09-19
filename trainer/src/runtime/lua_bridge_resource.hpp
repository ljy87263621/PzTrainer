#pragma once

#include <Windows.h>

#include <filesystem>
#include <string>

namespace pztrainer::runtime {

// Extracts the embedded Java bridge to a versioned per-user cache directory.
// The returned path is stable for the current embedded payload and is safe to
// pass to a URLClassLoader. An empty path means the resource could not be read.
void SetLuaBridgeResourceModule(HMODULE module);
std::filesystem::path EnsureLuaBridgeJar(std::string& error);

}  // namespace pztrainer::runtime
