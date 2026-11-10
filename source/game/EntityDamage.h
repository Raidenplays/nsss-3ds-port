#pragma once
// Generated from tools/java/DumpDamage.java (tools/generated/damage.tsv): Item.getDamageVsEntity.
#include <cstdint>

namespace survival {
// InventoryPlayer.getDamageVsEntity: 1 for a fist or anything not listed
inline int entityDamage(int id) {
    switch (id) {
    case 256: return 3; // ItemSpade
    case 257: return 4; // ItemPickaxe
    case 258: return 5; // ItemAxe
    case 267: return 8; // ItemSword
    case 268: return 4; // ItemSword
    case 270: return 2; // ItemPickaxe
    case 271: return 3; // ItemAxe
    case 272: return 6; // ItemSword
    case 273: return 2; // ItemSpade
    case 274: return 3; // ItemPickaxe
    case 275: return 4; // ItemAxe
    case 276: return 10; // ItemSword
    case 277: return 4; // ItemSpade
    case 278: return 5; // ItemPickaxe
    case 279: return 6; // ItemAxe
    case 283: return 4; // ItemSword
    case 285: return 2; // ItemPickaxe
    case 286: return 3; // ItemAxe
    case 1015: return 2; // ItemAxeolonWeapon
    case 1016: return 4; // ItemAxeolonWeapon
    case 1017: return 6; // ItemAxeolonWeapon
    case 1018: return 8; // ItemAxeolonWeapon
    case 1019: return 2; // ItemAxeolonWeapon
    case 1020: return 3; // ItemAxeolonWeapon
    case 1021: return 5; // ItemAxeolonWeapon
    case 1022: return 7; // ItemAxeolonWeapon
    case 1023: return 9; // ItemAxeolonWeapon
    case 1024: return 3; // ItemAxeolonWeapon
    case 1025: return 5; // ItemAxeolonWeapon
    case 1026: return 7; // ItemAxeolonWeapon
    case 1027: return 9; // ItemAxeolonWeapon
    case 1028: return 11; // ItemAxeolonWeapon
    case 1029: return 5; // ItemAxeolonWeapon
    case 1030: return 4; // ItemAxeolonWeapon
    case 1031: return 6; // ItemAxeolonWeapon
    case 1032: return 8; // ItemAxeolonWeapon
    case 1033: return 10; // ItemAxeolonWeapon
    case 1034: return 4; // ItemAxeolonWeapon
    default: return 1;
    }
}
// ItemSword / ItemTool.hitEntity: how much a hit wears the item (0: it doesn't)
inline int hitWear(int id) {
    switch (id) {
    case 256: return 2;
    case 257: return 2;
    case 258: return 2;
    case 267: return 1;
    case 268: return 1;
    case 270: return 2;
    case 271: return 2;
    case 272: return 1;
    case 273: return 2;
    case 274: return 2;
    case 275: return 2;
    case 276: return 1;
    case 277: return 2;
    case 278: return 2;
    case 279: return 2;
    case 283: return 1;
    case 285: return 2;
    case 286: return 2;
    case 1015: return 1;
    case 1016: return 1;
    case 1017: return 1;
    case 1018: return 1;
    case 1019: return 1;
    case 1020: return 1;
    case 1021: return 1;
    case 1022: return 1;
    case 1023: return 1;
    case 1024: return 1;
    case 1025: return 1;
    case 1026: return 1;
    case 1027: return 1;
    case 1028: return 1;
    case 1029: return 1;
    case 1030: return 1;
    case 1031: return 1;
    case 1032: return 1;
    case 1033: return 1;
    case 1034: return 1;
    default: return 0;
    }
}
// AxeolonWeapons: spears reach further, maces knock harder
inline bool isMace(int id) { return id >= 1030 && id <= 1034; }
inline bool isSpear(int id) { return id >= 1020 && id <= 1024; }
} // namespace survival
