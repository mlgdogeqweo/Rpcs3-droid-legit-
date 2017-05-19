#include "debuggerframe.h"

inline QString qstr(const std::string& _in) { return QString::fromUtf8(_in.data(), _in.size()); }

DebuggerFrame::DebuggerFrame(QWidget *parent) : QDockWidget(tr("Debugger"), parent)
{
	m_pc = 0;
	m_item_count = 30;
	pSize = 10;

	update = new QTimer(this);
	connect(update, &QTimer::timeout, this, &DebuggerFrame::UpdateUI);
	EnableUpdateTimer(true);

	body = new QWidget(this);
	mono = QFontDatabase::systemFont(QFontDatabase::FixedFont);
	mono.setPointSize(pSize);
	QFontMetrics* fontMetrics = new QFontMetrics(mono);

	QVBoxLayout* vbox_p_main = new QVBoxLayout();
	QHBoxLayout* hbox_b_main = new QHBoxLayout();

	m_list = new QListWidget(this);
	m_choice_units = new QComboBox(this);
	m_choice_units->setSizeAdjustPolicy(QComboBox::AdjustToContents);
	m_choice_units->setMaxVisibleItems(30);
	m_choice_units->setMaximumWidth(500);

	QPushButton* b_go_to_addr = new QPushButton(tr("Go To Address"), this);
	QPushButton* b_go_to_pc = new QPushButton(tr("Go To PC"), this);

	m_btn_step = new QPushButton(tr("Step"), this);
	m_btn_run = new QPushButton(tr("Run"), this);
	m_btn_pause = new QPushButton(tr("Pause"), this);

	hbox_b_main->addWidget(b_go_to_addr);
	hbox_b_main->addWidget(b_go_to_pc);
	hbox_b_main->addWidget(m_btn_step);
	hbox_b_main->addWidget(m_btn_run);
	hbox_b_main->addWidget(m_btn_pause);
	hbox_b_main->addWidget(m_choice_units);
	hbox_b_main->addStretch();

	//Registers
	m_regs = new QTextEdit(this);
	m_regs->setLineWrapMode(QTextEdit::NoWrap);
	m_regs->setTextInteractionFlags(Qt::TextSelectableByMouse | Qt::TextSelectableByKeyboard);

	m_list->setFont(mono);
	m_regs->setFont(mono);

	QHBoxLayout* hbox_w_list = new QHBoxLayout();
	hbox_w_list->addWidget(m_list);
	hbox_w_list->addWidget(m_regs);

	vbox_p_main->addLayout(hbox_b_main);
	vbox_p_main->addLayout(hbox_w_list);

	body->setLayout(vbox_p_main);
	setWidget(body);
	
	m_list->setWindowTitle(tr("ASM"));
	for (uint i = 0; i<m_item_count; ++i)
	{
		m_list->insertItem(i, new QListWidgetItem(""));
	}
	m_list->setSizeAdjustPolicy(QListWidget::AdjustToContents);

	connect(b_go_to_addr, &QAbstractButton::clicked, this, &DebuggerFrame::Show_Val);
	connect(b_go_to_pc, &QAbstractButton::clicked, this, &DebuggerFrame::Show_PC);
	connect(m_btn_step, &QAbstractButton::clicked, this, &DebuggerFrame::DoStep);
	connect(m_btn_run, &QAbstractButton::clicked, this, &DebuggerFrame::DoRun);
	connect(m_btn_pause, &QAbstractButton::clicked, this, &DebuggerFrame::DoPause);
	connect(m_choice_units, static_cast<void (QComboBox::*)(int)>(&QComboBox::activated), this, &DebuggerFrame::UpdateUI);
	connect(m_choice_units, static_cast<void (QComboBox::*)(int)>(&QComboBox::currentIndexChanged), this, &DebuggerFrame::OnSelectUnit);
	connect(this, &QDockWidget::visibilityChanged, this, &DebuggerFrame::EnableUpdateTimer);

	ShowAddr(CentrePc(m_pc));
	UpdateUnitList();
}


void DebuggerFrame::closeEvent(QCloseEvent *event)
{
	QDockWidget::closeEvent(event);
	emit DebugFrameClosed();
}

//static const int show_lines = 30;
#include <map>

std::map<u32, bool> g_breakpoints;

extern void ppu_breakpoint(u32 addr);

