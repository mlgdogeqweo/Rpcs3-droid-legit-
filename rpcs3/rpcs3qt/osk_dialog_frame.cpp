#include "osk_dialog_frame.h"
#include "custom_dialog.h"
#include "Emu/Cell/Modules/cellMsgDialog.h"

#include "util/asm.hpp"

#include <QDialogButtonBox>
#include <QLabel>
#include <QLineEdit>
#include <QTextEdit>
#include <QHBoxLayout>
#include <QFormLayout>
#include <QRegularExpressionValidator>

constexpr auto qstr = QString::fromStdString;

osk_dialog_frame::~osk_dialog_frame()
{
	if (m_dialog)
	{
		m_dialog->deleteLater();
	}
}

void osk_dialog_frame::Create(const std::string& title, const std::u16string& message, char16_t* init_text, u32 charlimit, u32 prohibit_flags, u32 panel_flag, u32 /*first_view_panel*/, color /*base_color*/, bool /*dimmer_enabled*/, bool /*intercept_input*/)
{
	state = OskDialogState::Open;

	static const auto& lineEditWidth = []() {return QLabel("This is the very length of the lineedit due to hidpi reasons.").sizeHint().width(); };

	if (m_dialog)
	{
		m_dialog->close();
		delete m_dialog;
	}

	m_dialog = new custom_dialog(false);
	m_dialog->setModal(true);

	// Title
	m_dialog->setWindowTitle(qstr(title));

	// Message
	QLabel* message_label = new QLabel(QString::fromStdU16String(message));

	// Text Input Counter
	const QString input_text = QString::fromStdU16String(std::u16string(init_text));
	QLabel* input_count_label = new QLabel(QString("%1/%2").arg(input_text.length()).arg(charlimit));

	// Button Layout
	QDialogButtonBox* button_box = new QDialogButtonBox(QDialogButtonBox::Ok);

	// Input Layout
	QHBoxLayout* inputLayout = new QHBoxLayout;
	inputLayout->setAlignment(Qt::AlignHCenter);

	// Text Input
	if (prohibit_flags & CELL_OSKDIALOG_NO_RETURN)
	{
		QLineEdit* input = new QLineEdit(m_dialog);
		input->setFixedWidth(lineEditWidth());
		input->setMaxLength(charlimit);
		input->setText(input_text);
		input->setFocus();

		if (panel_flag & CELL_OSKDIALOG_PANELMODE_PASSWORD)
		{
			input->setEchoMode(QLineEdit::Password); // Let's assume that games only use the password mode with single-line edit fields
		}

		if (prohibit_flags & CELL_OSKDIALOG_NO_SPACE)
		{
			input->setValidator(new QRegularExpressionValidator(QRegularExpression("^\\S*$"), this));
		}

		connect(input, &QLineEdit::textChanged, input_count_label, [input_count_label, charlimit, this](const QString& text)
		{
			input_count_label->setText(QString("%1/%2").arg(text.length()).arg(charlimit));
			SetOskText(text);
			// if (on_osk_key_input_entered) on_osk_key_input_entered({}); // Not applicable
		});
		connect(input, &QLineEdit::returnPressed, m_dialog, &QDialog::accept);

		inputLayout->addWidget(input);
	}
	else
	{
		QTextEdit* input = new QTextEdit(m_dialog);
		input->setFixedWidth(lineEditWidth());
		input->setText(input_text);
		input->setFocus();
		input->moveCursor(QTextCursor::End);
		m_text_old = input_text;

		connect(input, &QTextEdit::textChanged, [=, this]()
		{
			QString text = input->toPlainText();

			if (text == m_text_old)
			{
				return;
			}

			QTextCursor cursor = input->textCursor();
			const int cursor_pos_new = cursor.position();
			const int cursor_pos_old = cursor_pos_new + m_text_old.length() - text.length();

			// Reset to old state if character limit was reached
			if (m_text_old.length() >= static_cast<int>(charlimit) && text.length() > static_cast<int>(charlimit))
			{
				input->blockSignals(true);
				input->setPlainText(m_text_old);
				cursor.setPosition(cursor_pos_old);
				input->setTextCursor(cursor);
				input->blockSignals(false);
				return;
			}

			int cursor_pos = cursor.position();

			// Clear text of spaces if necessary
			if (prohibit_flags & CELL_OSKDIALOG_NO_SPACE)
			{
				int trim_len = text.length();
				text.remove(QRegularExpression("\\s+"));
				trim_len -= text.length();
				cursor_pos -= trim_len;
			}

			// Crop if more than one character was pasted and the character limit was exceeded
			text.chop(text.length() - charlimit);

			// Set new text and block signals to prevent infinite loop
			input->blockSignals(true);
			input->setPlainText(text);
			cursor.setPosition(cursor_pos);
			input->setTextCursor(cursor);
			input->blockSignals(false);

			m_text_old = text;

			input_count_label->setText(QString("%1/%2").arg(text.length()).arg(charlimit));
			SetOskText(text);
			// if (on_osk_key_input_entered) on_osk_key_input_entered({}); // Not applicable
		});

		inputLayout->addWidget(input);
	}

	inputLayout->addWidget(input_count_label);

	QFormLayout* layout = new QFormLayout(m_dialog);
	layout->setFormAlignment(Qt::AlignHCenter);
	layout->addRow(message_label);
	layout->addRow(inputLayout);
	layout->addWidget(button_box);
	m_dialog->setLayout(layout);

	// Events
	connect(button_box, &QDialogButtonBox::accepted, m_dialog, &QDialog::accept);

	connect(m_dialog, &QDialog::finished, [this](int result)
	{
		switch (result)
		{
		case QDialog::Accepted:
			on_osk_close(CELL_OSKDIALOG_CLOSE_CONFIRM);
			break;
		case QDialog::Rejected:
			on_osk_close(CELL_OSKDIALOG_CLOSE_CANCEL);
			break;
		default:
			on_osk_close(result);
			break;
		}
	});

	// Fix size
	m_dialog->layout()->setSizeConstraint(QLayout::SetFixedSize);
	m_dialog->show();
}

void osk_dialog_frame::SetOskText(const QString& text)
{
	std::memcpy(osk_text, utils::bless<char16_t>(text.constData()), (text.size() + 1u) * sizeof(char16_t));
}

void osk_dialog_frame::Close(s32 status)
{
	if (m_dialog)
	{
		switch (status)
		{
		case CELL_OSKDIALOG_CLOSE_CONFIRM:
			m_dialog->done(QDialog::Accepted);
			break;
		case CELL_OSKDIALOG_CLOSE_CANCEL:
			m_dialog->done(QDialog::Rejected);
			break;
		default:
			m_dialog->done(status);
			break;
		}
	}
}
