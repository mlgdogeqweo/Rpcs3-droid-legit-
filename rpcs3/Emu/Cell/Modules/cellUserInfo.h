#pragma once

namespace vm { using namespace ps3; }

// Return Codes
enum CellUserInfoError : s32
{
	CELL_USERINFO_RET_OK          = 0,
	CELL_USERINFO_RET_CANCEL      = 1,
	CELL_USERINFO_ERROR_BUSY      = ERROR_CODE(0x8002c301),
	CELL_USERINFO_ERROR_INTERNAL  = ERROR_CODE(0x8002c302),
	CELL_USERINFO_ERROR_PARAM     = ERROR_CODE(0x8002c303),
	CELL_USERINFO_ERROR_NOUSER    = ERROR_CODE(0x8002c304),
};

template<>
inline const char* ppu_error_code::print(CellUserInfoError error)
{
	switch (error)
	{
		STR_CASE(CELL_USERINFO_ERROR_BUSY);
		STR_CASE(CELL_USERINFO_ERROR_INTERNAL);
		STR_CASE(CELL_USERINFO_ERROR_PARAM);
		STR_CASE(CELL_USERINFO_ERROR_NOUSER);
	}

	return nullptr;
}

// Enums
enum CellUserInfoParamSize
{
	CELL_USERINFO_USER_MAX      = 16,
	CELL_USERINFO_TITLE_SIZE    = 256,
	CELL_USERINFO_USERNAME_SIZE = 64,
};

enum CellUserInfoListType
{
	CELL_USERINFO_LISTTYPE_ALL       = 0,
	CELL_USERINFO_LISTTYPE_NOCURRENT = 1,
};

enum
{
	CELL_SYSUTIL_USERID_CURRENT  = 0,
	CELL_SYSUTIL_USERID_MAX      = 99999999,
};

// Structs
struct CellUserInfoUserStat
{
	be_t<u32> id;
	char name[CELL_USERINFO_USERNAME_SIZE];
};

struct CellUserInfoUserList
{
	be_t<u32> userId[CELL_USERINFO_USER_MAX];
};

struct CellUserInfoListSet
{
	vm::bptr<char> title;
	be_t<u32> focus; // id
	be_t<u32> fixedListNum;
	vm::bptr<CellUserInfoUserList> fixedList;
	vm::bptr<void> reserved;
};

struct CellUserInfoTypeSet
{
	vm::bptr<char> title;
	be_t<u32> focus; // id
	be_t<u32> type; // CellUserInfoListType
	vm::bptr<void> reserved;
};
