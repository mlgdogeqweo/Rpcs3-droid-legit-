#include "stdafx.h"
#include "Emu/Memory/Memory.h"
#include "Emu/System.h"

#include "Emu/IdManager.h"
#include "Emu/Cell/PPUThread.h"
#include "Emu/Cell/SPUThread.h"
#include "Emu/Cell/RawSPUThread.h"
#include "Emu/Cell/lv2/sys_lwmutex.h"
#include "Emu/Cell/lv2/sys_lwcond.h"
#include "Emu/Cell/lv2/sys_mutex.h"
#include "Emu/Cell/lv2/sys_cond.h"
#include "Emu/Cell/lv2/sys_semaphore.h"
#include "Emu/Cell/lv2/sys_event.h"
#include "Emu/Cell/lv2/sys_event_flag.h"
#include "Emu/Cell/lv2/sys_rwlock.h"
#include "Emu/Cell/lv2/sys_prx.h"
#include "Emu/Cell/lv2/sys_memory.h"
#include "Emu/Cell/lv2/sys_mmapper.h"
#include "Emu/Cell/lv2/sys_spu.h"
#include "Emu/Cell/lv2/sys_interrupt.h"
#include "Emu/Cell/lv2/sys_timer.h"
#include "Emu/Cell/lv2/sys_process.h"
#include "Emu/Cell/lv2/sys_fs.h"

#include "kernel_explorer.h"

kernel_explorer::kernel_explorer(QWidget* parent) : QDialog(parent)
{
	setWindowTitle(tr(u8"\u6838\u5FC3\u700F\u89BD\u5668"));
	setObjectName("kernel_explorer");
	setAttribute(Qt::WA_DeleteOnClose);
	setMinimumSize(QSize(700, 450));

	QVBoxLayout* vbox_panel = new QVBoxLayout();
	QHBoxLayout* hbox_buttons = new QHBoxLayout();
	QPushButton* button_refresh = new QPushButton(tr(u8"\u66F4\u65B0"), this);
	hbox_buttons->addWidget(button_refresh);
	hbox_buttons->addStretch();

	m_tree = new QTreeWidget(this);
	m_tree->setBaseSize(QSize(600, 300));
	m_tree->setWindowTitle(tr(u8"\u6838\u5FC3"));
	m_tree->header()->close();

	// Merge and display everything
	vbox_panel->addSpacing(10);
	vbox_panel->addLayout(hbox_buttons);
	vbox_panel->addSpacing(10);
	vbox_panel->addWidget(m_tree);
	vbox_panel->addSpacing(10);
	setLayout(vbox_panel);

	// Events
	connect(button_refresh, &QAbstractButton::clicked, this, &kernel_explorer::Update);

	Update();
};

constexpr auto qstr = QString::fromStdString;

