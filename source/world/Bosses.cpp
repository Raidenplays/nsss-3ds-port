#include "Bosses.h"
#include "Nbt.h"

namespace bosses {
namespace {
std::string dir;
nbt::TagPtr data;

nbt::Tag& get() {
    if (!data) data = nbt::makeCompound();
    return *data;
}
void save() {
    if (!dir.empty() && data) nbt::writeGzipFile(dir + "/axeolon_bosses.dat", *data);
}
} // namespace

void open(const std::string& worldDir) {
    dir = worldDir;
    data.reset();
    if (!dir.empty()) data = nbt::readGzipFile(dir + "/axeolon_bosses.dat");
    if (!data) data = nbt::makeCompound();
}

bool hasSpawned(const std::string& id) { return get().getBool((id + ".spawned").c_str()); }
bool isDefeated(const std::string& id) { return get().getBool((id + ".defeated").c_str()); }
void markSpawned(const std::string& id) { get().setBool((id + ".spawned").c_str(), true); save(); }
void markDefeated(const std::string& id) { get().setBool((id + ".defeated").c_str(), true); save(); }
void reset(const std::string& id) {
    get().setBool((id + ".spawned").c_str(), false);
    get().setBool((id + ".defeated").c_str(), false);
    save();
}
bool hasInt(const std::string& key) { return get().has(key.c_str()); }
int getInt(const std::string& key) { return get().getInt(key.c_str()); }
void setInt(const std::string& key, int v) { get().setInt(key.c_str(), v); save(); }

} // namespace bosses
