﻿// Qt5.10+ frontend implementation for rpcs3. Known to work on Windows, Linux, Mac
// by Sacha Refshauge, Megamouse and flash-fire

#include <QApplication>
#include <QCommandLineParser>
#include <QFileInfo>
#include <QTimer>
#include <QObject>
#include <QMessageBox>
#include <QTextDocument>
#include <QStyleFactory>

#include "rpcs3qt/gui_application.h"

#include "headless_application.h"
#include "Utilities/sema.h"
#ifdef _WIN32
#include <windows.h>
#include "Utilities/dynamic_library.h"
DYNAMIC_IMPORT("ntdll.dll", NtQueryTimerResolution, NTSTATUS(PULONG MinimumResolution, PULONG MaximumResolution, PULONG CurrentResolution));
DYNAMIC_IMPORT("ntdll.dll", NtSetTimerResolution, NTSTATUS(ULONG DesiredResolution, BOOLEAN SetResolution, PULONG CurrentResolution));
#endif

#ifdef __linux__
#include <sys/time.h>
#include <sys/resource.h>
#endif

#ifdef __APPLE__
#include <dispatch/dispatch.h>
#endif

#include "rpcs3_version.h"

inline std::string sstr(const QString& _in) { return _in.toStdString(); }

template <typename... Args>
inline auto tr(Args&&... args)
{
	return QObject::tr(std::forward<Args>(args)...);
}

namespace logs
{
	void set_init();
}

static semaphore<> s_init{0};
static semaphore<> s_qt_init{0};
static semaphore<> s_qt_mutex{};

[[noreturn]] extern void report_fatal_error(const std::string& text)
{
	s_qt_mutex.lock();

	if (!s_qt_init.try_lock())
	{
		s_init.lock();
		static int argc = 1;
		static char arg1[] = {"ERROR"};
		static char* argv[] = {arg1};
		static QApplication app0{argc, argv};
	}

	auto show_report = [](const std::string& text)
	{
		QMessageBox msg;
		msg.setWindowTitle(tr("RPCS3: Fatal Error"));
		msg.setIcon(QMessageBox::Critical);
		msg.setTextFormat(Qt::RichText);
		msg.setText(QString(R"(
			<p style="white-space: nowrap;">
				%1<br>
				%2<br>
				<a href='https://github.com/RPCS3/rpcs3/wiki/How-to-ask-for-Support'>https://github.com/RPCS3/rpcs3/wiki/How-to-ask-for-Support</a><br>
				%3<br>
			</p>
			)")
			.arg(Qt::convertFromPlainText(QString::fromStdString(text)))
			.arg(tr("HOW TO REPORT ERRORS:"))
			.arg(tr("Please, don't send incorrect reports. Thanks for understanding.")));
		msg.layout()->setSizeConstraint(QLayout::SetFixedSize);
		msg.exec();
	};

#ifdef __APPLE__
	// Cocoa access is not allowed outside of the main thread
	if (!pthread_main_np())
	{
		dispatch_sync(dispatch_get_main_queue(), ^ { show_report(text); });
	}
	else
#endif
	{
		show_report(text);
	}

	std::abort();
}

const char* arg_headless   = "headless";
const char* arg_no_gui     = "no-gui";
const char* arg_high_dpi   = "hidpi";
const char* arg_styles     = "styles";
const char* arg_style      = "style";
const char* arg_stylesheet = "stylesheet";

int find_arg(std::string arg, int& argc, char* argv[])
{
	arg = "--" + arg;
	for (int i = 1; i < argc; ++i)
		if (!strcmp(arg.c_str(), argv[i]))
			return i;
	return 0;
}

QCoreApplication* createApplication(int& argc, char* argv[])
{
	if (find_arg(arg_headless, argc, argv))
		return new headless_application(argc, argv);

	bool use_high_dpi = true;

	const auto i_hdpi = find_arg(arg_high_dpi, argc, argv);
	if (i_hdpi)
	{
		const std::string cmp_str = "0";
		const auto i_hdpi_2 = (argc > (i_hdpi + 1)) ? (i_hdpi + 1) : 0;
		const auto high_dpi_setting = (i_hdpi_2 && !strcmp(cmp_str.c_str(), argv[i_hdpi_2])) ? "0" : "1";
		
		// Set QT_AUTO_SCREEN_SCALE_FACTOR from environment. Defaults to cli argument, which defaults to 1.
		use_high_dpi = "1" == qEnvironmentVariable("QT_AUTO_SCREEN_SCALE_FACTOR", high_dpi_setting);
	}

	// AA_EnableHighDpiScaling has to be set before creating a QApplication
	QApplication::setAttribute(use_high_dpi ? Qt::AA_EnableHighDpiScaling : Qt::AA_DisableHighDpiScaling);

	return new gui_application(argc, argv);
}

