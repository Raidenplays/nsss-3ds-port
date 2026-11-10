#pragma once
// Boats (EntityBoat) and minecarts (EntityMinecart): placed from the item, ridden, broken back
// into items by hitting them, and saved in the chunk's entity list like Java's. A boat is steered
// by the rider's walking; a cart runs along the rails (plain, with a chest, or with a furnace).
#include <memory>
#include <string>
#include <vector>
#include "world/Nbt.h"
#include "world/Physics.h"
#include "Inventory.h"

class World;
class Renderer;
class Player;
class ItemEntities;
class Chunk;
class Mobs;

class Vehicles {
public:
    struct Boat {
        double x = 0, y = 0, z = 0, px = 0, py = 0, pz = 0, mx = 0, my = 0, mz = 0;
        float yaw = 0, prevYaw = 0;
        int damage = 0, hurtTime = 0, hurtDir = 1; // field_807_a, field_806_b, field_808_c
        int wood = 0;                             // AxeolonWood
        bool onGround = false, collidedH = false, dead = false;
        int id = 0;
        AABB box{};
        std::shared_ptr<nbt::Tag> tag; // what Java saved (anything we don't know goes back untouched)
    };
    std::vector<Boat> boats;
    struct Cart {
        double x = 0, y = 0, z = 0, px = 0, py = 0, pz = 0, mx = 0, my = 0, mz = 0;
        float yaw = 0, prevYaw = 0, pitch = 0;
        int type = 0;                             // 0 plain, 1 chest, 2 furnace
        int damage = 0, hurtTime = 0, hurtDir = 1;
        bool flipped = false;                     // field_856_i: it turned round
        bool onGround = false, collidedH = false, dead = false;
        double pushX = 0, pushZ = 0;
        int fuel = 0;
        std::vector<ItemStack> items;             // the chest cart's 27 slots
        bool mobAboard = false;                   // a mob rides it (riddenByEntity)
        int id = 0;
        AABB box{};
        std::shared_ptr<nbt::Tag> tag;
    };
    std::vector<Cart> carts;

    Vehicles();
    ~Vehicles();
    void chunkLoaded(Chunk& c);
    void writeToChunk(Chunk& c) const;
    void chunkUnloaded(int cx, int cz);
    void clear();

    // ItemBoat.onItemRightClick: a boat on the block looked at (within 5); false if none
    bool placeBoat(World& w, const Player& p, int wood);
    // the boat on the look line within reach (-1 if none); dist along the line
    int pick(double ox, double oy, double oz, double dx, double dy, double dz, double reach, double& dist) const;
    void hit(int index, int damage, ItemEntities& items);  // EntityBoat.attackEntityFrom
    void use(int index, Player& p);                         // EntityBoat.interact: get in (or out)

    // ItemMinecart.onItemUse: a cart of this type on the rail clicked
    bool placeCart(World& w, int x, int y, int z, int type);
    int pickCart(double ox, double oy, double oz, double dx, double dy, double dz, double reach, double& dist) const;
    void hitCart(int index, int damage, ItemEntities& items);
    // EntityMinecart.interact: ride (plain), open (chest: returns its id), or fuel and push (furnace)
    int useCart(int index, Player& p, Inventory& inv, bool creative);
    Cart* cartById(int id);
    // the chest cart open in the inventory screen
    void saveCartItems(int id, const std::vector<ItemStack>& items);

    // riding
    bool riding() const { return riddenId != 0 || riddenCart != 0; }
    void dismount(Player& p);                               // Entity.mountEntity(null)
    // EntityPlayerSP while riding: its own walk only pushes the boat (motion from the input)
    void setRiderInput(float strafe, float forward, float yaw, bool riderInWater);
    bool riderSeat(double& x, double& y, double& z) const; // updateRiderPosition: eye position

    void tick(World& w, Player& p, ItemEntities& items, Mobs* mobs = nullptr);
    void render(const World& w, Renderer& r, float partial);

private:
    int riddenId = 0, riddenCart = 0, nextId = 1;
    double riderMx = 0, riderMz = 0;
    std::vector<AABB> scratch;
    Boat* byId(int id);
    const Boat* byId(int id) const;
    void move(const World& w, Boat& b, double dx, double dy, double dz);
    static void setPosition(Boat& b, double x, double y, double z);
    void tickCart(World& w, Cart& c);
    void moveCart(const World& w, Cart& c, double dx, double dy, double dz);
    static void setPosition(Cart& c, double x, double y, double z);
    void cartCollisions(Player& p, Mobs* mobs);
    void renderCarts(const World& w, Renderer& r, float partial);
};
