#pragma once
// TileEntitySignRenderer: the sign board (and post), each Axeolon wood with its own texture and
// dark woods written in white, with its four lines of text in Beta's font.
#include <string>
#include <vector>
#include "model/AxmModel.h"
#include "ui/Font.h"

class World;
class Renderer;

class Signs {
public:
    // Picks the signs out of the loaded chunks' tile entities (every second, and after an edit)
    void refresh(World& w, double px, double pz, int radius);
    void markDirty() { dirty = true; }
    bool needsRefresh() const { return dirty; }
    void render(const World& w, Renderer& r, const Font& font, double camX, double camY, double camZ);

private:
    struct Sign { int x, y, z, meta, wood; bool post; std::string text[4]; };
    std::vector<Sign> signs;
    bool dirty = true;
    std::vector<axm::ModelVertex> board, text;
};
