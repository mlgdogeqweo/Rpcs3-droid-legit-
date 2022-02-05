#pragma once

#include "Emu/Memory/vm_ptr.h"

using CellSysutilUserId = u32;

enum
{
	CELL_SYSUTIL_ERROR_BASE_INTERNAL              = 0x8002b000,
	CELL_SYSUTIL_ERROR_BASE_COMMON                = 0x8002b100,
	CELL_SYSUTIL_ERROR_BASE_SYSTEMPARAM           = 0x8002b200,
	CELL_SYSUTIL_ERROR_BASE_MSGDIALOG             = 0x8002b300,
	CELL_SYSUTIL_ERROR_BASE_SAVEDATA              = 0x8002b400,
	CELL_SYSUTIL_ERROR_BASE_OSKDIALOG             = 0x8002b500,
	CELL_SYSUTIL_ERROR_BASE_GAMEDATA              = 0x8002b600,
	CELL_SYSUTIL_ERROR_BASE_AVC                   = 0x8002b700,
	CELL_SYSUTIL_ERROR_BASE_NETCTL                = 0x8002b800,
	CELL_SYSUTIL_ERROR_BASE_WEBBROWSER            = 0x8002b900,
	CELL_SYSUTIL_ERROR_BASE_HDDGAME               = 0x8002ba00,
	CELL_SYSUTIL_ERROR_BASE_SYSCONF               = 0x8002bb00,
	CELL_SYSUTIL_ERROR_BASE_SYSCACHE              = 0x8002bc00,
	CELL_SYSUTIL_ERROR_BASE_DISCGAME              = 0x8002bd00,
	CELL_SYSUTIL_ERROR_BASE_STORAGEDATA           = 0x8002be00,
	CELL_SYSUTIL_ERROR_BASE_IMEJP                 = 0x8002bf00,
	CELL_SYSUTIL_ERROR_BASE_FILE_SELECT           = 0x8002c000,
	CELL_SYSUTIL_ERROR_BASE_MUSIC                 = 0x8002c100,
	CELL_SYSUTIL_ERROR_BASE_PHOTO_EXPORT_UTIL     = 0x8002c200,
	CELL_SYSUTIL_ERROR_BASE_USERINFO              = 0x8002c300,
	CELL_SYSUTIL_ERROR_BASE_PRINT                 = 0x8002c400,
	CELL_SYSUTIL_ERROR_BASE_REC                   = 0x8002c500,
	CELL_SYSUTIL_ERROR_BASE_MUSIC_EXPORT_UTIL     = 0x8002c600,
	CELL_SYSUTIL_ERROR_BASE_PHOTO_IMPORT          = 0x8002c700,
	CELL_SYSUTIL_ERROR_BASE_SEARCH                = 0x8002c800,
	CELL_SYSUTIL_ERROR_BASE_PHOTO_DECODE          = 0x8002c900,
	CELL_SYSUTIL_ERROR_BASE_VIDEO_EXPORT_UTIL     = 0x8002ca00,
	CELL_SYSUTIL_ERROR_BASE_GAME                  = 0x8002cb00,
	CELL_SYSUTIL_ERROR_BASE_GAMEUPDATE            = 0x8002cc00,
	CELL_SYSUTIL_ERROR_BASE_AP                    = 0x8002cd00,
	CELL_SYSUTIL_ERROR_BASE_COMBOPLAY             = 0x8002cd80,
	CELL_SYSUTIL_ERROR_BASE_BGDL                  = 0x8002ce00,
	CELL_SYSUTIL_ERROR_BASE_VIDEO_UPLOAD_UTIL     = 0x8002d000,
	CELL_SYSUTIL_ERROR_BASE_SCREENSHOT            = 0x8002d100,
	CELL_SYSUTIL_ERROR_BASE_AUTHDIALOG            = 0x8002d200,
	CELL_SYSUTIL_ERROR_BASE_BGMPLAYBACK_EX        = 0x8002d300,
	CELL_SYSUTIL_ERROR_BASE_SYSTEM_CHAT           = 0x8002d400,
	CELL_SYSUTIL_ERROR_BASE_PHOTO_NETWORK_SHARING = 0x8002d500,
};

