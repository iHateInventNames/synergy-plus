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

#ifndef CARCHDAEMONNONE_H
#define CARCHDAEMONNONE_H

#include "IArchDaemon.h"

#define ARCH_DAEMON CArchDaemonNone

//! Dummy implementation of IArchDaemon
/*!
This class implements IArchDaemon for a platform that does not have
daemons.  The install and uninstall functions do nothing, the query
functions return false, and \c daemonize() simply calls the passed
function and returns its result.
*/
class CArchDaemonNone : public IArchDaemon {
public:
	CArchDaemonNone();
	virtual ~CArchDaemonNone();

	// IArchDaemon overrides
	virtual void		installDaemon(const char* name,
							const char* description,
							const char* pathname,
							const char* commandLine,
							bool allUsers);
	virtual void		uninstallDaemon(const char* name, bool allUsers);
	virtual int			daemonize(const char* name, DaemonFunc func);
	virtual bool		canInstallDaemon(const char* name, bool allUsers);
	virtual bool		isDaemonInstalled(const char* name, bool allUsers);
};

#endif
