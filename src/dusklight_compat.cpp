#include "d/d_com_inf_game.h"
#include "d/d_meter2_info.h"
#include "d/actor/d_a_player.h"
#include "dusk/config_var.hpp"

namespace dusk::config {
bool ConfigVarBase::has_subscribers() const { return false; }
void ConfigVarBase::notify_changed(const void*) {}
}

CPaneMgr* dMeter2Info_getMeterItemPanePtr(s32 i_idx) {
    return g_meter2_info.getMeterItemPanePtr(i_idx);
}

int dMeter2Info_readItemTexture(u8 i_itemNo, void* i_texBuf1, J2DPicture* i_pic1,
                               void* i_texBuf2, J2DPicture* i_pic2, void* i_texBuf3, J2DPicture* i_pic3,
                               void* i_texBuf4, J2DPicture* i_pic4, int param_9) {
    return g_meter2_info.readItemTexture(i_itemNo, i_texBuf1, i_pic1, i_texBuf2, i_pic2,
                                         i_texBuf3, i_pic3, i_texBuf4, i_pic4, param_9);
}

void dMeter2Info_setItemColor(u8 i_itemNo, J2DPicture* i_pic1, J2DPicture* i_pic2,
                              J2DPicture* i_pic3, J2DPicture* i_pic4) {
    g_meter2_info.setItemColor(i_itemNo, i_pic1, i_pic2, i_pic3, i_pic4);
}

bool dMeter2Info_isUseButton(int i_buttonBit) {
    return g_meter2_info.isUseButton(i_buttonBit);
}

void dMeter2Info_onUseButton(int i_buttonBit) {
    g_meter2_info.onUseButton(i_buttonBit);
}

dMw_c* dMeter2Info_getMenuWindowClass() {
    return g_meter2_info.getMenuWindowClass();
}

dMeter2_c* dMeter2Info_getMeterClass() {
    return g_meter2_info.getMeterClass();
}

bool dMeter2Info_isShopTalkFlag() {
    return g_meter2_info.isShopTalkFlag();
}

u8 dMeter2Info_getWindowStatus() {
    return g_meter2_info.getWindowStatus();
}

u8 dMeter2Info_getPauseStatus() {
    return g_meter2_info.getPauseStatus();
}

u16 dComIfGs_getMaxLife() {
    return g_dComIfG_gameInfo.info.getPlayer().getPlayerStatusA().getMaxLife();
}

void dComIfGs_setMaxLife(u8 i_maxLife) {
    g_dComIfG_gameInfo.info.getPlayer().getPlayerStatusA().setMaxLife(i_maxLife);
}

u16 dComIfGs_getLife() {
    return g_dComIfG_gameInfo.info.getPlayer().getPlayerStatusA().getLife();
}

void dComIfGs_setLife(u16 i_life) {
    g_dComIfG_gameInfo.info.getPlayer().getPlayerStatusA().setLife(i_life);
}

u16 dComIfGs_getRupee() {
    return g_dComIfG_gameInfo.info.getPlayer().getPlayerStatusA().getRupee();
}

void dComIfGs_setRupee(u16 i_rupees) {
    g_dComIfG_gameInfo.info.getPlayer().getPlayerStatusA().setRupee(i_rupees);
}

u16 dComIfGs_getRupeeMax() {
    return g_dComIfG_gameInfo.info.getPlayer().getPlayerStatusA().getRupeeMax();
}

u16 dComIfGs_getMaxOil() {
    return g_dComIfG_gameInfo.info.getPlayer().getPlayerStatusA().getMaxOil();
}

void dComIfGs_setMaxOil(u16 i_maxOil) {
    g_dComIfG_gameInfo.info.getPlayer().getPlayerStatusA().setMaxOil(i_maxOil);
}

u16 dComIfGs_getOil() {
    return g_dComIfG_gameInfo.info.getPlayer().getPlayerStatusA().getOil();
}

void dComIfGs_setOil(u16 i_oil) {
    g_dComIfG_gameInfo.info.getPlayer().getPlayerStatusA().setOil(i_oil);
}

u8 dComIfGs_getSelectEquipClothes() {
    return g_dComIfG_gameInfo.info.getPlayer().getPlayerStatusA().getSelectEquip(COLLECT_CLOTHING);
}

u8 dComIfGs_getSelectEquipSword() {
    return g_dComIfG_gameInfo.info.getPlayer().getPlayerStatusA().getSelectEquip(COLLECT_SWORD);
}

