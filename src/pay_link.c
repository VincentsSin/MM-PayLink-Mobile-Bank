#include "modding.h"
#include "recomputils.h"
#include "global.h"
#include "recompconfig.h"
#include "eztr_api.h"
#include "rando_exports.h"
#include "z64item.h"

#include "overlays/actors/ovl_En_Elf/z_en_elf.h"

// extern s16 sRupeeDigitsCount[];
// extern s16 sRupeeDigitsFirst[];

// RECOMP_HOOK("Interface_Draw")
// void onInterface_Draw(PlayState* play) {
//     Gfx_DrawTexRectIA8(OVERLAY_DISP, gRupeeCounterIconTex, 16, 16, 26, 206, 16, 16, 1 << 10, 1 << 10);
// }

// RECOMP_CALLBACK("*", recomp_after_play_init) void modify_upgrade_table(PlayState* this) {

//     gUpgradeCapacities[UPG_WALLET][0] = 200;
//     gUpgradeCapacities[UPG_WALLET][1] = 1000;
//     gUpgradeCapacities[UPG_WALLET][2] = 5000;
//     gUpgradeCapacities[UPG_WALLET][3] = 9999;
//     sRupeeDigitsFirst[0] = 0;
//     sRupeeDigitsCount[0] = 4;
// }


#define LOCATION_BANK_200_REWARD 0x000008
#define LOCATION_BANK_500_REWARD 0x080177
#define LOCATION_BANK_1000_REWARD 0x070177

void OfferBankReward(EnElf* this, PlayState* play);

EnElfActionFunc savedTatlActionFunc;

EZTR_DEFINE_CUSTOM_MSG_HANDLE(mobilebank);
EZTR_DEFINE_CUSTOM_MSG_HANDLE(mobileupdate);

typedef enum {
    MOBILEBANK_NONE,
    MOBILEBANK_INIT,
    MOBILEBANK_INPUT,
} MobilebankState;
static MobilebankState mobilebankState = MOBILEBANK_NONE;

typedef enum {
    MOBILEUPDATE_NONE,
    MOBILEUPDATE_INIT,
    MOBILEUPDATE_INPUT,
} MobileupdateState;
static MobileupdateState mobileupdateState = MOBILEUPDATE_NONE;

bool sIsRecompRandoCompatEnabled;
bool awardChecked;
static bool isMobilebank = false;
static bool lastFrameMobilebank = false;
static u16 mobilebankTimer = 0;
static bool isMobileupdate = false;
static bool lastFrameMobileupdate = false;
static u16 mobileupdateTimer = 0;
static bool randoEnabled;

s16 previousBankValue;

RECOMP_CALLBACK("*", recomp_on_init)
void init_bank() {
    randoEnabled = (recomp_is_dependency_met("mm_recomp_rando") == DEPENDENCY_STATUS_FOUND);
}

void Mobilebank_Start(PlayState* play) {
    Player* player = GET_PLAYER(play);
    
    if (
        isMobilebank ||
        Message_GetState(&play->msgCtx) != TEXT_STATE_NONE ||
        player->tatlTextId != 0 ||
        play->transitionTrigger != TRANS_TRIGGER_OFF
    ) return;

    isMobilebank = true;
    mobilebankTimer = (20 * 2); // Stays for 2 seconds
    lastFrameMobilebank = false;
}

void Mobileupdate_Start(PlayState* play) {
    Player* player = GET_PLAYER(play);
    
    if (
        isMobileupdate ||
        Message_GetState(&play->msgCtx) != TEXT_STATE_NONE ||
        player->tatlTextId != 0 ||
        play->transitionTrigger != TRANS_TRIGGER_OFF
    ) return;

    isMobileupdate = true;
    mobileupdateTimer = (20 * 2); // Stays for 2 seconds
    lastFrameMobileupdate = false;
}

RECOMP_HOOK("Play_Update")
void Mobilebank_Reward(PlayState* play) {
    if (!isMobilebank) {
        return;
    };

    Player* player = GET_PLAYER(play);

    if (mobilebankTimer > 0)
        mobilebankTimer--;

    if (mobilebankTimer == 0) {
        isMobilebank = false;
        return;
    }

    if (isMobilebank) {
        bool isInMobilebank = play->msgCtx.currentTextId == EZTR_GET_ID_H(mobilebank);

        if (lastFrameMobilebank && !isInMobilebank) {
            isMobilebank = false;
            return;    
        } else if (!lastFrameMobilebank && !isInMobilebank) {
            player->tatlTextId = EZTR_GET_ID_H(mobilebank);
        } else if (!lastFrameMobilebank && isInMobilebank) {
            EnElf* tatl = ((EnElf*)player->tatlActor);
            savedTatlActionFunc = tatl->actionFunc; // save actionFunc
            tatl->actionFunc = OfferBankReward;
        }
        lastFrameMobilebank = isInMobilebank;
    }
}

