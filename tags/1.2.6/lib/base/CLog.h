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

#ifndef CLOG_H
#define CLOG_H

#include "common.h"
#include "IArchMultithread.h"
#include "stdlist.h"
#include <stdarg.h>

#define CLOG (CLog::getInstance())

class ILogOutputter;

//! Logging facility
/*!
The logging class;  all console output should go through this class.
It supports multithread safe operation, several message priority levels,
filtering by priority, and output redirection.  The macros LOG() and
LOGC() provide convenient access.
*/
class CLog {
public:
	//! Log levels
	/*!
	The logging priority levels in order of highest to lowest priority.
	*/
	enum ELevel {
		kFATAL,			//!< For fatal errors
		kERROR,			//!< For serious errors
		kWARNING,		//!< For minor errors and warnings
		kNOTE,			//!< For messages about notable events
		kINFO,			//!< For informational messages
		kDEBUG,			//!< For important debugging messages
		kDEBUG1,		//!< For more detailed debugging messages
		kDEBUG2			//!< For even more detailed debugging messages
	};

	~CLog();

	//! @name manipulators
	//@{

	//! Add an outputter to the head of the list
	/*!
	Inserts an outputter to the head of the outputter list.  When the
	logger writes a message, it goes to the outputter at the head of
	the outputter list.  If that outputter's \c write() method returns
	true then it also goes to the next outputter, as so on until an
	outputter returns false or there are no more outputters.  Outputters
	still in the outputter list when the log is destroyed will be
	deleted.  If \c alwaysAtHead is true then the outputter is always
	called before all outputters with \c alwaysAtHead false and the
	return value of the outputter is ignored.

	By default, the logger has one outputter installed which writes to
	the console.
	*/
	void				insert(ILogOutputter* adopted,
							   bool alwaysAtHead = false);

	//! Remove an outputter from the list
	/*!
	Removes the first occurrence of the given outputter from the
	outputter list.  It does nothing if the outputter is not in the
	list.  The outputter is not deleted.
	*/
	void				remove(ILogOutputter* orphaned);

	//! Remove the outputter from the head of the list
	/*!
	Removes and deletes the outputter at the head of the outputter list.
	This does nothing if the outputter list is empty.  Only removes
	outputters that were inserted with the matching \c alwaysAtHead.
	*/
	void				pop_front(bool alwaysAtHead = false);

	//! Set the minimum priority filter.
	/*!
	Set the filter.  Messages below this priority are discarded.
	The default priority is 4 (INFO) (unless built without NDEBUG
	in which case it's 5 (DEBUG)).   setFilter(const char*) returns
	true if the priority \c name was recognized;  if \c name is NULL
	then it simply returns true.
	*/
	bool				setFilter(const char* name);
	void				setFilter(int);

	//@}
	//! @name accessors
	//@{

	//! Print a log message
	/*!
	Print a log message using the printf-like \c format and arguments.
	*/
	void				print(const char* format, ...) const;

	//! Print a log message
	/*!
	Print a log message using the printf-like \c format and arguments
	preceded by the filename and line number.
	*/
	void				printt(const char* file, int line,
							const char* format, ...) const;

	//! Get the minimum priority level.
	int					getFilter() const;

	//! Get the singleton instance of the log
	static CLog*		getInstance();

	//@}

private:
	CLog();

	void				output(int priority, char* msg) const;

private:
	typedef std::list<ILogOutputter*> COutputterList;

	static CLog*		s_log;

	CArchMutex			m_mutex;
	COutputterList		m_outputters;
	COutputterList		m_alwaysOutputters;
	int					m_maxNewlineLength;
	int					m_maxPriority;
};

/*!
\def LOG(arg)
Write to the log.  Because macros cannot accept variable arguments, this
should be invoked like so:
\code
LOG((CLOG_XXX "%d and %d are %s", x, y, x == y ? "equal" : "not equal"));
\endcode
In particular, notice the double open and close parentheses.  Also note
that there is no comma after the \c CLOG_XXX.  The \c XXX should be
replaced by one of enumerants in \c CLog::ELevel without the leading
\c k.  For example, \c CLOG_INFO.  The special \c CLOG_PRINT level will
not be filtered and is never prefixed by the filename and line number.

If \c NOLOGGING is defined during the build then this macro expands to
nothing.  If \c NDEBUG is defined during the build then it expands to a
call to CLog::print.  Otherwise it expands to a call to CLog::printt,
which includes the filename and line number.
*/

/*!
\def LOGC(expr, arg)
Write to the log if and only if expr is true.  Because macros cannot accept
variable arguments, this should be invoked like so:
\code
LOGC(x == y, (CLOG_XXX "%d and %d are equal", x, y));
\endcode
In particular, notice the parentheses around everything after the boolean
expression.    Also note that there is no comma after the \c CLOG_XXX.
The \c XXX should be replaced by one of enumerants in \c CLog::ELevel
without the leading \c k.  For example, \c CLOG_INFO.  The special
\c CLOG_PRINT level will not be filtered and is never prefixed by the
filename and line number.

If \c NOLOGGING is defined during the build then this macro expands to
nothing.  If \c NDEBUG is defined during the build then it expands to a
call to CLog::print.  Otherwise it expands to a call to CLog::printt,
which includes the filename and line number.
*/

#if defined(NOLOGGING)
#define LOG(_a1)
#define LOGC(_a1, _a2)
#define CLOG_TRACE
#elif defined(NDEBUG)
#define LOG(_a1)		CLOG->print _a1
#define LOGC(_a1, _a2)	if (_a1) CLOG->print _a2
#define CLOG_TRACE
#else
#define LOG(_a1)		CLOG->printt _a1
#define LOGC(_a1, _a2)	if (_a1) CLOG->printt _a2
#define CLOG_TRACE		__FILE__, __LINE__,
#endif

#define CLOG_PRINT		CLOG_TRACE "%z\057"
#define CLOG_CRIT		CLOG_TRACE "%z\060"
#define CLOG_ERR		CLOG_TRACE "%z\061"
#define CLOG_WARN		CLOG_TRACE "%z\062"
#define CLOG_NOTE		CLOG_TRACE "%z\063"
#define CLOG_INFO		CLOG_TRACE "%z\064"
#define CLOG_DEBUG		CLOG_TRACE "%z\065"
#define CLOG_DEBUG1		CLOG_TRACE "%z\066"
#define CLOG_DEBUG2		CLOG_TRACE "%z\067"

#endif