enum CellSysutilError : u32
{
	CELL_SYSUTIL_ERROR_TYPE       = 0x8002b101,
	CELL_SYSUTIL_ERROR_VALUE      = 0x8002b102,
	CELL_SYSUTIL_ERROR_SIZE       = 0x8002b103,
	CELL_SYSUTIL_ERROR_NUM        = 0x8002b104,
	CELL_SYSUTIL_ERROR_BUSY       = 0x8002b105,
	CELL_SYSUTIL_ERROR_STATUS     = 0x8002b106,
	CELL_SYSUTIL_ERROR_MEMORY     = 0x8002b107,
	CELL_SYSUTIL_ERROR_3D_SUPPORT = 0x8002b108,
};

// Parameter IDs
enum CellSysutilParamId: s32
{
	// Integers
	CELL_SYSUTIL_SYSTEMPARAM_ID_LANG                            = 0x0111,
	CELL_SYSUTIL_SYSTEMPARAM_ID_ENTER_BUTTON_ASSIGN             = 0x0112,
	CELL_SYSUTIL_SYSTEMPARAM_ID_DATE_FORMAT                     = 0x0114,
	CELL_SYSUTIL_SYSTEMPARAM_ID_TIME_FORMAT                     = 0x0115,
	CELL_SYSUTIL_SYSTEMPARAM_ID_TIMEZONE                        = 0x0116,
	CELL_SYSUTIL_SYSTEMPARAM_ID_SUMMERTIME                      = 0x0117,
	CELL_SYSUTIL_SYSTEMPARAM_ID_GAME_PARENTAL_LEVEL             = 0x0121,
	CELL_SYSUTIL_SYSTEMPARAM_ID_LICENSE_AREA                    = 0x0122,
	CELL_SYSUTIL_SYSTEMPARAM_ID_GAME_PARENTAL_LEVEL0_RESTRICT   = 0x0123,
	CELL_SYSUTIL_SYSTEMPARAM_ID_CURRENT_USER_HAS_NP_ACCOUNT     = 0x0141,
	CELL_SYSUTIL_SYSTEMPARAM_ID_CAMERA_PLFREQ                   = 0x0151,
	CELL_SYSUTIL_SYSTEMPARAM_ID_PAD_RUMBLE                      = 0x0152,
	CELL_SYSUTIL_SYSTEMPARAM_ID_KEYBOARD_TYPE                   = 0x0153,
	CELL_SYSUTIL_SYSTEMPARAM_ID_JAPANESE_KEYBOARD_ENTRY_METHOD  = 0x0154,
	CELL_SYSUTIL_SYSTEMPARAM_ID_CHINESE_KEYBOARD_ENTRY_METHOD   = 0x0155,
	CELL_SYSUTIL_SYSTEMPARAM_ID_PAD_AUTOOFF                     = 0x0156,
	CELL_SYSUTIL_SYSTEMPARAM_ID_MAGNETOMETER                    = 0x0157,

	// Strings
	CELL_SYSUTIL_SYSTEMPARAM_ID_NICKNAME                        = 0x0113,
	CELL_SYSUTIL_SYSTEMPARAM_ID_CURRENT_USERNAME                = 0x0131,
	// Unknown strings
	CELL_SYSUTIL_SYSTEMPARAM_ID_x1008                           = 0x1008,
	CELL_SYSUTIL_SYSTEMPARAM_ID_x1011                           = 0x1011,
	CELL_SYSUTIL_SYSTEMPARAM_ID_x1012                           = 0x1012, // Equal meaning to x1011
	CELL_SYSUTIL_SYSTEMPARAM_ID_x1024                           = 0x1024,
};

enum CellSysutilLang : s32
{
	CELL_SYSUTIL_LANG_JAPANESE       = 0,
	CELL_SYSUTIL_LANG_ENGLISH_US     = 1,
	CELL_SYSUTIL_LANG_FRENCH         = 2,
	CELL_SYSUTIL_LANG_SPANISH        = 3,
	CELL_SYSUTIL_LANG_GERMAN         = 4,
	CELL_SYSUTIL_LANG_ITALIAN        = 5,
	CELL_SYSUTIL_LANG_DUTCH          = 6,
	CELL_SYSUTIL_LANG_PORTUGUESE_PT  = 7,
	CELL_SYSUTIL_LANG_RUSSIAN        = 8,
	CELL_SYSUTIL_LANG_KOREAN         = 9,
	CELL_SYSUTIL_LANG_CHINESE_T      = 10,
	CELL_SYSUTIL_LANG_CHINESE_S      = 11,
	CELL_SYSUTIL_LANG_FINNISH        = 12,
	CELL_SYSUTIL_LANG_SWEDISH        = 13,
	CELL_SYSUTIL_LANG_DANISH         = 14,
	CELL_SYSUTIL_LANG_NORWEGIAN      = 15,
	CELL_SYSUTIL_LANG_POLISH         = 16,
	CELL_SYSUTIL_LANG_PORTUGUESE_BR  = 17, // FW 4.00
	CELL_SYSUTIL_LANG_ENGLISH_GB     = 18, // FW 4.00
	CELL_SYSUTIL_LANG_TURKISH        = 19, // FW 4.30
};