RECOMP_HOOK("Play_Update")
void Mobilebank_Update(PlayState* play) {
    if (!isMobileupdate) {
        return;
    };

    Player* player = GET_PLAYER(play);

    if (mobileupdateTimer > 0)
        mobileupdateTimer--;

    if (mobileupdateTimer == 0) {
        isMobileupdate = false;
        return;
    }

    if (isMobileupdate) {
        bool isInMobileupdate = play->msgCtx.currentTextId == EZTR_GET_ID_H(mobileupdate);

        if (lastFrameMobileupdate && !isInMobileupdate) {
            isMobileupdate = false;
            return;    
        } else if (!lastFrameMobileupdate && !isInMobileupdate) {
            player->tatlTextId = EZTR_GET_ID_H(mobileupdate);
        }
        lastFrameMobileupdate = isInMobileupdate;
    }
}

RECOMP_HOOK_RETURN("Rupees_ChangeBy")
void afterRupees_ChangeBy() {
    //recomp_printf("rupee accumulator: %d\n", gSaveContext.rupeeAccumulator);

    if ((gSaveContext.rupeeAccumulator > 0) && ((gSaveContext.save.saveInfo.playerData.rupees + gSaveContext.rupeeAccumulator) > CUR_CAPACITY(UPG_WALLET))) {
        //recomp_printf("overflow\n");

        u32 overflow = (gSaveContext.save.saveInfo.playerData.rupees + gSaveContext.rupeeAccumulator) - CUR_CAPACITY(UPG_WALLET);

        gSaveContext.rupeeAccumulator -= overflow;
        //recomp_printf("remainder %d\n", overflow);

        HS_SET_BANK_RUPEES(HS_GET_BANK_RUPEES() + overflow);
        // recomp_printf("Wallet full; sending to bank. Bank Total: %d\n", HIGH_SCORE(HS_BANK_RUPEES));
    }

    if ((HIGH_SCORE(HS_BANK_RUPEES) > 9999)) {
        HS_SET_BANK_RUPEES(9000);
    }
        
}


RECOMP_HOOK("Player_Update") 
void onPlayer_Update(Player* this, PlayState* play) {
    if (CHECK_BTN_ALL(play->state.input[0].cur.button, BTN_L | BTN_B) &&
        (gSaveContext.save.saveInfo.playerData.rupees != CUR_CAPACITY(UPG_WALLET) &&
        (HIGH_SCORE(HS_BANK_RUPEES) > 0))
    ) {
        gSaveContext.save.saveInfo.playerData.rupees++;

        HS_SET_BANK_RUPEES(HS_GET_BANK_RUPEES() - 1);
        // recomp_printf("Bank Total: %d\n", HIGH_SCORE(HS_BANK_RUPEES));
    }

    if (CHECK_BTN_ALL(play->state.input[0].cur.button, BTN_L | BTN_Z) &&
        (gSaveContext.save.saveInfo.playerData.rupees !=0)
    ) {
        gSaveContext.save.saveInfo.playerData.rupees--;
        HS_SET_BANK_RUPEES(HS_GET_BANK_RUPEES() + 1);
        // recomp_printf("Bank Total: %d\n", HIGH_SCORE(HS_BANK_RUPEES));

    }
    
    if (CHECK_BTN_ALL(play->state.input[0].cur.button, BTN_L | BTN_R)
    ) {
        Mobileupdate_Start(play);
    
    }
    
    // if (CHECK_BTN_ALL(play->state.input[0].cur.button, BTN_L | BTN_R | BTN_Z)) {
    //     Rupees_ChangeBy(50);
    //     recomp_printf("Bank Total: %d\n", HIGH_SCORE(HS_BANK_RUPEES));
    // }

    if (randoEnabled) {
        if ((HIGH_SCORE(HS_BANK_RUPEES) >= 200) &&
            (previousBankValue < 200) &&
            !rando_location_is_checked_external(LOCATION_BANK_200_REWARD)
        ) {
            SET_WEEKEVENTREG(WEEKEVENTREG_59_40);
            Mobilebank_Start(play);

        } else if ((HIGH_SCORE(HS_BANK_RUPEES) >= 500) &&
            (previousBankValue < 500) &&
            !rando_location_is_checked_external(LOCATION_BANK_500_REWARD)
        ) {
            SET_WEEKEVENTREG(WEEKEVENTREG_59_80);
            Mobilebank_Start(play);

        } else if ((HIGH_SCORE(HS_BANK_RUPEES) >= 1000) &&
            (previousBankValue < 1000) &&
            !rando_location_is_checked_external(LOCATION_BANK_1000_REWARD)
        ) {
            SET_WEEKEVENTREG(WEEKEVENTREG_60_01);
            Mobilebank_Start(play);
        }

    } else {
        if ((HIGH_SCORE(HS_BANK_RUPEES) >= 200) &&
            (previousBankValue < 200) &&
            !CHECK_WEEKEVENTREG(WEEKEVENTREG_RECEIVED_BANK_WALLET_UPGRADE)
        ) {
            SET_WEEKEVENTREG(WEEKEVENTREG_59_40);
            Mobilebank_Start(play);
            
        } else if ((HIGH_SCORE(HS_BANK_RUPEES) >= 5000) &&
            (previousBankValue < 5000) &&
            !CHECK_WEEKEVENTREG(WEEKEVENTREG_RECEIVED_BANK_HEART_PIECE)
        ) {
            SET_WEEKEVENTREG(WEEKEVENTREG_60_01);
            Mobilebank_Start(play);
        }

    }
}

