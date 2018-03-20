#pragma once

#include "Emu/Io/PadHandler.h"

class NullPadHandler final : public PadHandlerBase
{
public:
	bool Init() override
	{
		return true;
	}

	void init_config(pad_config* /*cfg*/, const std::string& /*name*/) override
	{
	}

	std::vector<std::string> ListDevices() override
	{
		std::vector<std::string> nulllist;
		nulllist.push_back(u8"�w�]�Ÿ˸m");
		return nulllist;
	}

	bool bindPadToDevice(std::shared_ptr<Pad> /*pad*/, const std::string& /*device*/) override
	{
		return true;
	}

	void ThreadProc() override
	{
	}
};