u32 DebuggerFrame::GetPc() const
{
	const auto cpu = this->cpu.lock();

	if (!cpu)
	{
		return 0;
	}

	switch (g_system)
	{
	case system_type::ps3: return cpu->id_type() == 1 ? static_cast<ppu_thread*>(cpu.get())->cia : static_cast<SPUThread*>(cpu.get())->pc;
	case system_type::psv: return static_cast<ARMv7Thread*>(cpu.get())->PC;
	}

	return 0xabadcafe;
}

u32 DebuggerFrame::CentrePc(u32 pc) const
{
	return pc/* - ((m_item_count / 2) * 4)*/;
}

void DebuggerFrame::UpdateUI()
{
	UpdateUnitList();

	const auto cpu = this->cpu.lock();

	if (!cpu)
	{
		if (m_last_pc != -1 || m_last_stat)
		{
			m_last_pc = -1;
			m_last_stat = 0;
			DoUpdate();

			m_btn_run->setEnabled(false);
			m_btn_step->setEnabled(false);
			m_btn_pause->setEnabled(false);
		}
	}
	else
	{
		const auto cia = GetPc();
		const auto state = cpu->state.load();

		if (m_last_pc != cia || m_last_stat != static_cast<u32>(state))
		{
			m_last_pc = cia;
			m_last_stat = static_cast<u32>(state);
			DoUpdate();

			if (test(state & cpu_flag::dbg_pause))
			{
				m_btn_run->setEnabled(true);
				m_btn_step->setEnabled(true);
				m_btn_pause->setEnabled(false);
			}
			else
			{
				m_btn_run->setEnabled(false);
				m_btn_step->setEnabled(false);
				m_btn_pause->setEnabled(true);
			}
		}
	}

	if (Emu.IsStopped())
	{
		g_breakpoints.clear();
	}
}

void DebuggerFrame::UpdateUnitList()
{
	const u64 threads_created = cpu_thread::g_threads_created;
	const u64 threads_deleted = cpu_thread::g_threads_deleted;

	if (threads_created != m_threads_created || threads_deleted != m_threads_deleted)
	{
		m_threads_created = threads_created;
		m_threads_deleted = threads_deleted;
	}
	else
	{
		// Nothing to do
		return;
	}

	m_choice_units->clear();

	const auto on_select = [&](u32, cpu_thread& cpu)
	{
		QVariant var_cpu = qVariantFromValue((void *)&cpu);
		m_choice_units->addItem(qstr(cpu.get_name()), var_cpu);
	};

	idm::select<ppu_thread>(on_select);
	idm::select<ARMv7Thread>(on_select);
	idm::select<RawSPUThread>(on_select);
	idm::select<SPUThread>(on_select);

	m_choice_units->update();
}

void DebuggerFrame::OnSelectUnit()
{
	m_disasm.reset();

	const auto on_select = [&](u32, cpu_thread& cpu)
	{
		cpu_thread* data = (cpu_thread *)m_choice_units->currentData().value<void *>();
		return m_list->item(data == &cpu);
	};

	if (auto ppu = idm::select<ppu_thread>(on_select))
	{
		m_disasm = std::make_unique<PPUDisAsm>(CPUDisAsm_InterpreterMode);
		cpu = ppu.ptr;
	}
	else if (auto spu1 = idm::select<SPUThread>(on_select))
	{
		m_disasm = std::make_unique<SPUDisAsm>(CPUDisAsm_InterpreterMode);
		cpu = spu1.ptr;
	}
	else if (auto rspu = idm::select<RawSPUThread>(on_select))
	{
		m_disasm = std::make_unique<SPUDisAsm>(CPUDisAsm_InterpreterMode);
		cpu = rspu.ptr;
	}
	else if (auto arm = idm::select<ARMv7Thread>(on_select))
	{
		m_disasm = std::make_unique<ARMv7DisAsm>(CPUDisAsm_InterpreterMode);
		cpu = arm.ptr;
	}

	DoUpdate();
}

//void DebuggerFrame::resizeEvent(QResizeEvent* event)
//{
//	if (0)
//	{
//		if (!m_list->rowCount())
//		{
//			m_list->InsertItem(m_list->rowCount(), "");
//		}
//
//		int size = 0;
//		m_list->clear();
//		int item = 0;
//		while (size < m_list->GetSize().GetHeight())
//		{
//			item = m_list->rowCount();
//			m_list->InsertItem(item, "");
//			QRect rect;
//			m_list->GetItemRect(item, rect);
//
//			size = rect.GetBottom();
//		}
//
//		if (item)
//		{
//			m_list->removeRow(--item);
//		}
//
//		m_item_count = item;
//		ShowAddr(m_pc);
//	}
//}

