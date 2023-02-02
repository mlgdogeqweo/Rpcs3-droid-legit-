#include "stdafx.h"
#include "overlay_animated_icon.h"

#include "Utilities/File.h"
#include "../Common/time.hpp"

namespace rsx
{
	namespace overlays
	{
		animated_icon::animated_icon(const char* icon_name)
		{
			const std::string image_path = fmt::format("%s/Icons/ui/%s", fs::get_config_dir(), icon_name);
			m_icon = std::make_unique<image_info>(image_path.c_str());
			set_raw_image(m_icon.get());
		}

		void animated_icon::update_animation_frame(compiled_resource& result)
		{
			if (m_last_update_timestamp == 0)
			{
				m_last_update_timestamp = rsx::uclock();
			}
			else
			{
				const auto now = rsx::uclock();
				m_current_frame_duration += (rsx::uclock() - m_last_update_timestamp);
				m_last_update_timestamp = now;
			}

			if (m_current_frame_duration > m_frame_duration)
			{
				m_current_frame = (m_current_frame + 1) % m_total_frames;
				m_current_frame_duration = 0;
			}

			// We only care about the uvs (zw) components
			float x = f32(m_frame_width + m_spacing_x) * (m_current_frame % m_row_length) + m_start_x;
			float y = f32(m_frame_height + m_spacing_y) * (m_current_frame / m_row_length) + m_start_y;

			auto& cmd = result.draw_commands[0];
			cmd.verts[0].z() = x / m_icon->w;
			cmd.verts[0].w() = (y / m_icon->h);
			cmd.verts[1].z() = (x + m_frame_width) / m_icon->w;
			cmd.verts[1].w() = (y / m_icon->h);
			cmd.verts[2].z() = x / m_icon->w;
			cmd.verts[2].w() = ((y + m_frame_height) / m_icon->h);
			cmd.verts[3].z() = (x + m_frame_width) / m_icon->w;
			cmd.verts[3].w() = ((y + m_frame_height) / m_icon->h);
		}

		compiled_resource& animated_icon::get_compiled()
		{
			if (!is_compiled)
			{
				compiled_resources = image_view::get_compiled();
			}

			update_animation_frame(compiled_resources);
			return compiled_resources;
		}
	}
}
