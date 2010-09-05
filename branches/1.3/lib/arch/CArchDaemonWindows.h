/*
 * synergy-plus -- mouse and keyboard sharing utility
 * Copyright (C) 2009 The Synergy+ Project
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
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef CARCHDAEMONWINDOWS_H
#define CARCHDAEMONWINDOWS_H

#define WIN32_LEAN_AND_MEAN

#include "IArchDaemon.h"
#include "IArchMultithread.h"
#include "stdstring.h"
#include <windows.h>
#include <tchar.h>

#define ARCH_DAEMON CArchDaemonWindows

//! Win32 implementation of IArchDaemon
class CArchDaemonWindows : public IArchDaemon {
public:
	typedef int			(*RunFunc)(void);

	CArchDaemonWindows();
	virtual ~CArchDaemonWindows();

	//! Run the daemon
	/*!
	When the client calls \c daemonize(), the \c DaemonFunc should call this
	function after initialization and argument parsing to perform the
	daemon processing.  The \c runFunc should perform the daemon's
	main loop, calling \c daemonRunning(true) when it enters the main loop
	(i.e. after initialization) and \c daemonRunning(false) when it leaves
	the main loop.  The \c runFunc is called in a new thread and when the
	daemon must exit the main loop due to some external control the
	getDaemonQuitMessage() is posted to the thread.  This function returns
	what \c runFunc returns.  \c runFunc should call \c daemonFailed() if
	the daemon fails.
	*/
	static int			runDaemon(RunFunc runFunc);

	//! Indicate daemon is in main loop
	/*!
	The \c runFunc passed to \c runDaemon() should call this function
	to indicate when it has entered (\c running is \c true) or exited
	(\c running is \c false) the main loop.
	*/
	static void			daemonRunning(bool running);

	//! Indicate failure of running daemon
	/*!
	The \c runFunc passed to \c runDaemon() should call this function
	to indicate failure.  \c result is returned by \c daemonize().
	*/
	static void			daemonFailed(int result);

	//! Get daemon quit message
	/*!
	The windows NT daemon tells daemon thread to exit by posting this
	message to it.  The thread must, of course, have a message queue
	for this to work.
	*/
	static UINT			getDaemonQuitMessage();

	// IArchDaemon overrides
	virtual void		installDaemon(const char* name,
							const char* description,
							const char* pathname,
							const char* commandLine,
							const char* dependencies,
							bool allUsers);
	virtual void		uninstallDaemon(const char* name, bool allUsers);
	virtual int			daemonize(const char* name, DaemonFunc func);
	virtual bool		canInstallDaemon(const char* name, bool allUsers);
	virtual bool		isDaemonInstalled(const char* name, bool allUsers);

private:
	static HKEY			openNTServicesKey();
	static HKEY			open95ServicesKey();
	static HKEY			openUserStartupKey();

	int					doRunDaemon(RunFunc runFunc);
	void				doDaemonRunning(bool running);
	UINT				doGetDaemonQuitMessage();

	static void			setStatus(DWORD state);
	static void			setStatus(DWORD state, DWORD step, DWORD waitHint);
	static void			setStatusError(DWORD error);

	static bool			isRunState(DWORD state);

	void				serviceMain(DWORD, LPTSTR*);
	static void WINAPI	serviceMainEntry(DWORD, LPTSTR*);

	void				serviceHandler(DWORD ctrl);
	static void WINAPI	serviceHandlerEntry(DWORD ctrl);

private:
	class XArchDaemonRunFailed {
	public:
		XArchDaemonRunFailed(int result) : m_result(result) { }

	public:
		int				m_result;
	};

private:
	static CArchDaemonWindows*	s_daemon;

	CArchMutex			m_serviceMutex;
	CArchCond			m_serviceCondVar;
	DWORD				m_serviceState;
	bool				m_serviceHandlerWaiting;
	bool				m_serviceRunning;

	DWORD				m_daemonThreadID;
	DaemonFunc			m_daemonFunc;
	int					m_daemonResult;

	SERVICE_STATUS_HANDLE m_statusHandle;

	UINT				m_quitMessage;
};

#endif