void DebuggerFrame::DoUpdate()
{
	Show_PC();
	WriteRegs();
}

void DebuggerFrame::ShowAddr(u32 addr)
{
	m_pc = addr;

	const auto cpu = this->cpu.lock();

	if (!cpu)
	{
		for (uint i = 0; i<m_item_count; ++i, m_pc += 4)
		{
			m_list->item(i)->setText(qstr(fmt::format("[%08x] illegal address", m_pc)));
		}
	}
	else
	{
		const u32 cpu_offset = g_system == system_type::ps3 && cpu->id_type() != 1 ? static_cast<SPUThread&>(*cpu).offset : 0;
		m_disasm->offset = (u8*)vm::base(cpu_offset);
		for (uint i = 0, count = 4; i<m_item_count; ++i, m_pc += count)
		{
			if (!vm::check_addr(cpu_offset + m_pc, 4))
			{
				m_list->item(i)->setText(IsBreakPoint(m_pc) ? ">>> " : "    " + qstr(fmt::format("[%08x] illegal address", m_pc)));
				count = 4;
				continue;
			}

			count = m_disasm->disasm(m_disasm->dump_pc = m_pc);

			m_list->item(i)->setText(IsBreakPoint(m_pc) ? ">>> " : "    " + qstr(m_disasm->last_opcode));

			QColor colour;

			if (test(cpu->state & cpu_state_pause) && m_pc == GetPc())
			{
				colour = QColor(Qt::green);
			}
			else
			{
				colour = QColor(IsBreakPoint(m_pc) ? Qt::yellow : Qt::white);
			}

			m_list->item(i)->setBackgroundColor(colour);
		}
	}

	m_list->setLineWidth(-1);
}

void DebuggerFrame::WriteRegs()
{
	const auto cpu = this->cpu.lock();

	if (!cpu)
	{
		m_regs->clear();
		return;
	}

	m_regs->clear();
	m_regs->setText(qstr(cpu->dump()));
}

void DebuggerFrame::OnUpdate()
{
	//WriteRegs();
}

void DebuggerFrame::Show_Val()
{
	QDialog* diag = new QDialog(this);
	diag->setWindowTitle(tr("Set value"));
	diag->setModal(true);

	QPushButton* button_ok = new QPushButton(tr("Ok"));
	QPushButton* button_cancel = new QPushButton(tr("Cancel"));
	QVBoxLayout* vbox_panel(new QVBoxLayout());
	QHBoxLayout* hbox_text_panel(new QHBoxLayout());
	QHBoxLayout* hbox_button_panel(new QHBoxLayout());
	QLineEdit* p_pc(new QLineEdit(diag));
	p_pc->setFont(mono);
	p_pc->setMaxLength(8);
	p_pc->setFixedWidth(75);
	QLabel* addr(new QLabel(diag));
	addr->setFont(mono);
	
	hbox_text_panel->addWidget(addr);
	hbox_text_panel->addWidget(p_pc);

	hbox_button_panel->addWidget(button_ok);
	hbox_button_panel->addWidget(button_cancel);
	
	vbox_panel->addLayout(hbox_text_panel);
	vbox_panel->addSpacing(8);
	vbox_panel->addLayout(hbox_button_panel);
	
	diag->setLayout(vbox_panel);

	const auto cpu = this->cpu.lock();

	if (cpu) 
	{
		unsigned long pc = cpu ? GetPc() : 0x0;
		bool ok;
		addr->setText("Address: " + QString("%1").arg(pc, 8, 16, QChar('0')));	// set address input line to 8 digits
		p_pc->setPlaceholderText(QString("%1").arg(pc, 8, 16, QChar('0')));
	}
	else
	{
		p_pc->setPlaceholderText("00000000");
		addr->setText("Address: 00000000");
	}

	auto l_changeLabel = [=]()
	{
		if (p_pc->text().isEmpty())
		{
			addr->setText("Address: " + p_pc->placeholderText());
		}
		else
		{
			bool ok;
			ulong ul_addr = p_pc->text().toULong(&ok, 16);
			addr->setText("Address: " + QString("%1").arg(ul_addr, 8, 16, QChar('0'))); // set address input line to 8 digits
		}
	};

	connect(p_pc, &QLineEdit::textChanged, l_changeLabel);
	connect(button_ok, &QAbstractButton::clicked, diag, &QDialog::accept);
	connect(button_cancel, &QAbstractButton::clicked, diag, &QDialog::reject);;

	if (diag->exec() == QDialog::Accepted)
	{
		unsigned long pc = cpu ? GetPc() : 0x0;
		if (p_pc->text().isEmpty())
		{
			addr->setText(p_pc->placeholderText());
		}
		else
		{
			bool ok;
			pc = p_pc->text().toULong(&ok, 16);
			addr->setText(p_pc->text());
		}
		ShowAddr(CentrePc(pc));
	}
}

