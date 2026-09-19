# dusklight-collection-lib

A reusable [Dusklight](https://github.com/TwilitRealm/dusklight) library that extends the
Twilight Princess pause **Collection screen** with custom equippable slots. It is linked
into a mod statically at build time - there is no runtime dependency and no extra
`.dusk` file to install.

What it gives a mod:

- Custom **swords, shields and tunics** as new equippable slots on the Collection screen,
  including model swap on Link (in-game and on the menu doll), frame highlights,
  descriptions, cursor navigation and save persistence.
- Optional vanilla starter slots: **wooden sword, Ordon clothes and Ordon shield**,
  including the "shield never leaves the collection" behavior.
- An optional **Unequip** action for equip slots.
- Full-screen **pages** next to the item grid via a small `cl::Page` API (R/L to
  flip) - e.g. Heart Container + Mirror of Twilight on a second page, which
  frees up the right side of the item grid for custom slots.
- Widescreen layout handling and safe system-heap expansion for the menu's resources.

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
    collectionlib_register_slot({
        CE_SWORD, 5,
        "Gilded Sword",
        "A blade with a golden shine.",
        "textures/clctres/gilded.bti",
        "models/clctres/AlSwords.arc", 0x0007,
        0x0008,   // sheath model, 0xFFFF = none
    });
}

MOD_EXPORT ModResult mod_initialize(ModError*) {
    if (svc_hook == nullptr) {
        return MOD_ERROR;
    }

    // Optional feature policies (the library holds no settings of its own;
    // your mod owns its config and injects the decisions as predicates).

    // The library (re-)runs this callback on init and on every collection
    // screen build - registration is idempotent per (kind, item).
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

A complete, buildable mod lives in [examples/basic](examples/basic).

## Creating slots

Every slot is one `collectionlib_register_slot({...})` call with a `CustomEquipDef`.
The `kind` picks the row, `item` the 1-based column within that row:

| Field | Meaning |
| --- | --- |
| `kind` | `CE_SWORD`, `CE_SHIELD` or `CE_TUNIC` |
| `item` | 1-based column inside the row (continues after the vanilla slots) |
| `name`, `description` | Slot label and description text |
| `iconBti` | `.bti` icon texture, path relative to your mod's `res/` |
| `modelArc`, `modelFileId` | Model archive (`.arc`) and the file id of the BMD inside it |
| `sheathFileId` | Swords only: file id of the sheath model, `0xFFFF` for none |
| `offX/Y/Z`, `rotX/Y/Z`, `scale` | Model fit-up relative to the vanilla equip model |
| `baseClothes` | Tunics only: vanilla clothes model the custom body grafts onto |
| `padColor` | Tunics only: gamepad LED color override (`0xRRGGBB`), `0xFFFFFFFF` = vanilla |
| `unlocked` | Optional `bool(*)()` gate; `nullptr` = always available |

Slot registration is **idempotent** per `(kind, item)` - the library invokes your
registration callback again every time the collection screen is built, so the same
`collectionlib_register_slot` calls may run many times.

Icons and models are loaded through the Dusklight `ResourceService` and therefore always
resolve inside the **owning mod's** `res/` directory. Example layout:

```
my-mod/
  res/
    textures/clctres/my_sword.bti
    models/clctres/MySword.arc
  src/main.cpp
  mod.json
```

## Moving and adding slots

Slots live in rows (1 = swords, 2 = shields, 3 = tunics); positions are 1-based,
counted left-to-right. Moves are recorded once and re-applied on every collection
screen build:

```cpp
CollectionSlot master = collectionlib_get_slot(1, 2);   // second sword slot
collectionlib_move_slot(master, 3);                     // ...move it to column 3
collectionlib_add_sword_slot(1, woodenSwordDef);        // ...and add one at column 1
collectionlib_reset_layout();                            // ...or drop all moves again
```

### Vanilla first columns (starter gear)

Each row's first column hosts a vanilla cell (wooden sword, ordon shield, ordon
clothes). A mod claims it and owns its visibility entirely:

```cpp
collectionlib_add_vanilla_sword_slot({.unlocked = &my_unlocked_gate});
collectionlib_add_vanilla_shield_slot({.unlocked = &my_unlocked_gate});
collectionlib_add_vanilla_tunic_slot({.unlocked = &my_unlocked_gate,
    .name = "Ordon Clothes", .icon = get_ordon_clothes_texture()});
```

