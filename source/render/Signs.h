#pragma once
// TileEntitySignRenderer: the sign board (and post) with its four lines of text.
#include <string>
#include <vector>
#include "render/Vertex.h"
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
    struct Sign { int x, y, z, meta; bool post; std::string text[4]; };
    std::vector<Sign> signs;
    bool dirty = true;
    std::vector<QuadVertex> board, text;
};