u8 dComIfGs_getSelectEquipShield() {
    return g_dComIfG_gameInfo.info.getPlayer().getPlayerStatusA().getSelectEquip(COLLECT_SHIELD);
}

u8 dComIfGs_getWalletSize() {
    return g_dComIfG_gameInfo.info.getPlayer().getPlayerStatusA().getWalletSize();
}

u8 dComIfGs_getMaxMagic() {
    return g_dComIfG_gameInfo.info.getPlayer().getPlayerStatusA().getMaxMagic();
}

u8 dComIfGs_getMagic() {
    return g_dComIfG_gameInfo.info.getPlayer().getPlayerStatusA().getMagic();
}

u8 dComIfGs_getTransformStatus() {
    return g_dComIfG_gameInfo.info.getPlayer().getPlayerStatusA().getTransformStatus();
}

void dComIfGs_setTransformStatus(u8 i_status) {
    g_dComIfG_gameInfo.info.getPlayer().getPlayerStatusA().setTransformStatus(i_status);
}

u8 dComIfGs_getSelectItemIndex(int i_no) {
    return g_dComIfG_gameInfo.info.getPlayer().getPlayerStatusA().getSelectItemIndex(i_no);
}

u8 dComIfGs_getItem(int i_slotNo, bool i_checkCombo) {
    return g_dComIfG_gameInfo.info.getPlayer().getItem().getItem(i_slotNo, i_checkCombo);
}

void dComIfGs_setItem(int i_slotNo, u8 i_itemNo) {
    g_dComIfG_gameInfo.info.getPlayer().getItem().setItem(i_slotNo, i_itemNo);
}

int dComIfGs_isItemFirstBit(u8 i_no) {
    return g_dComIfG_gameInfo.info.getPlayer().getGetItem().isFirstBit(i_no);
}

void dComIfGs_onItemFirstBit(u8 i_itemNo) {
    g_dComIfG_gameInfo.info.getPlayer().getGetItem().onFirstBit(i_itemNo);
}

u8 dComIfGs_getArrowNum() {
    return g_dComIfG_gameInfo.info.getPlayer().getItemRecord().getArrowNum();
}

void dComIfGs_setArrowNum(u8 i_arrowNum) {
    g_dComIfG_gameInfo.info.getPlayer().getItemRecord().setArrowNum(i_arrowNum);
}

u8 dComIfGs_getArrowMax() {
    return g_dComIfG_gameInfo.info.getPlayer().getItemMax().getArrowNum();
}

void dComIfGs_setArrowMax(u8 i_arrowMax) {
    g_dComIfG_gameInfo.info.getPlayer().getItemMax().setArrowNum(i_arrowMax);
}

u8 dComIfGs_getPachinkoNum() {
    return g_dComIfG_gameInfo.info.getPlayer().getItemRecord().getPachinkoNum();
}

void dComIfGs_setPachinkoNum(u8 i_num) {
    g_dComIfG_gameInfo.info.getPlayer().getItemRecord().setPachinkoNum(i_num);
}

u8 dComIfGs_getPachinkoMax() {
    return 50;
}

u8 dComIfGs_getBombNum(u8 i_bagIdx) {
    return g_dComIfG_gameInfo.info.getPlayer().getItemRecord().getBombNum(i_bagIdx);
}

void dComIfGs_setBombNum(u8 i_bagIdx, u8 i_bombNum) {
    g_dComIfG_gameInfo.info.getPlayer().getItemRecord().setBombNum(i_bagIdx, i_bombNum);
}

u8 dComIfGs_getBombMax(u8 i_bombType) {
    return g_dComIfG_gameInfo.info.getPlayer().getItemMax().getBombNum(i_bombType);
}

void dComIfGs_setBombMax(u8 i_type, u8 i_max) {
    g_dComIfG_gameInfo.info.getPlayer().getItemMax().setBombNum(i_type, i_max);
}

u8 dComIfGs_getBottleNum(u8 i_bottleIdx) {
    return g_dComIfG_gameInfo.info.getPlayer().getItemRecord().getBottleNum(i_bottleIdx);
}

void dComIfGs_setBottleNum(u8 i_bottleIdx, u8 i_bottleNum) {
    g_dComIfG_gameInfo.info.getPlayer().getItemRecord().setBottleNum(i_bottleIdx, i_bottleNum);
}

