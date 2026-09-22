#pragma once

#include "collection_lib/collection_common.hpp"
#include "collection_lib/collection_page.hpp"

void collection_page_reset();
void collection_page_teardown();
void collection_page_update();
void collection_page_handle_input(dMenu_Collect2D_c*);
bool collection_page_active();
bool collection_page_p2_focused();
bool collection_page_on_page();
f32  collection_page_grid_dx();
void collection_page_apply(dMenu_Collect2D_c*);
bool collection_page_claims_cell(u8 x, u8 y);
void collection_page_sync_screen(J2DScreen* screen);
