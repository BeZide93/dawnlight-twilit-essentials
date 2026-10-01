#pragma once

class J2DPicture;
class dKantera_icon_c;
class dMeter2Draw_c;

bool lazy_tweaks_build();
bool lazy_tweaks_hide_midna_icon();
J2DPicture* lazy_tweaks_item_num_tex(dMeter2Draw_c* draw, int button, int digit);
dKantera_icon_c* lazy_tweaks_z_kantera_meter(dMeter2Draw_c* draw);