Claimed columns also drive the row layout: claimed first column = the row keeps
all columns; unclaimed = the row shifts one slot left.

### Auto-placement (multi-mod friendly)

Hardcoding a column means two mods can claim the same slot. To queue into the
next free column of a row instead, use the `add_next_*` variants - they scan
the row for the first column not claimed by another registered slot:

```cpp
collectionlib_add_next_sword_slot(def);   // first free sword column, -1 if full
```

Rows are 4 columns wide; when every column is taken the call returns `-1`.

`collectionlib_move_slot` visually translates the slot's panes; the recorded move is
re-applied on rebuilds, widescreen relayouts included. `collectionlib_add_*_slot`
register a custom equip slot at an explicit column.

## Pages (second screen)

Pages are opt-in: without one, the Collection screen behaves like vanilla. Create a
page **before** `collectionlib_init` and add elements to it:

```cpp
cl::Page* p2 = new cl::Page();
p2->add(cl::heart());          // Heart Container ('heart_n'), owns its grid cell (6,0)
p2->add(cl::fused_shadow());   // Mirror of Twilight ('kamen_n' + 'modelbgn' backdrop)
```

`add()` re-parents the element's pane into the page: the pane is removed from its
current parent and appended to the page's own container (this happens when the next
collection screen is built - before that, elements are just pane tags). From then on
the page owns the pane's position:

- **R** slides the page in from the right, **L** back to the item grid (the grid
  fades and slides away; the Link doll stays on all pages). Every transition is
  animated - page to page slides the outgoing page left while the incoming one
  enters from the right.
- Default layout: elements are spaced evenly around the page anchor
  (`set_anchor` / `set_spacing`), so the two lines above reproduce the classic
  second page - heart and mirror side by side, roughly screen-centred.
- The page has its own cursor (left/right between selectable elements, down drops
  into the item grid, walking up pops back onto the page).
- `cl::heart()` claims the heart's vanilla grid cell (6,0): the cell is not
  selectable on the main page while the page exists, and becomes a normal grid
  slot again when the page is removed.
- `cl::crystal()` is a placeholder: the vanilla layout has no crystal pane, so
  until its tag points at a pane that exists, the element stays invisible,
  occupies **no** layout slot (the others close the gap) and is skipped by the
  page cursor. Adjust the tag inside `cl::crystal()` once a crystal pane exists.

Custom elements: brace-initialize a `cl::Element` (pane tag + optional follower
pane, `hideOnMain`, `claimsCell`, explicit position) and `add()` it - see
`include/collection_lib/collection_page.hpp`.

## API overview

| Function | Purpose |
| --- | --- |
| `collectionlib_set_register_callback(void (*)())` | Provide the function that registers all of the mod's slots |
| `collectionlib_register_slot(const CustomEquipDef&)` | Add a custom slot; returns the slot id (call from the callback) |
| `collectionlib_add_sword_slot / add_shield_slot / add_tunic_slot` | Register a slot at an explicit column |
| `collectionlib_add_next_sword_slot / add_next_shield_slot / add_next_tunic_slot` | Register at the first free column of the row |
| `collectionlib_get_slot(row, item)` | Resolve a slot position to a handle |
| `collectionlib_move_slot(CollectionSlot, newItem)` | Move a slot to another column (recorded, replayed per build) |
| `collectionlib_reset_layout()` | Drop all recorded slot moves |
| `collectionlib_slot_count()` | Number of registered slots |
| `collectionlib_activate(int id)` | Equip the custom slot with this id |
| `collectionlib_clear(CustomEquipKind)` | Unequip back to vanilla gear for a kind |
| `collectionlib_active(CustomEquipKind)` | Is a custom item equipped for this kind? |
| `collectionlib_active_id(CustomEquipKind)` | Active slot id of a kind, or `-1` |
| `collectionlib_request_reload()` | Rebuild the Collection screen on next open (after changes) |
| `collectionlib_init / update / shutdown` | Lifecycle |

The full types (`CustomEquipDef`, `SlotSpec`, `SlotText`, `SlotAuto`, ...) are documented
inline in `include/collection_lib/collection_common.hpp` and
`include/collection_lib/custom_equip.hpp`. For hand-placed slots with custom positions and
navigation, build a `SlotSpec` with `autoLayout` enabled - see the header comments.

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