enum
{
	CELL_SYSUTIL_SYSTEMPARAM_NICKNAME_SIZE = 0x80,
	CELL_SYSUTIL_SYSTEMPARAM_CURRENT_USERNAME_SIZE = 0x40
};

enum
{
	CELL_SYSUTIL_KEYBOARD_ENTRY_METHOD_ROMAJI_INPUT  = 0,
	CELL_SYSUTIL_KEYBOARD_ENTRY_METHOD_KANA_INPUT    = 1,
};

enum
{
	CELL_SYSUTIL_KEYBOARD_ENTRY_METHOD_ZHUYIN_INPUT  = 0,
	CELL_SYSUTIL_KEYBOARD_ENTRY_METHOD_PINYIN_INPUT  = 1,
	CELL_SYSUTIL_KEYBOARD_ENTRY_METHOD_CANGJIE_INPUT = 2,
};

enum
{
	CELL_SYSUTIL_REQUEST_EXITGAME  = 0x0101,
	CELL_SYSUTIL_DRAWING_BEGIN     = 0x0121,
	CELL_SYSUTIL_DRAWING_END       = 0x0122,
	CELL_SYSUTIL_SYSTEM_MENU_OPEN  = 0x0131,
	CELL_SYSUTIL_SYSTEM_MENU_CLOSE = 0x0132,
	CELL_SYSUTIL_BGMPLAYBACK_PLAY  = 0x0141,
	CELL_SYSUTIL_BGMPLAYBACK_STOP  = 0x0142,

	CELL_SYSUTIL_NP_INVITATION_SELECTED   = 0x0151,
	CELL_SYSUTIL_NP_DATA_MESSAGE_SELECTED = 0x0152,

	CELL_SYSUTIL_SYSCHAT_START                   = 0x0161,
	CELL_SYSUTIL_SYSCHAT_STOP                    = 0x0162,
	CELL_SYSUTIL_SYSCHAT_VOICE_STREAMING_RESUMED = 0x0163,
	CELL_SYSUTIL_SYSCHAT_VOICE_STREAMING_PAUSED  = 0x0164,
};

using CellSysutilCallback = void(u64 status, u64 param, vm::ptr<void> userdata);

enum
{
	CELL_SYSUTIL_ENTER_BUTTON_ASSIGN_CIRCLE = 0,
	CELL_SYSUTIL_ENTER_BUTTON_ASSIGN_CROSS  = 1,
};

enum
{
	CELL_SYSUTIL_DATE_FMT_YYYYMMDD = 0,
	CELL_SYSUTIL_DATE_FMT_DDMMYYYY = 1,
	CELL_SYSUTIL_DATE_FMT_MMDDYYYY = 2,
};

enum
{
	CELL_SYSUTIL_TIME_FMT_CLOCK12 = 0,
	CELL_SYSUTIL_TIME_FMT_CLOCK24 = 1,
};

enum
{
	CELL_SYSUTIL_GAME_PARENTAL_OFF      = 0,
	CELL_SYSUTIL_GAME_PARENTAL_LEVEL01  = 1,
	CELL_SYSUTIL_GAME_PARENTAL_LEVEL02  = 2,
	CELL_SYSUTIL_GAME_PARENTAL_LEVEL03  = 3,
	CELL_SYSUTIL_GAME_PARENTAL_LEVEL04  = 4,
	CELL_SYSUTIL_GAME_PARENTAL_LEVEL05  = 5,
	CELL_SYSUTIL_GAME_PARENTAL_LEVEL06  = 6,
	CELL_SYSUTIL_GAME_PARENTAL_LEVEL07  = 7,
	CELL_SYSUTIL_GAME_PARENTAL_LEVEL08  = 8,
	CELL_SYSUTIL_GAME_PARENTAL_LEVEL09  = 9,
	CELL_SYSUTIL_GAME_PARENTAL_LEVEL10  = 10,
	CELL_SYSUTIL_GAME_PARENTAL_LEVEL11  = 11,
};

enum
{
	CELL_SYSUTIL_GAME_PARENTAL_LEVEL0_RESTRICT_OFF  = 0,
	CELL_SYSUTIL_GAME_PARENTAL_LEVEL0_RESTRICT_ON   = 1,
};

