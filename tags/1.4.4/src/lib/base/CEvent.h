/*
 * synergy -- mouse and keyboard sharing utility
 * Copyright (C) 2004 Chris Schoeneman, Nick Bolton, Sorin Sbarnea
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

#ifndef CEVENT_H
#define CEVENT_H

#include "BasicTypes.h"
#include "stdmap.h"

//! Event
/*!
A \c CEvent holds an event type and a pointer to event data.
*/
class CEvent {
public:
	typedef UInt32 Type;
	enum {
		kUnknown,	//!< The event type is unknown
		kQuit,		//!< The quit event
		kSystem,	//!< The data points to a system event type
		kTimer,		//!< The data points to timer info
		kLast		//!< Must be last
	};

	typedef UInt32 Flags;
	enum {
		kNone				= 0x00,	//!< No flags
		kDeliverImmediately	= 0x01,	//!< Dispatch and free event immediately
		kDontFreeData		= 0x02	//!< Don't free data in deleteData
	};

	CEvent();

	//! Create \c CEvent with data
	/*!
	The \p type must have been registered using \c registerType().
	The \p data must be POD (plain old data) allocated by malloc(),
	which means it cannot have a constructor, destructor or be
	composed of any types that do.  \p target is the intended
	recipient of the event.  \p flags is any combination of \c Flags.
	*/
	CEvent(Type type, void* target = NULL, void* data = NULL,
							 UInt32 flags = kNone);

	//! @name manipulators
	//@{

	//! Release event data
	/*!
	Deletes event data for the given event (using free()).
	*/
	static void			deleteData(const CEvent&);

	//@}
	//! @name accessors
	//@{

	//! Get event type
	/*!
	Returns the event type.
	*/
	Type				getType() const;

	//! Get the event target
	/*!
	Returns the event target.
	*/
	void*				getTarget() const;

	//! Get the event data
	/*!
	Returns the event data.
	*/
	void*				getData() const;

	//! Get event flags
	/*!
	Returns the event flags.
	*/
	Flags				getFlags() const;
	
	//@}

private:
	Type				m_type;
	void*				m_target;
	void*				m_data;
	Flags				m_flags;
};

#endif