void kernel_explorer::Update()
{
	m_tree->clear();

	const auto vm_block = vm::get(vm::user_space);

	if (!vm_block)
	{
		return;
	}

	const u32 total_memory_usage = vm_block->used();

	QTreeWidgetItem* root = new QTreeWidgetItem();
	root->setText(0, qstr(fmt::format((u8"\u8655\u7406, ID = 0x00000001, \u6574\u9AD4\u8A18\u61B6\u4F7F\u7528 = 0x%x (%0.2f MB)"), total_memory_usage, (float)total_memory_usage / (1024 * 1024))));
	m_tree->addTopLevelItem(root);

	union name64
	{
		u64 u64_data;
		char string[8];

		name64(u64 data)
			: u64_data(data & 0x00ffffffffffffffull)
		{
		}

		const char* operator+() const
		{
			return string;
		}
	};

	// TODO: FileSystem

	struct lv2_obj_rec
	{
		QTreeWidgetItem* node;
		u32 count{ 0 };

		lv2_obj_rec() = default;
		lv2_obj_rec(QTreeWidgetItem* node)
			: node(node)
		{
		}
	};

	auto l_addTreeChild = [=](QTreeWidgetItem *parent, QString text)
	{
		QTreeWidgetItem *treeItem = new QTreeWidgetItem();
		treeItem->setText(0, text);
		parent->addChild(treeItem);
		return treeItem;
	};

	std::vector<lv2_obj_rec> lv2_types(256);
	lv2_types[SYS_MEM_OBJECT] =									l_addTreeChild(root, (u8"\u8A18\u61B6"));
	lv2_types[SYS_MUTEX_OBJECT] =								l_addTreeChild(root, (u8"\u4E92\u65A5\u9396"));
	lv2_types[SYS_COND_OBJECT] =								l_addTreeChild(root, (u8"\u689D\u4EF6\u8B8A\u6578"));
	lv2_types[SYS_RWLOCK_OBJECT] =							l_addTreeChild(root, (u8"\u8B80\u5BEB\u5668\u9396"));
	lv2_types[SYS_INTR_TAG_OBJECT] =						l_addTreeChild(root, (u8"\u4E2D\u65B7\u6A19\u7C64"));
	lv2_types[SYS_INTR_SERVICE_HANDLE_OBJECT] = l_addTreeChild(root, (u8"\u4E2D\u65B7\u670D\u52D9\u8655\u7406"));
	lv2_types[SYS_EVENT_QUEUE_OBJECT] =					l_addTreeChild(root, (u8"\u4E8B\u4EF6\u968A\u5217"));
	lv2_types[SYS_EVENT_PORT_OBJECT] =					l_addTreeChild(root, (u8"\u4E8B\u4EF6\u57E0\u53E3"));
	lv2_types[SYS_TRACE_OBJECT] =								l_addTreeChild(root, (u8"\u8E64\u8DE1"));
	lv2_types[SYS_SPUIMAGE_OBJECT] =						l_addTreeChild(root, (u8"SPU \u5370\u8C61"));
	lv2_types[SYS_PRX_OBJECT] =									l_addTreeChild(root, (u8"\u6A21\u7D44"));
	lv2_types[SYS_SPUPORT_OBJECT] =							l_addTreeChild(root, (u8"SPU \u57E0\u53E3"));
	lv2_types[SYS_LWMUTEX_OBJECT] =							l_addTreeChild(root, (u8"\u8F15\u91CF\u7D1A\u4E92\u65A5"));
	lv2_types[SYS_TIMER_OBJECT] =								l_addTreeChild(root, (u8"\u8A08\u6642\u5668"));
	lv2_types[SYS_SEMAPHORE_OBJECT] =						l_addTreeChild(root, (u8"\u4FE1\u865F\u6A19"));
	lv2_types[SYS_LWCOND_OBJECT] =							l_addTreeChild(root, (u8"\u8F15\u91CF\u7D1A\u689D\u4EF6\u8B8A\u6578"));
	lv2_types[SYS_EVENT_FLAG_OBJECT] =					l_addTreeChild(root, (u8"\u4E8B\u4EF6\u6A19\u8A8C"));

	idm::select<lv2_obj>([&](u32 id, lv2_obj& obj)
	{
		lv2_types[id >> 24].count++;

		if (auto& node = lv2_types[id >> 24].node) switch (id >> 24)
		{
		case SYS_MEM_OBJECT:
		{
			// auto& mem = static_cast<lv2_memory&>(obj);
			l_addTreeChild(node, qstr(fmt::format("Memory: ID = 0x%08x", id)));
			break;
		}
		case SYS_MUTEX_OBJECT:
		{
			auto& mutex = static_cast<lv2_mutex&>(obj);
			l_addTreeChild(node, qstr(fmt::format("Mutex: ID = 0x%08x \"%s\",%s Owner = 0x%x, Locks = %u, Conds = %u, Wq = %zu", id, +name64(mutex.name),
				mutex.recursive == SYS_SYNC_RECURSIVE ? " Recursive," : "", mutex.owner >> 1, +mutex.lock_count, +mutex.cond_count, mutex.sq.size())));
			break;
		}
		case SYS_COND_OBJECT:
		{
			auto& cond = static_cast<lv2_cond&>(obj);
			l_addTreeChild(node, qstr(fmt::format("Cond: ID = 0x%08x \"%s\", Waiters = %u", id, +name64(cond.name), +cond.waiters)));
			break;
		}
		case SYS_RWLOCK_OBJECT:
		{
			auto& rw = static_cast<lv2_rwlock&>(obj);
			const s64 val = rw.owner;
			l_addTreeChild(node, qstr(fmt::format("RW Lock: ID = 0x%08x \"%s\", Owner = 0x%x(%d), Rq = %zu, Wq = %zu", id, +name64(rw.name),
				std::max<s64>(0, val >> 1), -std::min<s64>(0, val >> 1), rw.rq.size(), rw.wq.size())));
			break;
		}
		case SYS_INTR_TAG_OBJECT:
		{
			// auto& tag = static_cast<lv2_int_tag&>(obj);
			l_addTreeChild(node, qstr(fmt::format("Intr Tag: ID = 0x%08x", id)));
			break;
		}
		case SYS_INTR_SERVICE_HANDLE_OBJECT:
		{
			// auto& serv = static_cast<lv2_int_serv&>(obj);
			l_addTreeChild(node, qstr(fmt::format("Intr Svc: ID = 0x%08x", id)));
			break;
		}
		case SYS_EVENT_QUEUE_OBJECT:
		{
			auto& eq = static_cast<lv2_event_queue&>(obj);
			l_addTreeChild(node, qstr(fmt::format("Event Queue: ID = 0x%08x \"%s\", %s, Key = %#llx, Events = %zu/%d, Waiters = %zu", id, +name64(eq.name),
				eq.type == SYS_SPU_QUEUE ? "SPU" : "PPU", eq.key, eq.events.size(), eq.size, eq.sq.size())));
			break;
		}
		case SYS_EVENT_PORT_OBJECT:
		{
			auto& ep = static_cast<lv2_event_port&>(obj);
			l_addTreeChild(node, qstr(fmt::format("Event Port: ID = 0x%08x, Name = %#llx", id, ep.name)));
			break;
		}
		case SYS_TRACE_OBJECT:
		{
			l_addTreeChild(node, qstr(fmt::format("Trace: ID = 0x%08x", id)));
			break;
		}
		case SYS_SPUIMAGE_OBJECT:
		{
			l_addTreeChild(node, qstr(fmt::format("SPU Image: ID = 0x%08x", id)));
			break;
		}
		case SYS_PRX_OBJECT:
		{
			auto& prx = static_cast<lv2_prx&>(obj);
			l_addTreeChild(node, qstr(fmt::format("PRX: ID = 0x%08x '%s'", id, prx.name)));
			break;
		}
		case SYS_SPUPORT_OBJECT:
		{
			l_addTreeChild(node, qstr(fmt::format("SPU Port: ID = 0x%08x", id)));
			break;
		}
		case SYS_LWMUTEX_OBJECT:
		{
			auto& lwm = static_cast<lv2_lwmutex&>(obj);
			l_addTreeChild(node, qstr(fmt::format("LWMutex: ID = 0x%08x \"%s\", Wq = %zu", id, +name64(lwm.name), lwm.sq.size())));
			break;
		}
		case SYS_TIMER_OBJECT:
		{
			// auto& timer = static_cast<lv2_timer&>(obj);
			l_addTreeChild(node, qstr(fmt::format("Timer: ID = 0x%08x", id)));
			break;
		}
		case SYS_SEMAPHORE_OBJECT:
		{
			auto& sema = static_cast<lv2_sema&>(obj);
			l_addTreeChild(node, qstr(fmt::format("Semaphore: ID = 0x%08x \"%s\", Count = %d, Max Count = %d, Waiters = %#zu", id, +name64(sema.name),
				sema.val.load(), sema.max, sema.sq.size())));
			break;
		}
		case SYS_LWCOND_OBJECT:
		{
			auto& lwc = static_cast<lv2_cond&>(obj);
			l_addTreeChild(node, qstr(fmt::format("LWCond: ID = 0x%08x \"%s\", Waiters = %zu", id, +name64(lwc.name), +lwc.waiters)));
			break;
		}
		case SYS_EVENT_FLAG_OBJECT:
		{
			auto& ef = static_cast<lv2_event_flag&>(obj);
			l_addTreeChild(node, qstr(fmt::format("Event Flag: ID = 0x%08x \"%s\", Type = 0x%x, Pattern = 0x%llx, Wq = %zu", id, +name64(ef.name),
				ef.type, ef.pattern.load(), +ef.waiters)));
			break;
		}
		default:
		{
			l_addTreeChild(node, qstr(fmt::format("Unknown object: ID = 0x%08x", id)));
		}
		}
	});

	lv2_types.emplace_back(l_addTreeChild(root, (u8"\u8A18\u61B6\u5BB9\u5668")));

	idm::select<lv2_memory_container>([&](u32 id, lv2_memory_container&)
	{
		lv2_types.back().count++;
		l_addTreeChild(lv2_types.back().node, qstr(fmt::format("Memory Container: ID = 0x%08x", id)));
	});

	lv2_types.emplace_back(l_addTreeChild(root, (u8"PPU \u57F7\u884C\u7DD2")));

	idm::select<ppu_thread>([&](u32 id, ppu_thread& ppu)
	{
		lv2_types.back().count++;
		l_addTreeChild(lv2_types.back().node, qstr(fmt::format("PPU Thread: ID = 0x%08x '%s'", id, ppu.get_name())));
	});

	lv2_types.emplace_back(l_addTreeChild(root, (u8"SPU \u57F7\u884C\u7DD2")));

	idm::select<SPUThread>([&](u32 id, SPUThread& spu)
	{
		lv2_types.back().count++;
		l_addTreeChild(lv2_types.back().node, qstr(fmt::format("SPU Thread: ID = 0x%08x '%s'", id, spu.get_name())));
	});

	lv2_types.emplace_back(l_addTreeChild(root, (u8"SPU \u57F7\u884C\u7DD2\u7D44")));

	idm::select<lv2_spu_group>([&](u32 id, lv2_spu_group& tg)
	{
		lv2_types.back().count++;
		l_addTreeChild(lv2_types.back().node, qstr(fmt::format("SPU Thread Group: ID = 0x%08x '%s'", id, tg.name)));
	});

	lv2_types.emplace_back(l_addTreeChild(root, (u8"\u6A94\u6848\u63CF\u8FF0")));

	idm::select<lv2_fs_object>([&](u32 id, lv2_fs_object& fo)
	{
		lv2_types.back().count++;
		l_addTreeChild(lv2_types.back().node, qstr(fmt::format("FD: ID = 0x%08x '%s'", id, fo.name.data())));
	});

	for (auto&& entry : lv2_types)
	{
		if (entry.node && entry.count)
		{
			// Append object count
			entry.node->setText(0, entry.node->text(0) + qstr(fmt::format(" (%zu)", entry.count)));
		}
		else if (entry.node)
		{
			// Delete node otherwise
			delete entry.node;
		}
	}

	// RawSPU Threads (TODO)

	root->setExpanded(true);
}
