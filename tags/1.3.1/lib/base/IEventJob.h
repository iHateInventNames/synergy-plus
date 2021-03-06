/*
 * synergy -- mouse and keyboard sharing utility
 * Copyright (C) 2004 Chris Schoeneman
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

#ifndef IEVENTJOB_H
#define IEVENTJOB_H

#include "IInterface.h"

class CEvent;

//! Event handler interface
/*!
An event job is an interface for executing a event handler.
*/
class IEventJob : public IInterface {
public:
	//! Run the job
	virtual void		run(const CEvent&) = 0;
};

#endif