void OfferBankReward(EnElf* this, PlayState* play) {
    if (randoEnabled) {
        if (Actor_HasParent(&this->actor, play)) {
        this->actor.parent = NULL;
        this->actionFunc = savedTatlActionFunc; // set actionFunc back to what it was before

        } else if (!rando_location_is_checked_external(LOCATION_BANK_200_REWARD) &&
            HIGH_SCORE(HS_BANK_RUPEES) >= 200) {
            Actor_OfferGetItemHookExternal(&this->actor, play, rando_get_item_id_external(LOCATION_BANK_200_REWARD), LOCATION_BANK_200_REWARD, 500.0f, 100.0f, true, true);
        } else if (!rando_location_is_checked_external(LOCATION_BANK_500_REWARD) &&
                HIGH_SCORE(HS_BANK_RUPEES) >= 500) {
            Actor_OfferGetItemHookExternal(&this->actor, play, rando_get_item_id_external(LOCATION_BANK_500_REWARD), LOCATION_BANK_500_REWARD, 500.0f, 100.0f, true, true);
        } else if (!rando_location_is_checked_external(LOCATION_BANK_1000_REWARD) &&
                HIGH_SCORE(HS_BANK_RUPEES) >= 1000){
            Actor_OfferGetItemHookExternal(&this->actor, play, rando_get_item_id_external(LOCATION_BANK_1000_REWARD), LOCATION_BANK_1000_REWARD, 500.0f, 100.0f, true, true);
        }
    } else {
        if (Actor_HasParent(&this->actor, play)) {
        this->actor.parent = NULL;
        this->actionFunc = savedTatlActionFunc; // set actionFunc back to what it was before

        } else if (!CHECK_WEEKEVENTREG(WEEKEVENTREG_RECEIVED_BANK_WALLET_UPGRADE) &&
            HIGH_SCORE(HS_BANK_RUPEES) >= 200) {
            Actor_OfferGetItem(&this->actor, play, GI_WALLET_ADULT + CUR_UPG_VALUE(UPG_WALLET), 500.0f, 100.0f);
            SET_WEEKEVENTREG(WEEKEVENTREG_RECEIVED_BANK_WALLET_UPGRADE);
        } else if (!CHECK_WEEKEVENTREG(WEEKEVENTREG_RECEIVED_BANK_HEART_PIECE) &&
            HIGH_SCORE(HS_BANK_RUPEES) >= 5000){
            Actor_OfferGetItem(&this->actor, play, GI_HEART_PIECE, 500.0f, 100.0f);
            SET_WEEKEVENTREG(WEEKEVENTREG_RECEIVED_BANK_HEART_PIECE);
        }
    }
}

