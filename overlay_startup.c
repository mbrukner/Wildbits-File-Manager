/*
 * overlay_startup.c
 *
 *  Created on: Mar 11, 2024
 *      Author: micahbly
 *
 *  Routines for starting up Wildbits File Manager, including show splash screen(s)
 *    Some code here originated in sys.c and other places before being moved here
 *
 */



/*****************************************************************************/
/*                                Includes                                   */
/*****************************************************************************/

// project includes
#include "overlay_startup.h"
#include "app.h"
#include "comm_buffer.h"
#include "debug.h"
#include "file.h"
#include "general.h"
#include "kernel.h"
#include "keyboard.h"
#include "memory.h"
//#include "screen.h"
#include "sys.h"
#include "text.h"
#include "strings.h"

// C includes
#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

// WILDBITS includes
#include "wildbits.h"




/*****************************************************************************/
/*                               Definitions                                 */
/*****************************************************************************/

extern System*			global_system;

extern TextDialogTemplate	global_dlg;	// dialog we'll configure and re-use for different purposes
extern char					global_dlg_title[36];	// arbitrary
extern char					global_dlg_body_msg[70];	// arbitrary
extern char					global_dlg_button[3][10];	// arbitrary


extern char*				global_named_app_basic;
extern bool					global_started_from_flash;		// tracks whether app started from flash or from disk

extern char*				global_string[NUM_STRINGS];
extern char*				global_string_buff1;
extern char*				global_string_buff2;

extern uint8_t				zp_bank_num;
extern uint8_t				io_bank_value_kernel;	// stores value for the physical bank pointing to C000-DFFF whenever we change it, so we can restore it.

#pragma zpsym ("zp_bank_num");


/*****************************************************************************/
/*                       Private Function Prototypes                         */
/*****************************************************************************/

// display machine info: WILDBITS_JR or WILDBITS_K
void Startup_LoadString(void)
{
    uint8_t id, length, count;
    uint8_t old_bank, old_io;
    uint8_t* cursor = (uint8_t*)0xC000;
    asm("PHP");
    asm("SEI");
    old_io = R8(MMU_IO_CTRL);
    R8(MMU_IO_CTRL) = 4;
    zp_bank_num = STRING_STORAGE_EM_SLOT;
    old_bank = Memory_SwapInNewBank(BANK_IO);
    for (count = 0; count < NUM_STRINGS; ++count) {
        id = cursor[0];
        length = cursor[1];
        if (id >= NUM_STRINGS || cursor + length + 2 >= (uint8_t*)0xE000) break;
        cursor[0] = 0;
        cursor += 2;
        global_string[id] = (char*)cursor;
        cursor += length;
    }
    *cursor = 0;
    zp_bank_num = old_bank;
    Memory_SwapInNewBank(BANK_IO);
    R8(MMU_IO_CTRL) = old_io;
    asm("PLP");
}


// clear screen and show app (foenix) logo, and machine logo if running from flash
void Startup_ShowLogo(void)
{
    Text_ClearScreen(APP_FOREGROUND_COLOR, APP_BACKGROUND_COLOR);
    Text_DrawStringAtXY(29, 28, "Wildbits File Manager", APP_FOREGROUND_COLOR, APP_BACKGROUND_COLOR);
}


// enable the random number generator, and seed it
void Startup_InitializeRandomNumGen(void)
{
	// LOGIC:
	//   wildbits has a built in random number generator
	//   it works like this:
	//     1) you enable it with bit 0 of RND_CTRL
	//     2) you turn on seed mode by setting bit 1 of RND_CTRL to 1.
	//     3) you populate RNDL and RNDH with a seed
	//     4) you turn off see mode by unsetting bit 1 of RND_CTRL.
	//     5) you get random numbers by reading RNDL and RNDH. every time you read them, it repopulates them.
	//     6) resulting 16 bit number you divide by 65336 (RAND_MAX_FOENIX) to get a number 0-1.
	//   I will use the real time clock to seed the number generator

	uint8_t		old_rtc_control;

	// need to have vicky registers available
	Sys_SwapIOPage(VICKY_IO_PAGE_REGISTERS);

	// get mins and seconds from RTC
	old_rtc_control = R8(RTC_CONTROL);
	R8(RTC_CONTROL) = old_rtc_control | 0x08; // stop it from updating external registers

	// seed the RNG with time
	R8(RANDOM_NUM_GEN_ENABLE) = 3; // enable and set seed mode
	R8(RANDOM_NUM_GEN_LOW) = R8(RTC_SECONDS);
	R8(RANDOM_NUM_GEN_HI) = R8(RTC_MINUTES);
	R8(RANDOM_NUM_GEN_ENABLE) = 1; // keep enabled, return to generate mode

	// restore timer control to what it had been
	R8(RTC_CONTROL) = old_rtc_control;

	Sys_RestoreIOPage();
}


