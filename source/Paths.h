#pragma once
// Where the read-only game files live: the 3DS reads them from its RomFS ("romfs:/"); Android
// copies the ones the shared code opens with fopen into its files folder and points this there.
#include <string>

inline std::string& romfsRoot() {
    static std::string root = "romfs:/";
    return root;
}