EZTR_MSG_CALLBACK(mobilebank_callback) {
    if (randoEnabled) {
        if ((HS_GET_BANK_RUPEES() >= 200) &&
            (previousBankValue < 200)
        ) {
            EZTR_MsgSContent_Sprintf(buf->data.content, "" EZTR_CC_COLOR_BLUE "Wow! " EZTR_CC_COLOR_DEFAULT "You earned a reward" EZTR_CC_NEWLINE 
            "for depositing " EZTR_CC_COLOR_PINK "200 rupees!" EZTR_CC_EVENT "" EZTR_CC_END);
            
        } if ((HS_GET_BANK_RUPEES() >= 500) &&
            (previousBankValue < 500)
        ) {
            EZTR_MsgSContent_Sprintf(buf->data.content, "" EZTR_CC_COLOR_ORANGE "Amazing! " EZTR_CC_COLOR_DEFAULT "You earned a reward" EZTR_CC_NEWLINE 
            "for depositing " EZTR_CC_COLOR_PINK "500 rupees!" EZTR_CC_EVENT "" EZTR_CC_END);

        } if (HS_GET_BANK_RUPEES() >= 1000 &&
            (previousBankValue < 1000)
        ) {
            EZTR_MsgSContent_Sprintf(buf->data.content, "" EZTR_CC_COLOR_RED "Sp" EZTR_CC_COLOR_ORANGE "ec" EZTR_CC_COLOR_YELLOW "ta" EZTR_CC_COLOR_GREEN "cu" EZTR_CC_COLOR_LIGHTBLUE "la" EZTR_CC_COLOR_BLUE "r!" EZTR_CC_COLOR_DEFAULT " You earned a reward" EZTR_CC_NEWLINE 
            "for depositing " EZTR_CC_COLOR_PINK "1000 rupees!" EZTR_CC_EVENT "" EZTR_CC_END);
        }

    } else {
        if ((HS_GET_BANK_RUPEES() >= 200) &&
            (previousBankValue < 200)
        ) {
            EZTR_MsgSContent_Sprintf(buf->data.content, "" EZTR_CC_COLOR_BLUE "Wow! " EZTR_CC_COLOR_DEFAULT "You earned a reward" EZTR_CC_NEWLINE 
            "for depositing " EZTR_CC_COLOR_PINK "200 rupees!" EZTR_CC_EVENT "" EZTR_CC_END);
            
        } if (HS_GET_BANK_RUPEES() >= 5000 &&
            (previousBankValue < 5000)
        ) {
            EZTR_MsgSContent_Sprintf(buf->data.content, "" EZTR_CC_COLOR_RED "Sp" EZTR_CC_COLOR_ORANGE "ec" EZTR_CC_COLOR_YELLOW "ta" EZTR_CC_COLOR_GREEN "cu" EZTR_CC_COLOR_LIGHTBLUE "la" EZTR_CC_COLOR_BLUE "r!" EZTR_CC_COLOR_DEFAULT " You dedserve this" EZTR_CC_NEWLINE 
            "for depositing " EZTR_CC_COLOR_PINK "5000 rupees!" EZTR_CC_EVENT "" EZTR_CC_END);
        }
    }
}

EZTR_MSG_CALLBACK(mobileupdate_callback) {
    if (HIGH_SCORE(HS_BANK_RUPEES) >= 0) {
    play->msgCtx.rupeesTotal = HS_GET_BANK_RUPEES();
    EZTR_MsgSContent_Sprintf(buf->data.content, "You have a total balance of:" EZTR_CC_NEWLINE "" EZTR_CC_COLOR_RED "" EZTR_CC_RUPEES_TOTAL "" EZTR_CC_EVENT "" EZTR_CC_END);
    }
}

EZTR_ON_INIT void init_Mobilebank() {
    EZTR_Basic_AddCustomText(
        EZTR_HNAME(mobilebank),
        EZTR_STANDARD_TEXT_BOX_I,
        1,
        EZTR_ICON_NO_ICON,
        EZTR_NO_VALUE,
        EZTR_NO_VALUE,
        EZTR_NO_VALUE,
        false,
        EZTR_CC_END,
        mobilebank_callback
    );
    EZTR_Basic_AddCustomText(
        EZTR_HNAME(mobileupdate),
        EZTR_STANDARD_TEXT_BOX_I,
        1,
        EZTR_ICON_NO_ICON,
        EZTR_NO_VALUE,
        EZTR_NO_VALUE,
        EZTR_NO_VALUE,
        false,
        EZTR_CC_END,
        mobileupdate_callback
    );
}
