// This makes debugging on windows less painful
//#define HAVE_LIBEVDEV

#ifdef HAVE_LIBEVDEV

#include "stdafx.h"
#include "evdev_gun_handler.h"
#include "util/logs.hpp"

#include <libudev.h>
#include <fcntl.h>
#include <linux/input.h>
#include <unistd.h>

LOG_CHANNEL(evdev_log, "evdev");

constexpr usz max_devices = 8;

const std::map<gun_button, int> button_map
{
	{gun_button::btn_left, BTN_LEFT},
	{gun_button::btn_right, BTN_RIGHT},
	{gun_button::btn_middle, BTN_MIDDLE},
	{gun_button::btn_1, BTN_1},
	{gun_button::btn_2, BTN_2},
	{gun_button::btn_3, BTN_3},
	{gun_button::btn_4, BTN_4},
	{gun_button::btn_5, BTN_5}
};

struct event_udev_entry
{
	const char* devnode = nullptr;
	struct udev_list_entry* item = nullptr;
};

bool event_is_number(const char* s)
{
	if (!s)
		return false;

	const usz len = strlen(s);
	if (len == 0)
		return false;

	for (usz n = 0; n < len; n++)
	{
		if (s[n] < '0' || s[n] > '9')
			return false;
	}
	return true;
}

// compare /dev/input/eventX and /dev/input/eventY where X and Y are numbers
int event_strcmp_events(const char* x, const char* y)
{
	// find a common string
	int n = 0;
	while (x && y && x[n] == y[n] && x[n] != '\0' && y[n] != '\0')
	{
		n++;
	}

	// check if remaining string is a number
	if (event_is_number(x + n) && event_is_number(y + n))
	{
		const int a = atoi(x + n);
		const int b = atoi(y + n);

		if (a == b)
			return 0;
		if (a < b)
			return -1;
		return 1;
	}

	return strcmp(x, y);
}

evdev_gun_handler::evdev_gun_handler()
{
}

evdev_gun_handler::~evdev_gun_handler()
{
	for (evdev_gun& gun : m_devices)
	{
		close(gun.fd);
	}
	if (m_udev != nullptr)
		udev_unref(m_udev);
	evdev_log.notice("Lightgun: Shutdown udev initialization");
}

int evdev_gun_handler::get_button(u32 gunno, gun_button button) const
{
	const auto& buttons = ::at32(m_devices, gunno).buttons;

	if (const auto it = buttons.find(::at32(button_map, button)); it != buttons.end())
	{
		return it->second;
	}

	return 0;
}

int evdev_gun_handler::get_axis_x(u32 gunno) const
{
	const evdev_axis& axis = ::at32(::at32(m_devices, gunno).axis, ABS_X);
	return axis.value - axis.min;
}

int evdev_gun_handler::get_axis_y(u32 gunno) const
{
	const evdev_axis& axis = ::at32(::at32(m_devices, gunno).axis, ABS_Y);
	return axis.value - axis.min;
}

int evdev_gun_handler::get_axis_x_max(u32 gunno) const
{
	const evdev_axis& axis = ::at32(::at32(m_devices, gunno).axis, ABS_X);
	return axis.max - axis.min;
}

int evdev_gun_handler::get_axis_y_max(u32 gunno) const
{
	const evdev_axis& axis = ::at32(::at32(m_devices, gunno).axis, ABS_Y);
	return axis.max - axis.min;
}

void evdev_gun_handler::poll(u32 index)
{
	if (!m_is_init || index >= m_devices.size())
		return;

	std::array<input_event, 32> input_events;
	evdev_gun& gun = ::at32(m_devices, index);

	if (usz len = read(gun.fd, input_events.data(), input_events.size() * sizeof(input_event)); len > 0)
	{
		len /= sizeof(input_event);

		for (usz i = 0; i < std::min(len, input_events.size()); i++)
		{
			const input_event& evt = input_events[i];

			switch (evt.type)
			{
			case EV_KEY:
				gun.buttons[evt.code] = evt.value;
				break;
			case EV_ABS:
				gun.axis[evt.code].value = evt.value;
				break;
			default:
				break;
			}
		}
	}
}

bool evdev_gun_handler::is_init() const
{
	return m_is_init;
}

u32 evdev_gun_handler::get_num_guns() const
{
	return ::narrow<u32>(m_devices.size());
}

bool evdev_gun_handler::init()
{
	if (m_is_init)
		return true;

	evdev_log.notice("Lightgun: Begin udev initialization");

	m_devices.clear();

	m_udev = udev_new();
	if (m_udev == nullptr)
	{
		evdev_log.error("Lightgun: Failed udev initialization");
		return false;
	}

	if (udev_enumerate* enumerate = udev_enumerate_new(m_udev))
	{
		udev_enumerate_add_match_property(enumerate, "ID_INPUT_MOUSE", "1");
		udev_enumerate_add_match_subsystem(enumerate, "input");
		udev_enumerate_scan_devices(enumerate);
		udev_list_entry* devs = udev_enumerate_get_list_entry(enumerate);

		std::vector<event_udev_entry> sorted_devices;

		for (udev_list_entry* item = devs; item && sorted_devices.size() < max_devices; item = udev_list_entry_get_next(item))
		{
			const char* name = udev_list_entry_get_name(item);
			udev_device* dev = udev_device_new_from_syspath(m_udev, name);
			const char* devnode = udev_device_get_devnode(dev);

			if (devnode != nullptr)
			{
				event_udev_entry new_device{};
				new_device.devnode = devnode;
				new_device.item = item;
				sorted_devices.push_back(std::move(new_device));
			}
			else
			{
				udev_device_unref(dev);
			}
		}

		// Sort the udev entries by devnode name so that they are created in the proper order
		std::sort(sorted_devices.begin(), sorted_devices.end(), [](const event_udev_entry& a, const event_udev_entry& b)
		{
			return event_strcmp_events(a.devnode, b.devnode);
		});

		for (const event_udev_entry& entry : sorted_devices)
		{
			// Get the filename of the /sys entry for the device and create a udev_device object (dev) representing it.
			const char* name = udev_list_entry_get_name(entry.item);
			evdev_log.notice("Lightgun: found device %s", name);

			udev_device* dev = udev_device_new_from_syspath(m_udev, name);
			const char* devnode = udev_device_get_devnode(dev);

			if (devnode)
			{
				if (int fd = open(devnode, O_RDONLY | O_NONBLOCK); fd != -1)
				{
					input_absinfo absx, absy;
					if (ioctl(fd, EVIOCGABS(ABS_X), &absx) >= 0 &&
						ioctl(fd, EVIOCGABS(ABS_Y), &absy) >= 0)
					{
						evdev_log.notice("Lightgun: Adding device %d: %s, absx(%i, %i), absy(%i, %i)", m_devices.size(), name, absx.minimum, absx.maximum, absy.minimum, absy.maximum);

						evdev_gun gun{};
						gun.fd = fd;
						gun.axis[ABS_X].min = absx.minimum;
						gun.axis[ABS_X].max = absx.maximum;
						gun.axis[ABS_Y].min = absy.minimum;
						gun.axis[ABS_Y].max = absy.maximum;
						m_devices.push_back(gun);

						if (m_devices.size() >= max_devices)
							break;
					}
					else
					{
						evdev_log.notice("Lightgun: device %s not valid. abs_x and abs_y not found", name);
						close(fd);
					}
				}
			}
			udev_device_unref(dev);
		}
		udev_enumerate_unref(enumerate);
	}
	else
	{
		evdev_log.error("Lightgun: Failed udev enumeration");
	}

	m_is_init = true;
	return true;
}

#endif
