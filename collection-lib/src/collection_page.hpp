#pragma once

#include "collection_lib/collection_common.hpp"
#include "collection_lib/collection_page.hpp"

// Page engine. Pages (cl::Page, see collection_lib/collection_page.hpp) are
// consumer-created full-screen layers next to the main item grid. This header
// is the internal surface the rest of the menu code talks to - everything
// degrades to a no-op when the consumer has not created any page (the second
// page only exists if someone builds it via the API).

void collection_page_reset();                         // clear animation state (menu open/close)
void collection_page_teardown();                      // menu deleted / shutdown: drop pane refs
void collection_page_update();                        // advance the animation, once per frame
void collection_page_handle_input(dMenu_Collect2D_c*);// R / L page toggle (from wait_proc pre)
bool collection_page_active();                        // true while not fully on the main page
bool collection_page_p2_focused();                    // true if on a page and focusing an element
bool collection_page_on_page();                       // true while any page is the target (even unfocused)
f32  collection_page_grid_dx();                       // X offset to add to the grid's baseX
void collection_page_apply(dMenu_Collect2D_c*);       // position / show / hide page elements
bool collection_page_claims_cell(u8 x, u8 y);         // a page element owns this main-grid cell
void collection_page_sync_screen(J2DScreen* screen);  // (re)create page roots, resolve pane tags
