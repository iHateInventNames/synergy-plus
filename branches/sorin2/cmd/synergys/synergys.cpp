/*
 * synergy -- mouse and keyboard sharing utility
 * Copyright (C) 2002 Chris Schoeneman
 * 
 * This package is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * found in the file COPYING that should have accompanied this file.
 * 
 * This package is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include "CClientListener.h"
#include "CClientProxy.h"
#include "CConfig.h"
#include "CPrimaryClient.h"
#include "CServer.h"
#include "CScreen.h"
#include "ProtocolTypes.h"
#include "Version.h"
#include "XScreen.h"
#include "CSocketMultiplexer.h"
#include "CTCPSocketFactory.h"
#include "XSocket.h"
#include "CThread.h"
#include "CEventQueue.h"
#include "CFunctionEventJob.h"
#include "CLog.h"
#include "CString.h"
#include "CStringUtil.h"
#include "LogOutputters.h"
#include "CArch.h"
#include "XArch.h"
#include "CArchConsoleStd.h"
#include "stdfstream.h"

#include <cstring>
#include <iostream>
#include <sstream>

#define DAEMON_RUNNING(running_)
#if WINAPI_MSWINDOWS
#include "CArchMiscWindows.h"
#include "CMSWindowsScreen.h"
#include "CMSWindowsUtil.h"
#include "CMSWindowsServerTaskBarReceiver.h"
#include "resource.h"
#include "CArchDaemonWindows.h"
#include "CMSWindowsServerApp.h"
#include "CMSWindowsAppUtil.h"
#undef DAEMON_RUNNING
#define DAEMON_RUNNING(running_) CArchMiscWindows::daemonRunning(running_)
#elif WINAPI_XWINDOWS
#include "CXWindowsScreen.h"
#include "CXWindowsServerTaskBarReceiver.h"
#include "CXWindowsServerApp.h"
#include "CXWindowsAppUtil.h"
#elif WINAPI_CARBON
#include "COSXScreen.h"
#include "COSXServerTaskBarReceiver.h"
#include "COSXServerApp.h"
#include "COSXAppUtil.h"
#endif

// platform dependent name of a daemon
#if SYSAPI_WIN32
#define DAEMON_NAME "Synergy+ Server"
#define DAEMON_INFO "Shares this computers mouse and keyboard with other computers."
#elif SYSAPI_UNIX
#define DAEMON_NAME "synergys"
#endif

typedef int (*StartupFunc)(int, char**);
static bool loadConfig(const CString& pathname);
static void loadConfig();

#if WINAPI_MSWINDOWS
CMSWindowsServerApp app;
#elif WINAPI_XWINDOWS
CXWindowsServerApp app;
#elif WINAPI_CARBON
COSXServerApp app;
#endif

#define ARG ((CServerApp::CArgs*)&app.args())

//
// platform dependent factories
//

static
CScreen*
createScreen()
{
#if WINAPI_MSWINDOWS
	return new CScreen(new CMSWindowsScreen(true, ARG->m_noHooks));
#elif WINAPI_XWINDOWS
	return new CScreen(new CXWindowsScreen(ARG->m_display, true));
#elif WINAPI_CARBON
	return new CScreen(new COSXScreen(true));
#endif
}

static
CServerTaskBarReceiver*
createTaskBarReceiver(const CBufferedLogOutputter* logBuffer)
{
#if WINAPI_MSWINDOWS
	return new CMSWindowsServerTaskBarReceiver(
							CMSWindowsScreen::getInstance(), logBuffer);
#elif WINAPI_XWINDOWS
	return new CXWindowsServerTaskBarReceiver(logBuffer);
#elif WINAPI_CARBON
	return new COSXServerTaskBarReceiver(logBuffer);
#endif
}


//
// platform independent main
//

enum EServerState {
	kUninitialized,
	kInitializing,
	kInitializingToStart,
	kInitialized,
	kStarting,
	kStarted
};

static EServerState				s_serverState         = kUninitialized;
static CServer*					s_server              = NULL;
static CScreen*					s_serverScreen        = NULL;
static CPrimaryClient*			s_primaryClient       = NULL;
static CClientListener*			s_listener            = NULL;
static CServerTaskBarReceiver*	s_taskBarReceiver     = NULL;
static CEvent::Type				s_reloadConfigEvent   = CEvent::kUnknown;
static CEvent::Type				s_forceReconnectEvent = CEvent::kUnknown;
static CEvent::Type				s_resetServerEvent	  = CEvent::kUnknown;
static bool						s_suspended           = false;
static CEventQueueTimer*		s_timer               = NULL;

CEvent::Type
getReloadConfigEvent()
{
	return CEvent::registerTypeOnce(s_reloadConfigEvent, "reloadConfig");
}

CEvent::Type
getForceReconnectEvent()
{
	return CEvent::registerTypeOnce(s_forceReconnectEvent, "forceReconnect");
}

CEvent::Type
getResetServerEvent()
{
	return CEvent::registerTypeOnce(s_resetServerEvent, "resetServer");
}

static
void
updateStatus()
{
	s_taskBarReceiver->updateStatus(s_server, "");
}

static
void
updateStatus(const CString& msg)
{
	s_taskBarReceiver->updateStatus(s_server, msg);
}

static
void
handleClientConnected(const CEvent&, void* vlistener)
{
	CClientListener* listener = reinterpret_cast<CClientListener*>(vlistener);
	CClientProxy* client = listener->getNextClient();
	if (client != NULL) {
		s_server->adoptClient(client);
		updateStatus();
	}
}

static
CClientListener*
openClientListener(const CNetworkAddress& address)
{
	CClientListener* listen =
		new CClientListener(address, new CTCPSocketFactory, NULL);
	EVENTQUEUE->adoptHandler(CClientListener::getConnectedEvent(), listen,
							new CFunctionEventJob(
								&handleClientConnected, listen));
	return listen;
}

static
void
closeClientListener(CClientListener* listen)
{
	if (listen != NULL) {
		EVENTQUEUE->removeHandler(CClientListener::getConnectedEvent(), listen);
		delete listen;
	}
}

static
void
handleScreenError(const CEvent&, void*)
{
	LOG((CLOG_CRIT "error on screen"));
	EVENTQUEUE->addEvent(CEvent(CEvent::kQuit));
}


static void handleSuspend(const CEvent& event, void*);
static void handleResume(const CEvent& event, void*);

static
CScreen*
openServerScreen()
{
	CScreen* screen = createScreen();
	EVENTQUEUE->adoptHandler(IScreen::getErrorEvent(),
							screen->getEventTarget(),
							new CFunctionEventJob(
								&handleScreenError));
	EVENTQUEUE->adoptHandler(IScreen::getSuspendEvent(),
							screen->getEventTarget(),
							new CFunctionEventJob(
								&handleSuspend));
	EVENTQUEUE->adoptHandler(IScreen::getResumeEvent(),
							screen->getEventTarget(),
							new CFunctionEventJob(
								&handleResume));
	return screen;
}

static
void
closeServerScreen(CScreen* screen)
{
	if (screen != NULL) {
		EVENTQUEUE->removeHandler(IScreen::getErrorEvent(),
							screen->getEventTarget());
		EVENTQUEUE->removeHandler(IScreen::getSuspendEvent(),
							screen->getEventTarget());
		EVENTQUEUE->removeHandler(IScreen::getResumeEvent(),
							screen->getEventTarget());
		delete screen;
	}
}

static
CPrimaryClient*
openPrimaryClient(const CString& name, CScreen* screen)
{
	LOG((CLOG_DEBUG1 "creating primary screen"));
	return new CPrimaryClient(name, screen);
}

static
void
closePrimaryClient(CPrimaryClient* primaryClient)
{
	delete primaryClient;
}

static
void
handleNoClients(const CEvent&, void*)
{
	updateStatus();
}

static
void
handleClientsDisconnected(const CEvent&, void*)
{
	EVENTQUEUE->addEvent(CEvent(CEvent::kQuit));
}

static
CServer*
openServer(const CConfig& config, CPrimaryClient* primaryClient)
{
	CServer* server = new CServer(config, primaryClient);
	
	try {
		EVENTQUEUE->adoptHandler(
			CServer::getDisconnectedEvent(), server,
			new CFunctionEventJob(handleNoClients));

	} catch (std::bad_alloc &ba) {
		delete server;
		throw ba;
	}
	
	return server;
}

static
void
closeServer(CServer* server)
{
	if (server == NULL) {
		return;
	}

	// tell all clients to disconnect
	server->disconnect();

	// wait for clients to disconnect for up to timeout seconds
	double timeout = 3.0;
	CEventQueueTimer* timer = EVENTQUEUE->newOneShotTimer(timeout, NULL);
	EVENTQUEUE->adoptHandler(CEvent::kTimer, timer,
						new CFunctionEventJob(handleClientsDisconnected));
	EVENTQUEUE->adoptHandler(CServer::getDisconnectedEvent(), server,
						new CFunctionEventJob(handleClientsDisconnected));
	CEvent event;
	EVENTQUEUE->getEvent(event);
	while (event.getType() != CEvent::kQuit) {
		EVENTQUEUE->dispatchEvent(event);
		CEvent::deleteData(event);
		EVENTQUEUE->getEvent(event);
	}
	EVENTQUEUE->removeHandler(CEvent::kTimer, timer);
	EVENTQUEUE->deleteTimer(timer);
	EVENTQUEUE->removeHandler(CServer::getDisconnectedEvent(), server);

	// done with server
	delete server;
}

static bool initServer();
static bool startServer();

static
void
stopRetryTimer()
{
	if (s_timer != NULL) {
		EVENTQUEUE->deleteTimer(s_timer);
		EVENTQUEUE->removeHandler(CEvent::kTimer, NULL);
		s_timer = NULL;
	}
}

static
void
retryHandler(const CEvent&, void*)
{
	// discard old timer
	assert(s_timer != NULL);
	stopRetryTimer();

	// try initializing/starting the server again
	switch (s_serverState) {
	case kUninitialized:
	case kInitialized:
	case kStarted:
		assert(0 && "bad internal server state");
		break;

	case kInitializing:
		LOG((CLOG_DEBUG1 "retry server initialization"));
		s_serverState = kUninitialized;
		if (!initServer()) {
			EVENTQUEUE->addEvent(CEvent(CEvent::kQuit));
		}
		break;

	case kInitializingToStart:
		LOG((CLOG_DEBUG1 "retry server initialization"));
		s_serverState = kUninitialized;
		if (!initServer()) {
			EVENTQUEUE->addEvent(CEvent(CEvent::kQuit));
		}
		else if (s_serverState == kInitialized) {
			LOG((CLOG_DEBUG1 "starting server"));
			if (!startServer()) {
				EVENTQUEUE->addEvent(CEvent(CEvent::kQuit));
			}
		}
		break;

	case kStarting:
		LOG((CLOG_DEBUG1 "retry starting server"));
		s_serverState = kInitialized;
		if (!startServer()) {
			EVENTQUEUE->addEvent(CEvent(CEvent::kQuit));
		}
		break;
	}
}

static
bool
initServer()
{
	// skip if already initialized or initializing
	if (s_serverState != kUninitialized) {
		return true;
	}

	double retryTime;
	CScreen* serverScreen         = NULL;
	CPrimaryClient* primaryClient = NULL;
	try {
		CString name    = ARG->m_config->getCanonicalName(ARG->m_name);
		serverScreen    = openServerScreen();
		primaryClient   = openPrimaryClient(name, serverScreen);
		s_serverScreen  = serverScreen;
		s_primaryClient = primaryClient;
		s_serverState   = kInitialized;
		updateStatus();
		return true;
	}
	catch (XScreenUnavailable& e) {
		LOG((CLOG_WARN "cannot open primary screen: %s", e.what()));
		closePrimaryClient(primaryClient);
		closeServerScreen(serverScreen);
		updateStatus(CString("cannot open primary screen: ") + e.what());
		retryTime = e.getRetryTime();
	}
	catch (XScreenOpenFailure& e) {
		LOG((CLOG_CRIT "cannot open primary screen: %s", e.what()));
		closePrimaryClient(primaryClient);
		closeServerScreen(serverScreen);
		return false;
	}
	catch (XBase& e) {
		LOG((CLOG_CRIT "failed to start server: %s", e.what()));
		closePrimaryClient(primaryClient);
		closeServerScreen(serverScreen);
		return false;
	}
	
	if (ARG->m_restartable) {
		// install a timer and handler to retry later
		assert(s_timer == NULL);
		LOG((CLOG_DEBUG "retry in %.0f seconds", retryTime));
		s_timer = EVENTQUEUE->newOneShotTimer(retryTime, NULL);
		EVENTQUEUE->adoptHandler(CEvent::kTimer, s_timer,
							new CFunctionEventJob(&retryHandler, NULL));
		s_serverState = kInitializing;
		return true;
	}
	else {
		// don't try again
		return false;
	}
}

static
bool
startServer()
{
	// skip if already started or starting
	if (s_serverState == kStarting || s_serverState == kStarted) {
		return true;
	}

	// initialize if necessary
	if (s_serverState != kInitialized) {
		if (!initServer()) {
			// hard initialization failure
			return false;
		}
		if (s_serverState == kInitializing) {
			// not ready to start
			s_serverState = kInitializingToStart;
			return true;
		}
		assert(s_serverState == kInitialized);
	}

	double retryTime;
	CClientListener* listener = NULL;
	try {
		listener   = openClientListener(ARG->m_config->getSynergyAddress());
		s_server   = openServer(*ARG->m_config, s_primaryClient);
		s_listener = listener;
		updateStatus();
		LOG((CLOG_NOTE "started server"));
		s_serverState = kStarted;
		return true;
	}
	catch (XSocketAddressInUse& e) {
		LOG((CLOG_WARN "cannot listen for clients: %s", e.what()));
		closeClientListener(listener);
		updateStatus(CString("cannot listen for clients: ") + e.what());
		retryTime = 10.0;
	}
	catch (XBase& e) {
		LOG((CLOG_CRIT "failed to start server: %s", e.what()));
		closeClientListener(listener);
		return false;
	}

	if (ARG->m_restartable) {
		// install a timer and handler to retry later
		assert(s_timer == NULL);
		LOG((CLOG_DEBUG "retry in %.0f seconds", retryTime));
		s_timer = EVENTQUEUE->newOneShotTimer(retryTime, NULL);
		EVENTQUEUE->adoptHandler(CEvent::kTimer, s_timer,
							new CFunctionEventJob(&retryHandler, NULL));
		s_serverState = kStarting;
		return true;
	}
	else {
		// don't try again
		return false;
	}
}

static
void
stopServer()
{
	if (s_serverState == kStarted) {
		closeClientListener(s_listener);
		closeServer(s_server);
		s_server      = NULL;
		s_listener    = NULL;
		s_serverState = kInitialized;
	}
	else if (s_serverState == kStarting) {
		stopRetryTimer();
		s_serverState = kInitialized;
	}
	assert(s_server == NULL);
	assert(s_listener == NULL);
}

static
void
cleanupServer()
{
	stopServer();
	if (s_serverState == kInitialized) {
		closePrimaryClient(s_primaryClient);
		closeServerScreen(s_serverScreen);
		s_primaryClient = NULL;
		s_serverScreen  = NULL;
		s_serverState   = kUninitialized;
	}
	else if (s_serverState == kInitializing ||
			s_serverState == kInitializingToStart) {
		stopRetryTimer();
		s_serverState = kUninitialized;
	}
	assert(s_primaryClient == NULL);
	assert(s_serverScreen == NULL);
	assert(s_serverState == kUninitialized);
}

static
void
handleSuspend(const CEvent&, void*)
{
	if (!s_suspended) {
		LOG((CLOG_INFO "suspend"));
		stopServer();
		s_suspended = true;
	}
}

static
void
handleResume(const CEvent&, void*)
{
	if (s_suspended) {
		LOG((CLOG_INFO "resume"));
		startServer();
		s_suspended = false;
	}
}

static
void
reloadSignalHandler(CArch::ESignal, void*)
{
	EVENTQUEUE->addEvent(CEvent(getReloadConfigEvent(),
							IEventQueue::getSystemTarget()));
}

static
void
reloadConfig(const CEvent&, void*)
{
	LOG((CLOG_DEBUG "reload configuration"));
	if (loadConfig(ARG->m_configFile)) {
		if (s_server != NULL) {
			s_server->setConfig(*ARG->m_config);
		}
		LOG((CLOG_NOTE "reloaded configuration"));
	}
}

static
void
forceReconnect(const CEvent&, void*)
{
	if (s_server != NULL) {
		s_server->disconnect();
	}
}

// simply stops and starts the server in order to try and
// work around issues like the sticky meta keys problem, etc
static
void 
resetServer(const CEvent&, void*)
{
	LOG((CLOG_DEBUG1 "resetting server"));
	stopServer();
	cleanupServer();
	startServer();
}

static
int
mainLoop()
{
	// create socket multiplexer.  this must happen after daemonization
	// on unix because threads evaporate across a fork().
	CSocketMultiplexer multiplexer;

	// create the event queue
	CEventQueue eventQueue;

	// logging to files
	CFileLogOutputter* fileLog = NULL;

	if (ARG->m_logFile != NULL) {
		fileLog = new CFileLogOutputter(ARG->m_logFile);

		CLOG->insert(fileLog);

		LOG((CLOG_DEBUG1 "Logging to file (%s) enabled", ARG->m_logFile));
	}

	// if configuration has no screens then add this system
	// as the default
	if (ARG->m_config->begin() == ARG->m_config->end()) {
		ARG->m_config->addScreen(ARG->m_name);
	}

	// set the contact address, if provided, in the config.
	// otherwise, if the config doesn't have an address, use
	// the default.
	if (ARG->m_synergyAddress->isValid()) {
		ARG->m_config->setSynergyAddress(*ARG->m_synergyAddress);
	}
	else if (!ARG->m_config->getSynergyAddress().isValid()) {
		ARG->m_config->setSynergyAddress(CNetworkAddress(kDefaultPort));
	}

	// canonicalize the primary screen name
	CString primaryName = ARG->m_config->getCanonicalName(ARG->m_name);
	if (primaryName.empty()) {
		LOG((CLOG_CRIT "unknown screen name `%s'", ARG->m_name.c_str()));
		return kExitFailed;
	}

	// start the server.  if this return false then we've failed and
	// we shouldn't retry.
	LOG((CLOG_DEBUG1 "starting server"));
	if (!startServer()) {
		return kExitFailed;
	}

	// handle hangup signal by reloading the server's configuration
	ARCH->setSignalHandler(CArch::kHANGUP, &reloadSignalHandler, NULL);
	EVENTQUEUE->adoptHandler(getReloadConfigEvent(),
							IEventQueue::getSystemTarget(),
							new CFunctionEventJob(&reloadConfig));

	// handle force reconnect event by disconnecting clients.  they'll
	// reconnect automatically.
	EVENTQUEUE->adoptHandler(getForceReconnectEvent(),
							IEventQueue::getSystemTarget(),
							new CFunctionEventJob(&forceReconnect));

	// to work around the sticky meta keys problem, we'll give users
	// the option to reset the state of synergys
	EVENTQUEUE->adoptHandler(getResetServerEvent(),
							IEventQueue::getSystemTarget(),
							new CFunctionEventJob(&resetServer));

	// run event loop.  if startServer() failed we're supposed to retry
	// later.  the timer installed by startServer() will take care of
	// that.
	CEvent event;
	DAEMON_RUNNING(true);
	EVENTQUEUE->getEvent(event);
	while (event.getType() != CEvent::kQuit) {
		EVENTQUEUE->dispatchEvent(event);
		CEvent::deleteData(event);
		EVENTQUEUE->getEvent(event);
	}
	DAEMON_RUNNING(false);

	// close down
	LOG((CLOG_DEBUG1 "stopping server"));
	EVENTQUEUE->removeHandler(getForceReconnectEvent(),
							IEventQueue::getSystemTarget());
	EVENTQUEUE->removeHandler(getReloadConfigEvent(),
							IEventQueue::getSystemTarget());
	cleanupServer();
	updateStatus();
	LOG((CLOG_NOTE "stopped server"));

	if (fileLog) {
		CLOG->remove(fileLog);
		delete fileLog;		
	}

	return kExitSuccess;
}

// used by windows nt (service mode)
static
int
daemonMainLoop(int, const char**)
{
#if SYSAPI_WIN32
	CSystemLogger sysLogger(DAEMON_NAME, false);
#else
	CSystemLogger sysLogger(DAEMON_NAME, true);
#endif
	return mainLoop();
}

// used by unix and win95
static
int
standardStartup(int argc, char** argv)
{

	// parse command line
	app.parse(argc, argv);

	// load configuration
	loadConfig();

	// daemonize if requested
	if (ARG->m_daemon) {
		return ARCH->daemonize(DAEMON_NAME, &daemonMainLoop);
	}
	else {
		return mainLoop();
	}
}

static
int
run(int argc, char** argv, ILogOutputter* outputter, StartupFunc startup)
{
	// general initialization
	ARG->m_synergyAddress = new CNetworkAddress;
	ARG->m_config         = new CConfig;
	ARG->m_pname          = ARCH->getBasename(argv[0]);

	// install caller's output filter
	if (outputter != NULL) {
		CLOG->insert(outputter);
	}

	// save log messages
	// use heap memory because CLog deletes outputters on destruction
	CBufferedLogOutputter* logBuffer = new CBufferedLogOutputter(1000);
	CLOG->insert(logBuffer, true);

	// make the task bar receiver.  the user can control this app
	// through the task bar.
	s_taskBarReceiver = createTaskBarReceiver(logBuffer);

	// run
	int result = startup(argc, argv);

	// done with task bar receiver
	delete s_taskBarReceiver;

	delete ARG->m_config;
	delete ARG->m_synergyAddress;
	return result;
}


//
// command line parsing
//

static
bool
loadConfig(const CString& pathname)
{
	try {
		// load configuration
		LOG((CLOG_DEBUG "opening configuration \"%s\"", pathname.c_str()));
		std::ifstream configStream(pathname.c_str());
		if (!configStream.is_open()) {
			// report failure to open configuration as a debug message
			// since we try several paths and we expect some to be
			// missing.
			LOG((CLOG_DEBUG "cannot open configuration \"%s\"",
								pathname.c_str()));
			return false;
		}
		configStream >> *ARG->m_config;
		LOG((CLOG_DEBUG "configuration read successfully"));
		return true;
	}
	catch (XConfigRead& e) {
		// report error in configuration file
		LOG((CLOG_ERR "cannot read configuration \"%s\": %s",
								pathname.c_str(), e.what()));
	}
	return false;
}

static
void
loadConfig()
{
	bool loaded = false;

	// load the config file, if specified
	if (!ARG->m_configFile.empty()) {
		loaded = loadConfig(ARG->m_configFile);
	}

	// load the default configuration if no explicit file given
	else {
		// get the user's home directory
		CString path = ARCH->getUserDirectory();
		if (!path.empty()) {
			// complete path
			path = ARCH->concatPath(path, USR_CONFIG_NAME);

			// now try loading the user's configuration
			if (loadConfig(path)) {
				loaded            = true;
				ARG->m_configFile = path;
			}
		}
		if (!loaded) {
			// try the system-wide config file
			path = ARCH->getSystemDirectory();
			if (!path.empty()) {
				path = ARCH->concatPath(path, SYS_CONFIG_NAME);
				if (loadConfig(path)) {
					loaded            = true;
					ARG->m_configFile = path;
				}
			}
		}
	}

	if (!loaded) {
		LOG((CLOG_PRINT "%s: no configuration available", ARG->m_pname));
		app.m_bye(kExitConfig);
	}
}


//
// platform dependent entry points
//

#if SYSAPI_WIN32

static bool				s_hasImportantLogMessages = false;

//
// CMessageBoxOutputter
//
// This class writes severe log messages to a message box
//

class CMessageBoxOutputter : public ILogOutputter {
public:
	CMessageBoxOutputter() { }
	virtual ~CMessageBoxOutputter() { }

	// ILogOutputter overrides
	virtual void		open(const char*) { }
	virtual void		close() { }
	virtual void		show(bool) { }
	virtual bool		write(ELevel level, const char* message);
};

bool
CMessageBoxOutputter::write(ELevel level, const char* message)
{
	// note any important messages the user may need to know about
	if (level <= CLog::kWARNING) {
		s_hasImportantLogMessages = true;
	}

	// FATAL and PRINT messages get a dialog box if not running as
	// backend.  if we're running as a backend the user will have
	// a chance to see the messages when we exit.
	if (!ARG->m_backend && level <= CLog::kFATAL) {
		MessageBox(NULL, message, ARG->m_pname, MB_OK | MB_ICONWARNING);
		return false;
	}
	else {
		return true;
	}
}

static
void
byeThrow(int x)
{
	CArchMiscWindows::daemonFailed(x);
}

static
int
daemonNTMainLoop(int argc, const char** argv)
{
	app.parse(argc, argv);
	ARG->m_backend = false;
	loadConfig();
	return CArchMiscWindows::runDaemon(mainLoop);
}

static
int
daemonNTStartup(int, char**)
{
	CSystemLogger sysLogger(DAEMON_NAME, false);
	app.m_bye = &byeThrow;
	return ARCH->daemonize(DAEMON_NAME, &daemonNTMainLoop);
}

// used by windows nt (foreground)
static
int
foregroundStartup(int argc, char** argv)
{
	// parse command line
	app.parse(argc, argv);

	// load configuration
	loadConfig();

	// never daemonize
	return mainLoop();
}

static
void
showError(HINSTANCE instance, const char* title, UINT id, const char* arg)
{
	CString fmt = CMSWindowsUtil::getString(instance, id);
	CString msg = CStringUtil::format(fmt.c_str(), arg);
	LOG((CLOG_ERR "%s", msg.c_str()));
	app.m_bye(kExitFailed);
}

int main(int argc, char** argv) {
	
	app.m_daemonName = DAEMON_NAME;
	app.m_daemonInfo = DAEMON_INFO;
	app.util().m_instance = GetModuleHandle(NULL);

	int code;
	if (app.util().m_instance) {
		code = WinMain(app.util().m_instance, NULL, GetCommandLine(), SW_SHOWNORMAL);
	} else {
		code = kExitFailed;
	}
	
	if (code != kExitSuccess) {
		app.m_bye(code);
	}

	return kExitSuccess;
}

int WINAPI
WinMain(HINSTANCE instance, HINSTANCE, LPSTR, int)
{
	// instantiate before the try-catch block, so they are destroyed after the
	// exception handling (we may need to log exception message). these should
	// not throw exceptions at this stage, so this code should be pretty safe.
	CArch arch(instance);
	CLOG;

	try {
		CArchMiscWindows::setIcons((HICON)LoadImage(instance,
									MAKEINTRESOURCE(IDI_SYNERGY),
									IMAGE_ICON,
									32, 32, LR_SHARED),
									(HICON)LoadImage(instance,
									MAKEINTRESOURCE(IDI_SYNERGY),
									IMAGE_ICON,
									16, 16, LR_SHARED));

		CMSWindowsScreen::init(instance);
		CThread::getCurrentThread().setPriority(-14);
		
		StartupFunc startup;
		if (!CArchMiscWindows::isWindows95Family()) {

			// WARNING: this may break backwards computability!
			// previously, we were assuming that the process is launched from the
			// service host when no arguments were passed. if we wanted to launch
			// from console or debugger, we had to remember to pass -f which was
			// always the first pitfall for new committers. now, we are able to
			// check using the new `wasLaunchedAsService` function, which is a
			// more elegant solution.
			if (CArchMiscWindows::wasLaunchedAsService()) {
				startup = &daemonNTStartup;
			} else {
				startup = &foregroundStartup;
				ARG->m_daemon = false;
			}
		} else {
			startup = &standardStartup;
		}

		// previously we'd send PRINT and FATAL output to a message box, but now
		// that we're using an MS console window for Windows, there's no need really
		//int result = run(__argc, __argv, new CMessageBoxOutputter, startup);
		int result = run(__argc, __argv, NULL, startup);

		delete CLOG;
		return result;
	}
	catch (XBase& e) {
		showError(instance, __argv[0], IDS_UNCAUGHT_EXCEPTION, e.what());
	}
	catch (XArch& e) {
		showError(instance, __argv[0], IDS_INIT_FAILED, e.what().c_str());
	}
	catch (std::exception& e) {
		showError(instance, __argv[0], IDS_UNCAUGHT_EXCEPTION, e.what());
	}
	catch (...) {
		showError(instance, __argv[0], IDS_UNCAUGHT_EXCEPTION, "<unknown exception>");
	}
	return kExitFailed;
}

#elif SYSAPI_UNIX

int
main(int argc, char** argv)
{
	try {
		int result;
		CArch arch;
		CLOG;
		result = run(argc, argv, NULL, &standardStartup);
		delete CLOG;
		return result;
	}
	catch (XBase& e) {
		LOG((CLOG_CRIT "Uncaught exception: %s\n", e.what()));
		throw;
	}
	catch (XArch& e) {
		LOG((CLOG_CRIT "Initialization failed: %s" BYE, e.what().c_str()));
		return kExitFailed;
	}
	catch (std::exception& e) {
		LOG((CLOG_CRIT "Uncaught exception: %s\n", e.what()));
		throw;
	}
	catch (...) {
		LOG((CLOG_CRIT "Uncaught exception: <unknown exception>\n"));
		throw;
	}
}

#else

#error no main() for platform

#endif