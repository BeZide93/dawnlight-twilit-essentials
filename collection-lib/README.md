# dusklight-collection-lib

A reusable [Dusklight](https://github.com/TwilitRealm/dusklight) library that lets a mod
change the equipment rows of the Twilight Princess pause **Collection screen**: add slots,
move slots, replace slots. It is linked into a mod statically at build time - there is no
runtime dependency and no extra `.dusk` file to install.

The library does not redraw the screen. Without any change the rows stay exactly native,
and native items keep the game's own code (visibility, names, equipping, equipped frame)
wherever they end up. Only what a mod adds or replaces is handled by the library, with the
native rules: empty frames stay visible, the cursor walks the columns like the native one,
A equips, the frame of the worn item lights up, mouse pointer support comes with it.

What it gives a mod:

- Custom **swords, shields and clothes** as equippable slots, including model swap on
  Link, the B button icon, names, descriptions and save persistence.
- Slots **without a model**: a custom icon/name for a vanilla item (e.g. Ordon Clothes).
- Full-screen **pages** next to the item grid via a small `cl::Page` API (R/L to flip) -
  e.g. Pieces of Heart and Fused Shadow on a second page, which frees the sword row's
  columns 3 and 4.
- Optional, non-native behavior behind predicates: **Unequip** with A, keeping the Ordon
  Shield for item checks.

> One library instance owns the Collection screen. Do **not** install two mods that both
> embed this library: each copy would add its own slots and hooks independently, and the
> grids would conflict. Per game installation, only one mod may link it.

## Requirements

- A Dusklight mod project set up against the Dusklight Mod SDK
  (`add_subdirectory(<dusk>/sdk dusk-sdk)`), like the
  [Twilit Essentials](https://github.com/F1mmel/twilit-essentials) mod.
- CMake 3.25 or newer.

## Integration

### 1. Fetch the library

Add this to your mod's `CMakeLists.txt` after the Dusklight SDK subdirectory:

```cmake
include(FetchContent)
FetchContent_Declare(dusklight-collection-lib
        GIT_REPOSITORY https://github.com/F1mmel/dusklight-collection-lib.git
        GIT_TAG        main)   # pin a commit SHA for reproducible builds
FetchContent_MakeAvailable(dusklight-collection-lib)
```

### 2. Link it into your mod

```cmake
add_mod(my_mod
        FEATURES game
        SOURCES
            src/main.cpp
        MOD_JSON mod.json
        RES_DIR res
)

target_link_libraries(my_mod PRIVATE collection_lib)
```

The library imports the services it needs itself (resource, host); the mod passes the
hook, log and save services to `collectionlib_init`.

### 3. Use it in code

```cpp
#include <collection_lib/collection_lib.hpp>

#include "global.h"
#include "mods/service.hpp"

IMPORT_SERVICE(LogService, svc_log);
IMPORT_SERVICE(HookService, svc_hook);
IMPORT_OPTIONAL_SERVICE(SaveService, svc_save);

// Defines the mod_ctx global and the mod metadata records - exactly once per mod.
DEFINE_MOD();

static void register_slots() {
    collectionlib_add_next_sword_slot({
        CE_SWORD, 0,
        "Gilded Sword",
        "A blade with a golden shine.",
        "textures/clctres/gilded.bti", nullptr,
        "models/clctres/AlSwords.arc", 0x0007,
        0x0008,   // sheath model, 0xFFFF = none
        0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f,
        dItemNo_MASTER_SWORD_e,   // plays like the Master Sword
    });
}

MOD_EXPORT ModResult mod_initialize(ModError*) {
    if (svc_hook == nullptr) {
        return MOD_ERROR;
    }
    // The library runs this on init and before every build of the Collection screen,
    // always starting from the native layout.
    collectionlib_set_register_callback(&register_slots);
    return collectionlib_init(svc_hook, svc_log, svc_save, mod_ctx);
}

MOD_EXPORT ModResult mod_update(ModError*) {
    collectionlib_update();
    return MOD_OK;
}

MOD_EXPORT ModResult mod_shutdown(ModError*) {
    collectionlib_shutdown();
    return MOD_OK;
}
```

## The layout

Rows and columns are 1-based. Columns count like the native screen:

| Row | Native columns |
| --- | --- |
| 1 (swords) | 1 = Ordon Sword (Wooden Sword before it), 2 = Master Sword |
| 2 (shields) | 1 = Wooden Shield (Ordon Shield before it), 2 = Hylian Shield |
| 3 (clothes) | 1 = Hero's Clothes, 2 = Zora Armor, 3 = Magic Armor |

A row holds up to 6 columns. The sword row gets 4 while the Pieces of Heart and the Fused
Shadow use its spare grid cells; put those on a page (see below) for all 6. Rows longer than
the native ones push the heart and the Fused Shadow to the right.

```cpp
get_slot(1, 1).replace(kokiriSword);     // custom item instead of the Ordon Sword
get_slot(1, 3).replace(goronSword);      // fills the empty column 3
collectionlib_add_next_sword_slot(fairy);  // first empty column (4)
get_slot(2, 1).insert(ordonShield);      // column 1, the native shields move to 2 and 3
get_slot(3, 3).move(3, 1);               // Magic Armor to column 1, Hero's Clothes to 3
get_slot(2, 2).remove();                 // no Hylian Shield in the grid
collectionlib_clear_all_slots();         // empty all rows, native items included
```

All layout calls belong into the register callback. Registration is idempotent per
(row, column): a slot registered again keeps its loaded model, icon and equipped state;
slots the callback stops registering are removed. After changing what the callback does
at runtime (e.g. a config toggle), call `collectionlib_request_reload()`.

## Custom slots

A slot is a `CustomEquipDef`:

| Field | Meaning |
| --- | --- |
| `kind`, `item` | Row and column; set by the layout function you call |
| `name`, `description` | Slot label and description text; `name = nullptr` shows the game's own (translated) name and description of `baseItem` |
| `iconBti` | `.bti` icon in your mod's `res/` - or the archive holding `iconArcFileId`; `nullptr` shows the game's own icon of `baseItem` (Wooden / Ordon Sword, Ordon / Wooden Shield) |
| `iconArcFileId` | File id of the icon inside that archive (`nullptr` = `iconBti` is a .bti). An archive that is not in `res/` is looked up in the game's `Layout/clctres.arc`, overlay patches included |
| `modelArc`, `modelFileId` | Model archive and the file id of the BMD inside it; `nullptr` = no model swap |
| `sheathFileId` | Swords only: file id of the sheath model, `0xFFFF` for none |
| `offX/Y/Z`, `rotX/Y/Z`, `scale` | Model fit-up relative to the vanilla equip model |
| `baseItem` | The vanilla item the slot stands in for, equipped underneath so it plays like it (swords/shields: stats and abilities, clothes: also the body the model grafts on) |
| `padColor` | Clothes only: gamepad LED color override (`0xRRGGBB`), `0xFFFFFFFF` = vanilla |
| `unlocked` | Optional `bool(*)()` gate; `nullptr` = always available |
| `ironBootsHideFeet` | Clothes only: the Iron Boots hide the model's own boots (default, like vanilla); `false` keeps the feet visible |

A slot without a model is simply its `baseItem` with the slot's icon and name - equipping
it equips the vanilla item, and it shows as worn while that item is. Such a slot gives the
item a column of its own, the way the starter gear gets one:

```cpp
CustomEquipDef woodenSword{};
woodenSword.baseItem = dItemNo_WOOD_STICK_e;   // name, icon and model: none of its own
get_slot(1, 1).insert(woodenSword);            // Wooden Sword, Ordon Sword, Master Sword
```

- A Wooden Sword or Ordon Shield slot takes that item out of the native cell that shows it
  before the Ordon Sword / Wooden Shield is owned.
- With a slot based on the Ordon Clothes, the clothes row stays while they are worn (the game
  empties it, having no cell to switch back from them), and Link keeps them on reload.

Icons are copied once and stay valid for the whole session, so the B button can keep a
custom sword icon outside the menu. Icons from `.bti` files can be overridden by the user
with PNGs in `<Dusklight data folder>/texture_replacements/` (named after the `.bti`).

## Pages (second screen)

Pages are opt-in: without one, the Collection screen behaves like vanilla. Create a page
**before** `collectionlib_init` and add elements to it:

```cpp
cl::Page* p2 = new cl::Page();
p2->add(cl::heart());          // Pieces of Heart ('heart_n'), native cell 5/0
p2->add(cl::fused_shadow());   // Fused Shadow / Mirror ('kamen_n' + 'modelbgn'), native cell 6/0
```

`add()` re-parents the element's pane into the page when the screen is built; from then
on the page owns its position:

- **R** slides the page in from the right, **L** back to the item grid (the equipment
  rows fade and slide away; the Link doll stays on all pages).
- Elements are spaced evenly around the page anchor (`set_anchor` / `set_spacing`).
- The page has its own cursor (left/right between elements, down or past the first
  element drops into the item rows, walking up from the item rows goes back onto the
  page). Selecting an element that claims a native cell shows that cell's native name
  and description.
- An element with `claimsCell` takes its native cell out of the main grid, so the sword
  row can use it: with `cl::heart()` and `cl::fused_shadow()` it holds 6 columns instead of 4.
- `cl::crystal()` is a placeholder: the vanilla layout has no crystal pane, so the element
  stays invisible and is skipped until its tag points at a pane that exists.

Custom elements: brace-initialize a `cl::Element` (pane tag, optional follower pane,
`hideOnMain`, `claimsCell`, explicit position) and `add()` it - see
`include/collection_lib/collection_page.hpp`.

## API overview

| Function | Purpose |
| --- | --- |
| `collectionlib_set_register_callback(void (*)())` | The function that describes the layout |
| `get_slot(row, item).replace(def)` / `collectionlib_add_*_slot(item, def)` | Custom item in a column (replaces what is there) |
| `get_slot(row, item).insert(def)` | Custom item in a column, what is there moves one column right |
| `collectionlib_add_next_sword/shield/tunic_slot(def)` | Custom item in the first empty column, `-1` if the row is full |
| `collectionlib_register_slot(def)` | Row from `def.kind`, column `def.item` (0 = next free) |
| `get_slot(row, item).move(row, newItem)` / `collectionlib_move_slot` | Swap two columns of a row |
| `get_slot(row, item).remove()` / `collectionlib_remove_slot` | Empty a column |
| `collectionlib_clear_all_slots()` / `collectionlib_reset_layout()` | Empty all rows / back to native |
| `collectionlib_slot_count()` | Number of custom slots |
| `collectionlib_activate(id)` / `collectionlib_clear(kind)` | Equip a custom slot / back to vanilla gear |
| `collectionlib_active(kind)` / `collectionlib_active_id(kind)` | Is / which custom slot is worn |
| `collectionlib_request_reload()` | Rebuild the Collection screen if it is open |
| `custom_equip_set_suppressed(bool)` / `custom_equip_restore_from_save()` | Take custom items off (e.g. for a challenge mode) / put the saved ones back on |
| `collectionlib_set_unequip_policy(fn)` | A on the worn sword/shield unequips it (not native) |
| `collectionlib_set_keep_ordon_shield_policy(fn)` | Item checks keep counting the Ordon Shield (not native) |
| `collectionlib_init / update / shutdown` | Lifecycle |

## Building the example

```bash
cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=Release -S examples/basic
cmake --build build --parallel
```

The built `collection_lib_example.dusk` lands in `examples/basic/build/mods/`. Always
build with `Release` on Windows: a Debug build links the debug MSVC runtime and will not
load on a normal machine.

## CI

`.github/workflows/build.yml` builds the example consumer on Linux, Windows and macOS
against a pinned Dusklight commit, which keeps the library compiling as the engine moves.