void DebuggerFrame::Show_PC()
{
	if (const auto cpu = this->cpu.lock()) ShowAddr(CentrePc(GetPc()));
}

void DebuggerFrame::DoRun()
{
	const auto cpu = this->cpu.lock();

	if (cpu && cpu->state.test_and_reset(cpu_flag::dbg_pause))
	{
		if (!test(cpu->state, cpu_flag::dbg_pause + cpu_flag::dbg_global_pause))
		{
			cpu->notify();
		}
	}
	UpdateUI();
}

void DebuggerFrame::DoPause()
{
	if (const auto cpu = this->cpu.lock())
	{
		cpu->state += cpu_flag::dbg_pause;
	}
	UpdateUI();
}

void DebuggerFrame::DoStep()
{
	if (const auto cpu = this->cpu.lock())
	{
		if (test(cpu_flag::dbg_pause, cpu->state.fetch_op([](bs_t<cpu_flag>& state)
		{
			state += cpu_flag::dbg_step;
			state -= cpu_flag::dbg_pause;
		})))
		{
			cpu->notify();
		}
	}
	UpdateUI();
}

void DebuggerFrame::keyPressEvent(QKeyEvent* event)
{
	if (!isActiveWindow())
	{
		return;
	}

	const auto cpu = this->cpu.lock();
	long i = m_list->currentRow();

	if (i < 0 || !cpu)
	{
		return;
	}

	const u32 start_pc = m_pc - m_item_count * 4;
	const u32 pc = start_pc + i * 4;

	if (event->key() == Qt::Key_Space && QApplication::keyboardModifiers() & Qt::ControlModifier)
	{
		DoStep();
		return;
	}
	else
	{
		switch (event->key())
		{
		case Qt::Key_PageUp:   ShowAddr(m_pc - (m_item_count * 2) * 4); return;
		case Qt::Key_PageDown: ShowAddr(m_pc); return;
		case Qt::Key_Up:       ShowAddr(m_pc - (m_item_count + 1) * 4); return;
		case Qt::Key_Down:     ShowAddr(m_pc - (m_item_count - 1) * 4); return;
		case Qt::Key_E:
		{
			InstructionEditorDialog dlg(this, pc, cpu, m_disasm.get());
			dlg.exec();
			DoUpdate();
			return;
		}
		case Qt::Key_R:
		{
			RegisterEditorDialog* dlg = new RegisterEditorDialog(this, pc, cpu, m_disasm.get());
			dlg->show();
			DoUpdate();
			return;
		}
		}
	}
}

void DebuggerFrame::mouseDoubleClickEvent(QMouseEvent* event)
{
	long i = m_list->currentRow();
	if (i < 0) return;

	const u32 start_pc = m_pc - m_item_count * 4;
	const u32 pc = start_pc + i * 4;
	//ConLog.Write("pc=0x%llx", pc);

	if (IsBreakPoint(pc))
	{
		RemoveBreakPoint(pc);
	}
	else
	{
		AddBreakPoint(pc);
	}

	ShowAddr(start_pc);
}

void DebuggerFrame::wheelEvent(QWheelEvent* event)
{
	QPoint numSteps = event->angleDelta() / 8 / 15;	// http://doc.qt.io/qt-5/qwheelevent.html#pixelDelta
	const int value = numSteps.y();

	ShowAddr(m_pc - (event->modifiers() == Qt::ControlModifier ? m_item_count * (value + 1) : m_item_count + value) * 4);
}

bool DebuggerFrame::IsBreakPoint(u32 pc)
{
	return g_breakpoints.count(pc) != 0;
}

void DebuggerFrame::AddBreakPoint(u32 pc)
{
	g_breakpoints.emplace(pc, false);
	ppu_breakpoint(pc);
}

void DebuggerFrame::RemoveBreakPoint(u32 pc)
{
	g_breakpoints.erase(pc);
	ppu_breakpoint(pc);
}

void DebuggerFrame::EnableUpdateTimer(bool enable)
{
	enable ? update->start(50) : update->stop();
}