int dComIfGs_isCollectShield(u8 i_shield) {
    return g_dComIfG_gameInfo.info.getPlayer().getCollect().isCollect(0, i_shield);
}

void dComIfGs_setCollectShield(u8 i_shieldNo) {
    g_dComIfG_gameInfo.info.getPlayer().getCollect().setCollect(0, i_shieldNo);
}

BOOL dComIfGs_isCollectClothes(u8 i_clothesNo) {
    return g_dComIfG_gameInfo.info.getPlayer().getCollect().isCollect(1, i_clothesNo);
}

void dComIfGs_setCollectClothes(u8 i_clothesNo) {
    g_dComIfG_gameInfo.info.getPlayer().getCollect().setCollect(1, i_clothesNo);
}

BOOL dComIfGs_isCollectSword(u8 i_swordNo) {
    return g_dComIfG_gameInfo.info.getPlayer().getCollect().isCollect(2, i_swordNo);
}

void dComIfGs_setCollectSword(u8 i_swordNo) {
    g_dComIfG_gameInfo.info.getPlayer().getCollect().setCollect(2, i_swordNo);
}

int dComIfGs_isEventBit(u16 i_flag) {
    return g_dComIfG_gameInfo.info.getSavedata().getEvent().isEventBit(i_flag);
}

void dComIfGs_onEventBit(const u16 i_flag) {
    g_dComIfG_gameInfo.info.getSavedata().getEvent().onEventBit(i_flag);
}

void dComIfGs_offEventBit(const u16 i_flag) {
    g_dComIfG_gameInfo.info.getSavedata().getEvent().offEventBit(i_flag);
}

s32 dComIfGs_isStageBossEnemy() {
    return g_dComIfG_gameInfo.info.getMemory().getBit().isStageBossEnemy();
}

s32 dComIfGs_isStageMiddleBoss() {
    return g_dComIfG_gameInfo.info.getMemory().getBit().isStageBossEnemy2();
}

void dComIfGs_setRestartRoom(const cXyz& i_position, s16 i_angle, s8 i_roomNo) {
    g_dComIfG_gameInfo.info.getRestart().setRoom(i_position, i_angle, i_roomNo);
}

void dComIfGs_setRestartRoomParam(u32 i_param) {
    g_dComIfG_gameInfo.info.getRestart().setRoomParam(i_param);
}

JKRArchive* dComIfGp_getRingResArchive() {
    return g_dComIfG_gameInfo.play.getRingResArchive();
}

JKRArchive* dComIfGp_getMain2DArchive() {
    return g_dComIfG_gameInfo.play.getMain2DArchive();
}

JKRExpHeap* dComIfGp_getExpHeap2D() {
    return g_dComIfG_gameInfo.play.getExpHeap2D();
}

J2DGrafContext* dComIfGp_getCurrentGrafPort() {
    return g_dComIfG_gameInfo.play.getCurrentGrafPort();
}

u32 dComIfGp_checkPlayerStatus1(int i_idx, u32 i_mask) {
    return g_dComIfG_gameInfo.play.checkPlayerStatus(i_idx, 1, i_mask);
}

int dComIfGp_event_runCheck() {
    return g_dComIfG_gameInfo.play.getEvent()->runCheck();
}

dEvt_control_c* dComIfGp_getEvent() {
    return g_dComIfG_gameInfo.play.getEvent();
}

u8 dComIfGp_isPauseFlag() {
    return g_dComIfG_gameInfo.play.isPauseFlag();
}

dAttention_c* dComIfGp_getAttention() {
    return g_dComIfG_gameInfo.play.getAttention();
}

const char* dComIfGp_getStartStageName() {
    return g_dComIfG_gameInfo.play.getStartStage()->getName();
}

s16 dComIfGp_getStartStagePoint() {
    return g_dComIfG_gameInfo.play.getStartStage()->getPoint();
}

daPy_py_c* dComIfGp_getLinkPlayer() {
    return (daPy_py_c*)g_dComIfG_gameInfo.play.getPlayerPtr(LINK_PTR);
}

fopAc_ac_c* dComIfGp_getPlayer(int i_idx) {
    return g_dComIfG_gameInfo.play.getPlayer(i_idx);
}

daHorse_c* dComIfGp_getHorseActor() {
    return g_dComIfG_gameInfo.play.getHorseActor();
}