enum
{
	CELL_SYSUTIL_CAMERA_PLFREQ_DISABLED       = 0,
	CELL_SYSUTIL_CAMERA_PLFREQ_50HZ           = 1,
	CELL_SYSUTIL_CAMERA_PLFREQ_60HZ           = 2,
	CELL_SYSUTIL_CAMERA_PLFREQ_DEVCIE_DEPEND  = 4,
};

enum
{
	CELL_SYSUTIL_PAD_RUMBLE_OFF  = 0,
	CELL_SYSUTIL_PAD_RUMBLE_ON   = 1,
};

// License areas
enum CellSysutilLicenseArea : s32 // Made up name
{
	CELL_SYSUTIL_LICENSE_AREA_J = 0,       // SCEJ (Japan)
	CELL_SYSUTIL_LICENSE_AREA_A = 1,       // SCEA (North America, South America, Canada, Brazil)
	CELL_SYSUTIL_LICENSE_AREA_E = 2,       // SCEE (UK, Europe, Eastern Europe, Oceania, Russia)
	CELL_SYSUTIL_LICENSE_AREA_H = 3,       // SCEH (Hong Kong, Taiwan, Southeast Asia)
	CELL_SYSUTIL_LICENSE_AREA_K = 4,       // SCEK (Korea)
	CELL_SYSUTIL_LICENSE_AREA_C = 5,       // SCH  (China)
	CELL_SYSUTIL_LICENSE_AREA_OTHER = 100, // Other
};

enum
{
	CELL_SYSUTIL_PAD_AUTOOFF_OFF = 0,
	CELL_SYSUTIL_PAD_AUTOOFF_ON  = 1,
};

enum
{
	CELL_SYSUTIL_MAGNETOMETER_OFF = 0,
	CELL_SYSUTIL_MAGNETOMETER_ON  = 1,
};

enum
{
	CELL_SYSUTIL_INTERNET_BROWSER_START_RESTRICT_OFF = 0,
	CELL_SYSUTIL_INTERNET_BROWSER_START_RESTRICT_ON  = 1,
};

enum
{
	CELL_SYSCACHE_RET_OK_CLEARED      = 0,
	CELL_SYSCACHE_RET_OK_RELAYED      = 1,

	CELL_SYSCACHE_ID_SIZE             = 32,
	CELL_SYSCACHE_PATH_MAX            = 1055,
};

enum CellSysCacheError : u32
{
	CELL_SYSCACHE_ERROR_ACCESS_ERROR  = 0x8002bc01, // I don't think we need this
	CELL_SYSCACHE_ERROR_INTERNAL      = 0x8002bc02, // Not really useful, if we run out of HDD space sys_fs should handle that

	CELL_SYSCACHE_ERROR_PARAM         = 0x8002bc03,
	CELL_SYSCACHE_ERROR_NOTMOUNTED    = 0x8002bc04, // We don't really need to simulate the mounting, so this is probably useless
};

enum CellSysutilBgmPlaybackStatusState
{
	CELL_SYSUTIL_BGMPLAYBACK_STATUS_PLAY = 0,
	CELL_SYSUTIL_BGMPLAYBACK_STATUS_STOP = 1
};

enum CellSysutilBgmPlaybackStatusEnabled
{
	CELL_SYSUTIL_BGMPLAYBACK_STATUS_ENABLE = 0,
	CELL_SYSUTIL_BGMPLAYBACK_STATUS_DISABLE = 1
};

struct CellSysutilBgmPlaybackStatus
{
	u8 playerState;
	u8 enableState;
	char contentId[16];
	u8 currentFadeRatio;
	char reserved[13];
};

struct CellSysutilBgmPlaybackStatus2
{
	u8 playerState;
	char reserved[7];
};

struct CellSysutilBgmPlaybackExtraParam
{
	be_t<s32> systemBgmFadeInTime;
	be_t<s32> systemBgmFadeOutTime;
	be_t<s32> gameBgmFadeInTime;
	be_t<s32> gameBgmFadeOutTime;
	char reserved[8];
};

struct CellSysCacheParam
{
	char cacheId[CELL_SYSCACHE_ID_SIZE];
	char getCachePath[CELL_SYSCACHE_PATH_MAX];
	vm::bptr<void> reserved;
};

extern void sysutil_register_cb(std::function<s32(ppu_thread&)>&&);
extern u32 sysutil_send_system_cmd(u64 status, u64 param);
s32 sysutil_check_name_string(const char* src, s32 minlen, s32 maxlen);
