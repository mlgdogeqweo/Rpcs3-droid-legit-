﻿#pragma once

#include <vector>

namespace input
{
	enum class product_type
	{
		playstation_3_controller,
		red_octane_gh_guitar,
		red_octane_gh_drum_kit,
		dance_dance_revolution_mat,
		dj_hero_turntable,
		harmonix_rockband_guitar,
		harmonix_rockband_drum_kit,
	};

	enum vendor_id
	{
		sony_corp = 0x054C, // Sony Corp.
		sony_cea  = 0x12BA, // Sony Computer Entertainment America
		konami_de = 0x1CCF, // Konami Digital Entertainment
	};

	enum product_id
	{
		red_octane_gh_guitar       = 0x0100, // RedOctane Guitar (Guitar Hero)
		red_octane_gh_drum_kit     = 0x0120, // RedOctane Drum Kit (Guitar Hero)
		dance_dance_revolution_mat = 0x0140, // Dance Dance Revolution Mat
		dj_hero_turntable          = 0x0140, // DJ Hero Turntable
		harmonix_rockband_guitar   = 0x0200, // Harmonix Guitar (Rock Band)
		harmonix_rockband_drum_kit = 0x0210, // Harmonix Drum Kit (Rock Band)
		playstation_3_controller   = 0x0268, // PlayStation 3 Controller
	};

	struct product_info
	{
		product_type type;
		uint16_t vendor_id;
		uint16_t product_id;
	};

	static product_info get_product_info(product_type type)
	{
		switch (type)
		{
		default:
		case product_type::playstation_3_controller:
		{
			return product_info{ type, vendor_id::sony_corp, product_id::playstation_3_controller };
		}
		case product_type::dance_dance_revolution_mat:
		{
			return product_info{ type, vendor_id::konami_de, product_id::dance_dance_revolution_mat };
		}
		case product_type::dj_hero_turntable:
		{
			return product_info{ type, vendor_id::sony_cea, product_id::dj_hero_turntable };
		}
		case product_type::harmonix_rockband_drum_kit:
		{
			return product_info{ type, vendor_id::sony_cea, product_id::harmonix_rockband_drum_kit };
		}
		case product_type::harmonix_rockband_guitar:
		{
			return product_info{ type, vendor_id::sony_cea, product_id::harmonix_rockband_guitar };
		}
		case product_type::red_octane_gh_drum_kit:
		{
			return product_info{ type, vendor_id::sony_cea, product_id::red_octane_gh_drum_kit };
		}
		case product_type::red_octane_gh_guitar:
		{
			return product_info{ type, vendor_id::sony_cea, product_id::red_octane_gh_guitar };
		}
		}
	}

	static std::vector<product_info> get_products_by_class(int class_id)
	{
		switch (class_id)
		{
		default:
		case 0: // CELL_PAD_PCLASS_TYPE_STANDARD
		{
			return
			{
				get_product_info(product_type::playstation_3_controller)
			};
		}
		case 1: // CELL_PAD_PCLASS_TYPE_GUITAR
		{
			return
			{
				get_product_info(product_type::red_octane_gh_guitar),
				get_product_info(product_type::harmonix_rockband_guitar)
			};
		}
		case 2: // CELL_PAD_PCLASS_TYPE_DRUM
		{
			return
			{
				get_product_info(product_type::red_octane_gh_drum_kit),
				get_product_info(product_type::harmonix_rockband_drum_kit)
			};
		}
		case 3: // CELL_PAD_PCLASS_TYPE_DJ
		{
			return
			{
				get_product_info(product_type::dj_hero_turntable)
			};
		}
		case 4: // CELL_PAD_PCLASS_TYPE_DANCEMAT
		{
			return
			{
				get_product_info(product_type::dance_dance_revolution_mat)
			};
		}
		case 5: // CELL_PAD_PCLASS_TYPE_NAVIGATION
		{
			return
			{
				get_product_info(product_type::playstation_3_controller)
			};
		}
		}
	}
};