int dComIfG_setObjectRes(const char* i_arcName, u8 i_mountDirection, JKRHeap* i_heap) {
    return g_dComIfG_gameInfo.mResControl.setObjectRes(i_arcName, i_mountDirection, i_heap);
}

int dComIfG_syncObjectRes(const char* i_arcName) {
    return g_dComIfG_gameInfo.mResControl.syncObjectRes(i_arcName);
}

int dComIfG_deleteObjectResMain(const char* i_arcName) {
    return g_dComIfG_gameInfo.mResControl.deleteObjectRes(i_arcName);
}

dRes_info_c* dComIfG_getObjectResInfo(const char* i_arcName) {
    return g_dComIfG_gameInfo.mResControl.getObjectResInfo(i_arcName);
}

void* dComIfG_getObjectRes(const char* i_arcName, int i_index) {
    return g_dComIfG_gameInfo.mResControl.getObjectRes(i_arcName, i_index);
}

void* dComIfG_getObjectRes(const char* i_arcName, const char* i_resName) {
    return g_dComIfG_gameInfo.mResControl.getObjectRes(i_arcName, i_resName);
}

void* dComIfG_getObjectIDRes(const char* i_arcName, u16 i_resID) {
    return g_dComIfG_gameInfo.mResControl.getObjectIDRes(i_arcName, i_resID);
}

bool dComIfGd_addRealShadow(u32 key, J3DModel* model) {
    return g_dComIfG_gameInfo.drawlist.addRealShadow(key, model);
}

const char* dComIfGp_getNextStageName() {
    return g_dComIfG_gameInfo.play.getNextStageName();
}

BOOL dComIfGp_isEnableNextStage() {
    return g_dComIfG_gameInfo.play.isEnableNextStage();
}

int dComIfGp_roomControl_getStayNo() {
    return g_dComIfG_gameInfo.play.getRoomControl()->getStayNo();
}

camera_process_class* dComIfGp_getCamera(int i_cameraID) {
    return (camera_process_class*)g_dComIfG_gameInfo.play.getCamera(i_cameraID);
}

dMsgObject_c* dComIfGp_getMsgObjectClass() {
    return g_dComIfG_gameInfo.play.getMsgObjectClass();
}

u8 dComIfGp_getDoStatus() {
    return g_dComIfG_gameInfo.play.getDoStatus();
}

void dComIfGp_setSelectEquipClothes(u8 i_cloth) {
    g_dComIfG_gameInfo.play.setSelectEquip(COLLECT_CLOTHING, i_cloth);
}

void dComIfGp_setSelectEquipSword(u8 i_sword) {
    g_dComIfG_gameInfo.play.setSelectEquip(COLLECT_SWORD, i_sword);
}

void dComIfGp_setSelectEquipShield(u8 i_shield) {
    g_dComIfG_gameInfo.play.setSelectEquip(COLLECT_SHIELD, i_shield);
}

void dComIfGp_setItem(u8 i_slotNo, u8 i_itemNo) {
    g_dComIfG_gameInfo.play.setItem(i_slotNo, i_itemNo);
}

void dComIfGs_onTransformLV(int i_no) {
    g_dComIfG_gameInfo.info.getPlayer().getPlayerStatusB().onTransformLV(i_no);
}

void dComIfGp_event_reset() {
    g_dComIfG_gameInfo.play.getEvent()->reset();
}

fopAc_ac_c* dComIfGp_att_getZHint() {
    return g_dComIfG_gameInfo.play.getAttention()->getZHintTarget();
}

void dComIfGp_setZStatus(u8 status, u8 flag) {
    g_dComIfG_gameInfo.play.setZStatus(status, flag);
}

void dComIfGp_setMesgCameraInfoActor(fopAc_ac_c* param_1, fopAc_ac_c* param_2,
                                    fopAc_ac_c* param_3, fopAc_ac_c* param_4,
                                    fopAc_ac_c* param_5, fopAc_ac_c* param_6,
                                    fopAc_ac_c* param_7, fopAc_ac_c* param_8,
                                    fopAc_ac_c* param_9, fopAc_ac_c* param_10)
{
    g_dComIfG_gameInfo.play.setMesgCamInfoActor(param_1, param_2, param_3, param_4, param_5,
                                                param_6, param_7, param_8, param_9, param_10);
}

u32 dComIfGp_checkPlayerStatus0(int param_0, u32 flag) {
    return g_dComIfG_gameInfo.play.checkPlayerStatus(param_0, 0, flag);
}
