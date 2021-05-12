#pragma once

#include "util/types.hpp"
#include "../gcm_enums.h"

#include <concepts>

namespace rsx
{
#pragma pack(push, 1)
	struct fragment_program_texture_config
	{
		struct TIU_slot
		{
			float scale_x;
			float scale_y;
			u32 remap;
			u32 control;
		}
		slots_[16]; // QT headers will collide with any variable named 'slots' because reasons

		template <std::integral T>
		TIU_slot& operator[](T index) { return slots_[index]; }

		void write_to(void* dst, u16 mask);
		void load_from(const void* src, u16 mask);

		static void masked_transfer(void* dst, const void* src, u16 mask);
	};
#pragma pack(pop)

	struct fragment_program_texture_state
	{
		u32 texture_dimensions = 0;
		u16 unnormalized_coords = 0;
		u16 redirected_textures = 0;
		u16 shadow_textures = 0;
		u16 reserved = 0;

		void clear(u32 index);
		void import(const fragment_program_texture_state& other, u16 mask);
		void set_dimension(texture_dimension_extended type, u32 index);
		bool operator == (const fragment_program_texture_state& other) const;
	};

	struct vertex_program_texture_state
	{
		u32 texture_dimensions = 0;

		void clear(u32 index);
		void import(const vertex_program_texture_state& other, u16 mask);
		void set_dimension(texture_dimension_extended type, u32 index);
		bool operator == (const vertex_program_texture_state& other) const;
	};
}