int main(int argc, char** argv)
{
	logs::set_init();

#ifdef __linux__
	struct ::rlimit rlim;
	rlim.rlim_cur = 4096;
	rlim.rlim_max = 4096;
	if (::setrlimit(RLIMIT_NOFILE, &rlim) != 0)
		std::fprintf(stderr, "Failed to set max open file limit (4096).");
	// Work around crash on startup on KDE: https://bugs.kde.org/show_bug.cgi?id=401637
	setenv( "KDE_DEBUG", "1", 0 );
#endif

	s_init.unlock();
	s_qt_mutex.lock();

	// The constructor of QApplication eats the --style and --stylesheet arguments.
	// By checking for stylesheet().isEmpty() we could implicitly know if a stylesheet was passed,
	// but I haven't found an implicit way to check for style yet, so we naively check them both here for now.
	const bool use_cli_style = find_arg(arg_style, argc, argv) || find_arg(arg_stylesheet, argc, argv);

	QScopedPointer<QCoreApplication> app(createApplication(argc, argv));
	app->setApplicationVersion(qstr(rpcs3::version.to_string()));
	app->setApplicationName("RPCS3");

	// Command line args
	QCommandLineParser parser;
	parser.setApplicationDescription("Welcome to RPCS3 command line.");
	parser.addPositionalArgument("(S)ELF", "Path for directly executing a (S)ELF");
	parser.addPositionalArgument("[Args...]", "Optional args for the executable");

	const QCommandLineOption helpOption    = parser.addHelpOption();
	const QCommandLineOption versionOption = parser.addVersionOption();
	parser.addOption(QCommandLineOption(arg_headless, "Run RPCS3 in headless mode."));
	parser.addOption(QCommandLineOption(arg_no_gui, "Run RPCS3 without its GUI."));
	parser.addOption(QCommandLineOption(arg_high_dpi, "Enables Qt High Dpi Scaling.", "enabled", "1"));
	parser.addOption(QCommandLineOption(arg_styles, "Lists the available styles."));
	parser.addOption(QCommandLineOption(arg_style, "Loads a custom style.", "style", ""));
	parser.addOption(QCommandLineOption(arg_stylesheet, "Loads a custom stylesheet.", "path", ""));
	parser.process(app->arguments());

	// Don't start up the full rpcs3 gui if we just want the version or help.
	if (parser.isSet(versionOption) || parser.isSet(helpOption))
		return 0;

	if (parser.isSet(arg_styles))
	{
#ifdef _WIN32
		if (AttachConsole(ATTACH_PARENT_PROCESS) || AllocConsole())
			const auto con_out = freopen("CONOUT$", "w", stdout);
#endif
		for (const auto& style : QStyleFactory::keys())
			std::cout << "\n" << style.toStdString();

		return 0;
	}

	if (auto gui_app = qobject_cast<gui_application*>(app.data()))
	{
		gui_app->setAttribute(Qt::AA_UseHighDpiPixmaps);
		gui_app->setAttribute(Qt::AA_DisableWindowContextHelpButton);
		gui_app->setAttribute(Qt::AA_DontCheckOpenGLContextThreadAffinity);

		gui_app->SetShowGui(!parser.isSet(arg_no_gui));
		gui_app->SetUseCliStyle(use_cli_style);
		gui_app->Init();
	}
	else if (auto headless_app = qobject_cast<headless_application*>(app.data()))
	{
		headless_app->Init();
	}

#ifdef _WIN32
	// Set 0.5 msec timer resolution for best performance
	// - As QT5 timers (QTimer) sets the timer resolution to 1 msec, override it here.
	// - Don't bother "unsetting" the timer resolution after the emulator stops as QT5 will still require the timer resolution to be set to 1 msec.
	ULONG min_res, max_res, orig_res, new_res;
	if (NtQueryTimerResolution(&min_res, &max_res, &orig_res) == 0)
	{
		NtSetTimerResolution(max_res, TRUE, &new_res);
	}
#endif

	QStringList args = parser.positionalArguments();

	if (args.length() > 0)
	{
		// Propagate command line arguments
		std::vector<std::string> argv;

		if (args.length() > 1)
		{
			argv.emplace_back();

			for (int i = 1; i < args.length(); i++)
			{
				argv.emplace_back(args[i].toStdString());
			}
		}

		// Ugly workaround
		QTimer::singleShot(2, [path = sstr(QFileInfo(args.at(0)).canonicalFilePath()), argv = std::move(argv)]() mutable
		{
			Emu.argv = std::move(argv);
			Emu.SetForceBoot(true);
			Emu.BootGame(path, "", true);
		});
	}

	s_qt_init.unlock();
	s_qt_mutex.unlock();

	// run event loop (maybe only needed for the gui application)
	return app->exec();
}