// initialize or re-initialize the global dialog box for standard 2-button entry
// some routine may change it temporarily to 3-button format/size
void App_InitializeDialogBox(void)
{
	// set up the dialog template we'll use throughout the app
	global_dlg.x_ = (SCREEN_NUM_COLS - APP_DIALOG_WIDTH)/2;
	global_dlg.y_ = 16;
	global_dlg.width_ = APP_DIALOG_WIDTH;
	global_dlg.height_ = APP_DIALOG_HEIGHT;
	global_dlg.num_buttons_ = APP_DIALOG_STARTING_NUM_BUTTONS;
	global_dlg.title_text_ = global_dlg_title;
	global_dlg.body_text_ = global_dlg_body_msg;
	global_dlg.btn_label_[0] = global_dlg_button[0];
	global_dlg.btn_label_[1] = global_dlg_button[1];
	global_dlg.btn_label_[2] = global_dlg_button[2];
	global_dlg.btn_keycolor_[0] = COLOR_GREEN;
	global_dlg.btn_keycolor_[1] = COLOR_RED;
	global_dlg.btn_keycolor_[2] = COLOR_BLUE;
	global_dlg.btn_shortcut_[0] = 'n';
	global_dlg.btn_shortcut_[1] = 'y';
	global_dlg.default_button_id_ = 1;
	global_dlg.default_button_shortcut_ = CH_ENTER;
	global_dlg.cancel_button_shortcut_ = CH_RUNSTOP;

}


// // ask user for their WiFi name and password.
// // for Meatloaf, this is not needed every time the computer turns on, it will retain a wifi connection
// // the main point of this function is to let the user set their Wifi SSID and password. Meatloaf will remember it.
// // this is not strictly a "startup" function, but startup has some space, and it's a one-time operation, so don't want it in MAIN
// bool App_ConnectToWifi(void)
// {
// 	char*		the_name;
// 	char*		the_pass;
// 	char*		this_response;
// 	char		password_buff[WIFI_MAX_PASSWORD_LEN];
// 	char*		password = password_buff;
// 	char		ssid_buff[WIFI_MAX_SSID_LEN];
// 	char*		ssid = ssid_buff;
//
// 	// do some kind of check to ensure we even have a Meatloaf connected?
// 	// NO, because UI_Menu_Enabler_Info has a .is_meatloaf_ property that enables/disables meatloaf commands already.
// // 	if (MEATLOAF-exists-and-is-working == false)
// // 	{
// // 		return false;
// // 	}
//
// 	General_Strlcpy(global_string_buff1, General_GetString(ID_STR_DLG_WIFI_SSID_TITLE), APP_DIALOG_WIDTH);
//
// 	the_name = Screen_GetStringFromUser(
// 		global_string_buff1,
// 		General_GetString(ID_STR_DLG_WIFI_SSID_BODY),
// 		ssid,
// 		WIFI_MAX_SSID_LEN);
//
// 	if (the_name == NULL)
// 	{
// 		return false;
// 	}

//
// 	General_Strlcpy(ssid, the_name, WIFI_MAX_PASSWORD_LEN);
// 	General_Strlcpy(global_string_buff1, General_GetString(ID_STR_DLG_WIFI_PASSWORD_TITLE), APP_DIALOG_WIDTH);
//
// 	the_pass = Screen_GetStringFromUser(
// 		global_string_buff1,
// 		General_GetString(ID_STR_DLG_WIFI_PASSWORD_BODY),
// 		password,
// 		WIFI_MAX_PASSWORD_LEN);
//
// 	if (the_pass == NULL)
// 	{
// 		return false;
// 	}
//
// 	General_Strlcpy(password, the_name, WIFI_MAX_PASSWORD_LEN);
//
// 	// send CBM DOS command to Meatloaf
// 	// OPEN1,30,15,"SETSSID:MYSSID,MYPASSWORD":CLOSE1
// 	snprintf(global_string_buff1, STORAGE_STRING_BUFFER_1_LEN, "SETSSID:%s,%s", ssid, password);
//
// 		// try to change directory
//
// 		cbm_k_setlfs(1, the_panel->device_number_, 15);	// 15 is command channel
// 		cbm_k_setnam("");				// no name supplied to OPEN when doing command channel
// 		cbm_k_open();					// set logical file 1 as the 'out' file
//
// 		if (cbm_k_readst() == 0)
// 		{
// 			// command channel successfully opened
// 			Buffer_NewMessage(global_temp_path_2);
// 			cbm_write(1, global_temp_path_2, strlen(global_temp_path_2));
// 			cbm_k_close(1);
//
// 			if (cbm_k_readst() == 0)
// 			{
// 				// change directory succeeded
// 				if ( (the_panel->root_folder_ = Folder_NewOrReset(the_panel->root_folder_, global_temp_path_1, the_panel->device_number_, the_panel->unit_number_, 0)) == NULL )
// 				{
// 					LOG_ERR(("%s %d: could not reset the panel's root folder", __func__ , __LINE__));
// 					#ifdef EXTRA_USER_FEEDBACK
// 						Buffer_NewMessage("could not reset the panel's root folder");
// 					#endif
// 					App_Exit(ERROR_DEFINE_ME);	// crash early, crash often
// 				}
//
// 				Panel_Refresh(the_panel);
// 				success = true;
// 			}
// 			else
// 			{
// 				// change dir attempt failed.
// 				success = false;
// 				#ifdef EXTRA_USER_FEEDBACK
// 					Buffer_NewMessage("failed to change directory");
// 				#endif
// 			}
//
// 		}
// 		else
// 		{
// 			// couldn't open command channel to even try to change dir
// 			cbm_k_close(1);
// 			success = false;
// 			#ifdef EXTRA_USER_FEEDBACK
// 				Buffer_NewMessage("failed to open command channel");
// 			#endif
// 		}
//
// }



