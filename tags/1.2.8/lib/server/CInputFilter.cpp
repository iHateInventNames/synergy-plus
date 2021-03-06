/*
 * synergy -- mouse and keyboard sharing utility
 * Copyright (C) 2005 Chris Schoeneman
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

#include "CInputFilter.h"
#include "CServer.h"
#include "CPrimaryClient.h"
#include "CKeyMap.h"
#include "CEventQueue.h"
#include "CLog.h"
#include "TMethodEventJob.h"
#include <stdlib.h>
#include <string.h>

// -----------------------------------------------------------------------------
// Input Filter Condition Classes
// -----------------------------------------------------------------------------
CInputFilter::CCondition::CCondition()
{
	// do nothing
}

CInputFilter::CCondition::~CCondition()
{
	// do nothing
}

void
CInputFilter::CCondition::enablePrimary(CPrimaryClient*)
{
	// do nothing
}

void
CInputFilter::CCondition::disablePrimary(CPrimaryClient*)
{
	// do nothing
}

CInputFilter::CKeystrokeCondition::CKeystrokeCondition(
		IPlatformScreen::CKeyInfo* info) :
	m_id(0),
	m_key(info->m_key),
	m_mask(info->m_mask)
{
	free(info);
}

CInputFilter::CKeystrokeCondition::CKeystrokeCondition(
		KeyID key, KeyModifierMask mask) :
	m_id(0),
	m_key(key),
	m_mask(mask)
{
	// do nothing
}

CInputFilter::CKeystrokeCondition::~CKeystrokeCondition()
{
	// do nothing
}

CInputFilter::CCondition*
CInputFilter::CKeystrokeCondition::clone() const
{
	return new CKeystrokeCondition(m_key, m_mask);
}

CString
CInputFilter::CKeystrokeCondition::format() const
{
	return CStringUtil::print("keystroke(%s)",
							CKeyMap::formatKey(m_key, m_mask).c_str());
}

CInputFilter::EFilterStatus
CInputFilter::CKeystrokeCondition::match(const CEvent& event)
{
	EFilterStatus status;

	// check for hotkey events
	CEvent::Type type = event.getType();
	if (type == IPrimaryScreen::getHotKeyDownEvent()) {
		status = kActivate;
	}
	else if (type == IPrimaryScreen::getHotKeyUpEvent()) {
		status = kDeactivate;
	}
	else {
		return kNoMatch;
	}

	// check if it's our hotkey
	IPrimaryScreen::CHotKeyInfo* kinfo =
		reinterpret_cast<IPlatformScreen::CHotKeyInfo*>(event.getData());
	if (kinfo->m_id != m_id) {
		return kNoMatch;
	}

	return status;
}

void
CInputFilter::CKeystrokeCondition::enablePrimary(CPrimaryClient* primary)
{
	m_id = primary->registerHotKey(m_key, m_mask);
}

void
CInputFilter::CKeystrokeCondition::disablePrimary(CPrimaryClient* primary)
{
	primary->unregisterHotKey(m_id);
	m_id = 0;
}

CInputFilter::CMouseButtonCondition::CMouseButtonCondition(
		IPlatformScreen::CButtonInfo* info) :
	m_button(info->m_button),
	m_mask(info->m_mask)
{
	free(info);
}

CInputFilter::CMouseButtonCondition::CMouseButtonCondition(
		ButtonID button, KeyModifierMask mask) :
	m_button(button),
	m_mask(mask)
{
	// do nothing
}

CInputFilter::CMouseButtonCondition::~CMouseButtonCondition()
{
	// do nothing
}

CInputFilter::CCondition*
CInputFilter::CMouseButtonCondition::clone() const
{
	return new CMouseButtonCondition(m_button, m_mask);
}

CString
CInputFilter::CMouseButtonCondition::format() const
{
	CString key = CKeyMap::formatKey(kKeyNone, m_mask);
	if (!key.empty()) {
		key += "+";
	}
	return CStringUtil::print("mousebutton(%s%d)", key.c_str(), m_button);
}

CInputFilter::EFilterStatus		
CInputFilter::CMouseButtonCondition::match(const CEvent& event)
{
	EFilterStatus status;

	// check for hotkey events
	CEvent::Type type = event.getType();
	if (type == IPrimaryScreen::getButtonDownEvent()) {
		status = kActivate;
	}
	else if (type == IPrimaryScreen::getButtonUpEvent()) {
		status = kDeactivate;
	}
	else {
		return kNoMatch;
	}

	// check if it's the right button and modifiers
	IPlatformScreen::CButtonInfo* minfo =
		reinterpret_cast<IPlatformScreen::CButtonInfo*>(event.getData());
	if (minfo->m_button != m_button || minfo->m_mask != m_mask) {
		return kNoMatch;
	}

	return status;
}

CInputFilter::CScreenConnectedCondition::CScreenConnectedCondition(
				const CString& screen) :
	m_screen(screen)
{
	// do nothing
}

CInputFilter::CScreenConnectedCondition::~CScreenConnectedCondition()
{
	// do nothing
}

CInputFilter::CCondition*
CInputFilter::CScreenConnectedCondition::clone() const
{
	return new CScreenConnectedCondition(m_screen);
}

CString
CInputFilter::CScreenConnectedCondition::format() const
{
	return CStringUtil::print("connect(%s)", m_screen.c_str());
}

CInputFilter::EFilterStatus
CInputFilter::CScreenConnectedCondition::match(const CEvent& event)
{
	if (event.getType() == CServer::getConnectedEvent()) {
		CServer::CScreenConnectedInfo* info = 
			reinterpret_cast<CServer::CScreenConnectedInfo*>(event.getData());
		if (m_screen == info->m_screen || m_screen.empty()) {
			return kActivate;
		}
	}

	return kNoMatch;
}

// -----------------------------------------------------------------------------
// Input Filter Action Classes
// -----------------------------------------------------------------------------
CInputFilter::CAction::CAction()
{
	// do nothing
}

CInputFilter::CAction::~CAction()
{
	// do nothing
}

CInputFilter::CLockCursorToScreenAction::CLockCursorToScreenAction(Mode mode):
	m_mode(mode)
{
	// do nothing
}

CInputFilter::CAction*
CInputFilter::CLockCursorToScreenAction::clone() const
{
	return new CLockCursorToScreenAction(*this);
}

CString
CInputFilter::CLockCursorToScreenAction::format() const
{
	static const char* s_mode[] = { "off", "on", "toggle" };

	return CStringUtil::print("lockCursorToScreen(%s)", s_mode[m_mode]);
}

void
CInputFilter::CLockCursorToScreenAction::perform(const CEvent& event)
{
	static const CServer::CLockCursorToScreenInfo::State s_state[] = {
		CServer::CLockCursorToScreenInfo::kOff,
		CServer::CLockCursorToScreenInfo::kOn,
		CServer::CLockCursorToScreenInfo::kToggle
	};

	// send event
	CServer::CLockCursorToScreenInfo* info = 
		CServer::CLockCursorToScreenInfo::alloc(s_state[m_mode]);
	EVENTQUEUE->addEvent(CEvent(CServer::getLockCursorToScreenEvent(),
								event.getTarget(), info,
								CEvent::kDeliverImmediately));
}

CInputFilter::CSwitchToScreenAction::CSwitchToScreenAction(
				const CString& screen) :
	m_screen(screen)
{
	// do nothing
}

CInputFilter::CAction*
CInputFilter::CSwitchToScreenAction::clone() const
{
	return new CSwitchToScreenAction(*this);
}

CString
CInputFilter::CSwitchToScreenAction::format() const
{
	return CStringUtil::print("switchToScreen(%s)", m_screen.c_str());
}

void
CInputFilter::CSwitchToScreenAction::perform(const CEvent& event)
{
	// pick screen name.  if m_screen is empty then use the screen from
	// event if it has one.
	CString screen = m_screen;
	if (screen.empty() && event.getType() == CServer::getConnectedEvent()) {
		CServer::CScreenConnectedInfo* info = 
			reinterpret_cast<CServer::CScreenConnectedInfo*>(event.getData());
		screen = info->m_screen;
	}

	// send event
	CServer::CSwitchToScreenInfo* info =
		CServer::CSwitchToScreenInfo::alloc(screen);
	EVENTQUEUE->addEvent(CEvent(CServer::getSwitchToScreenEvent(),
								event.getTarget(), info,
								CEvent::kDeliverImmediately));
}

CInputFilter::CSwitchInDirectionAction::CSwitchInDirectionAction(
				EDirection direction) :
	m_direction(direction)
{
	// do nothing
}

CInputFilter::CAction*
CInputFilter::CSwitchInDirectionAction::clone() const
{
	return new CSwitchInDirectionAction(*this);
}

CString
CInputFilter::CSwitchInDirectionAction::format() const
{
	static const char* s_names[] = {
		"",
		"left",
		"right",
		"up",
		"down"
	};

	return CStringUtil::print("switchInDirection(%s)", s_names[m_direction]);
}

void
CInputFilter::CSwitchInDirectionAction::perform(const CEvent& event)
{
	CServer::CSwitchInDirectionInfo* info =
		CServer::CSwitchInDirectionInfo::alloc(m_direction);
	EVENTQUEUE->addEvent(CEvent(CServer::getSwitchInDirectionEvent(),
								event.getTarget(), info,
								CEvent::kDeliverImmediately));
}

CInputFilter::CKeystrokeAction::CKeystrokeAction(
		IPlatformScreen::CKeyInfo* info, bool press) :
	m_keyInfo(info),
	m_press(press)
{
	// do nothing
}

CInputFilter::CKeystrokeAction::~CKeystrokeAction()
{
	free(m_keyInfo);
}

CInputFilter::CAction*
CInputFilter::CKeystrokeAction::clone() const
{
	IKeyState::CKeyInfo* info = IKeyState::CKeyInfo::alloc(*m_keyInfo);
	return new CKeystrokeAction(info, m_press);
}

CString
CInputFilter::CKeystrokeAction::format() const
{
	const char* type = m_press ? "Down" : "Up";

	if (m_keyInfo->m_screens[0] == '\0') {
		return CStringUtil::print("key%s(%s)", type,
							CKeyMap::formatKey(m_keyInfo->m_key,
								m_keyInfo->m_mask).c_str());
	}
	else if (m_keyInfo->m_screens[0] == '*') {
		return CStringUtil::print("key%s(%s,*)", type,
							CKeyMap::formatKey(m_keyInfo->m_key,
								m_keyInfo->m_mask).c_str());
	}
	else {
		return CStringUtil::print("key%s(%s,%.*s)", type,
							CKeyMap::formatKey(m_keyInfo->m_key,
								m_keyInfo->m_mask).c_str(),
							strlen(m_keyInfo->m_screens + 1) - 1,
							m_keyInfo->m_screens + 1);
	}
}

void
CInputFilter::CKeystrokeAction::perform(const CEvent& event)
{
	CEvent::Type type = m_press ? IPlatformScreen::getKeyDownEvent() :
								IPlatformScreen::getKeyUpEvent();
	EVENTQUEUE->addEvent(CEvent(type, event.getTarget(), m_keyInfo,
								CEvent::kDeliverImmediately |
								CEvent::kDontFreeData));
}

CInputFilter::CMouseButtonAction::CMouseButtonAction(
		IPlatformScreen::CButtonInfo* info, bool press) : 
	m_buttonInfo(info),
	m_press(press)
{
	// do nothing
}

CInputFilter::CMouseButtonAction::~CMouseButtonAction()
{
	free(m_buttonInfo);
}

CInputFilter::CAction*
CInputFilter::CMouseButtonAction::clone() const
{
	IPlatformScreen::CButtonInfo* info =
		IPrimaryScreen::CButtonInfo::alloc(*m_buttonInfo);
	return new CMouseButtonAction(info, m_press);
}

CString
CInputFilter::CMouseButtonAction::format() const
{
	const char* type = m_press ? "Down" : "Up";

	return CStringUtil::print("mouse%s(%s+%d)", type,
							CKeyMap::formatKey(kKeyNone,
								m_buttonInfo->m_mask).c_str(),
							m_buttonInfo->m_button);
}

void
CInputFilter::CMouseButtonAction::perform(const CEvent& event)

{
	// send modifiers
	IPlatformScreen::CKeyInfo* modifierInfo = NULL;
	if (m_buttonInfo->m_mask != 0) {
		KeyID key = m_press ? kKeySetModifiers : kKeyClearModifiers;
		modifierInfo =
			IKeyState::CKeyInfo::alloc(key, m_buttonInfo->m_mask, 0, 1);
		EVENTQUEUE->addEvent(CEvent(IPlatformScreen::getKeyDownEvent(),
								event.getTarget(), modifierInfo,
								CEvent::kDeliverImmediately));
	}

	// send button
	CEvent::Type type = m_press ? IPlatformScreen::getButtonDownEvent() :
								IPlatformScreen::getButtonUpEvent();
	EVENTQUEUE->addEvent(CEvent(type, event.getTarget(), m_buttonInfo,
								CEvent::kDeliverImmediately |
								CEvent::kDontFreeData));
}

//
// CInputFilter::CRule
//

CInputFilter::CRule::CRule() :
	m_condition(NULL)
{
	// do nothing
}

CInputFilter::CRule::CRule(CCondition* adoptedCondition) :
	m_condition(adoptedCondition)
{
	// do nothing
}

CInputFilter::CRule::CRule(const CRule& rule) :
	m_condition(NULL)
{
	copy(rule);
}

CInputFilter::CRule::~CRule()
{
	clear();
}

CInputFilter::CRule&
CInputFilter::CRule::operator=(const CRule& rule)
{
	if (&rule != this) {
		copy(rule);
	}
	return *this;
}

void
CInputFilter::CRule::clear()
{
	delete m_condition;
	for (CActionList::iterator i = m_activateActions.begin();
								i != m_activateActions.end(); ++i) {
		delete *i;
	}
	for (CActionList::iterator i = m_deactivateActions.begin();
								i != m_deactivateActions.end(); ++i) {
		delete *i;
	}

	m_condition = NULL;
	m_activateActions.clear();
	m_deactivateActions.clear();
}

void
CInputFilter::CRule::copy(const CRule& rule)
{
	clear();
	if (rule.m_condition != NULL) {
		m_condition = rule.m_condition->clone();
	}
	for (CActionList::const_iterator i = rule.m_activateActions.begin();
								i != rule.m_activateActions.end(); ++i) {
		m_activateActions.push_back((*i)->clone());
	}
	for (CActionList::const_iterator i = rule.m_deactivateActions.begin();
								i != rule.m_deactivateActions.end(); ++i) {
		m_deactivateActions.push_back((*i)->clone());
	}
}

void
CInputFilter::CRule::adoptAction(CAction* action, bool onActivation)
{
	if (action != NULL) {
		if (onActivation) {
			m_activateActions.push_back(action);
		}
		else {
			m_deactivateActions.push_back(action);
		}
	}
}

void
CInputFilter::CRule::enable(CPrimaryClient* primaryClient)
{
	if (m_condition != NULL) {
		m_condition->enablePrimary(primaryClient);
	}
}

void
CInputFilter::CRule::disable(CPrimaryClient* primaryClient)
{
	if (m_condition != NULL) {
		m_condition->disablePrimary(primaryClient);
	}
}

bool
CInputFilter::CRule::handleEvent(const CEvent& event)
{
	// NULL condition never matches
	if (m_condition == NULL) {
		return false;
	}

	// match
	const CActionList* actions;
	switch (m_condition->match(event)) {
	default:
		// not handled
		return false;

	case kActivate:
		actions = &m_activateActions;
		LOG((CLOG_DEBUG1 "activate actions"));
		break;

	case kDeactivate:
		actions = &m_deactivateActions;
		LOG((CLOG_DEBUG1 "deactivate actions"));
		break;
	}

	// perform actions
	for (CActionList::const_iterator i = actions->begin();
								i != actions->end(); ++i) {
		LOG((CLOG_DEBUG1 "hotkey: %s", (*i)->format().c_str()));
		(*i)->perform(event);
	}

	return true;
}

CString
CInputFilter::CRule::format() const
{
	CString s;
	if (m_condition != NULL) {
		// condition
		s += m_condition->format();
		s += " = ";

		// activate actions
		CActionList::const_iterator i = m_activateActions.begin();
		if (i != m_activateActions.end()) {
			s += (*i)->format();
			while (++i != m_activateActions.end()) {
				s += ", ";
				s += (*i)->format();
			}
		}

		// deactivate actions
		if (!m_deactivateActions.empty()) {
			s += "; ";
			i = m_deactivateActions.begin();
			if (i != m_deactivateActions.end()) {
				s += (*i)->format();
				while (++i != m_deactivateActions.end()) {
					s += ", ";
					s += (*i)->format();
				}
			}
		}
	}
	return s;
}


// -----------------------------------------------------------------------------
// Input Filter Class
// -----------------------------------------------------------------------------
CInputFilter::CInputFilter() :
	m_primaryClient(NULL)
{
	// do nothing
}

CInputFilter::CInputFilter(const CInputFilter& x) :
	m_ruleList(x.m_ruleList),
	m_primaryClient(NULL)
{
	setPrimaryClient(x.m_primaryClient);
}

CInputFilter::~CInputFilter()
{
	setPrimaryClient(NULL);
}

CInputFilter&
CInputFilter::operator=(const CInputFilter& x)
{
	if (&x != this) {
		CPrimaryClient* oldClient = m_primaryClient;
		setPrimaryClient(NULL);

		CRuleList newRules(x.m_ruleList);
		m_ruleList.swap(newRules);

		setPrimaryClient(oldClient);
	}
	return *this;
}

void
CInputFilter::addFilterRule(const CRule& rule)
{
	m_ruleList.push_back(rule);
	if (m_primaryClient != NULL) {
		m_ruleList.back().enable(m_primaryClient);
	}
}

void
CInputFilter::setPrimaryClient(CPrimaryClient* client)
{
	if (m_primaryClient == client) {
		return;
	}

	if (m_primaryClient != NULL) {
		for (CRuleList::iterator rule  = m_ruleList.begin();
								 rule != m_ruleList.end(); ++rule) {
			rule->disable(m_primaryClient);
		}

		EVENTQUEUE->removeHandler(IPlatformScreen::getKeyDownEvent(),
							m_primaryClient->getEventTarget());
		EVENTQUEUE->removeHandler(IPlatformScreen::getKeyUpEvent(),
							m_primaryClient->getEventTarget());
		EVENTQUEUE->removeHandler(IPlatformScreen::getKeyRepeatEvent(),
							m_primaryClient->getEventTarget());
		EVENTQUEUE->removeHandler(IPlatformScreen::getButtonDownEvent(),
							m_primaryClient->getEventTarget());
		EVENTQUEUE->removeHandler(IPlatformScreen::getButtonUpEvent(),
							m_primaryClient->getEventTarget());
		EVENTQUEUE->removeHandler(IPlatformScreen::getHotKeyDownEvent(),
							m_primaryClient->getEventTarget());
		EVENTQUEUE->removeHandler(IPlatformScreen::getHotKeyUpEvent(),
							m_primaryClient->getEventTarget());
		EVENTQUEUE->removeHandler(CServer::getConnectedEvent(),
							m_primaryClient->getEventTarget());
	}

	m_primaryClient = client;

	if (m_primaryClient != NULL) {
		EVENTQUEUE->adoptHandler(IPlatformScreen::getKeyDownEvent(),
							m_primaryClient->getEventTarget(),
							new TMethodEventJob<CInputFilter>(this,
								&CInputFilter::handleEvent));
		EVENTQUEUE->adoptHandler(IPlatformScreen::getKeyUpEvent(),
							m_primaryClient->getEventTarget(),
							new TMethodEventJob<CInputFilter>(this,
								&CInputFilter::handleEvent));
		EVENTQUEUE->adoptHandler(IPlatformScreen::getKeyRepeatEvent(),
							m_primaryClient->getEventTarget(),
							new TMethodEventJob<CInputFilter>(this,
								&CInputFilter::handleEvent));
		EVENTQUEUE->adoptHandler(IPlatformScreen::getButtonDownEvent(),
							m_primaryClient->getEventTarget(),
							new TMethodEventJob<CInputFilter>(this,
								&CInputFilter::handleEvent));
		EVENTQUEUE->adoptHandler(IPlatformScreen::getButtonUpEvent(),
							m_primaryClient->getEventTarget(),
							new TMethodEventJob<CInputFilter>(this,
								&CInputFilter::handleEvent));
		EVENTQUEUE->adoptHandler(IPlatformScreen::getHotKeyDownEvent(),
							m_primaryClient->getEventTarget(),
							new TMethodEventJob<CInputFilter>(this,
								&CInputFilter::handleEvent));
		EVENTQUEUE->adoptHandler(IPlatformScreen::getHotKeyUpEvent(),
							m_primaryClient->getEventTarget(),
							new TMethodEventJob<CInputFilter>(this,
								&CInputFilter::handleEvent));
		EVENTQUEUE->adoptHandler(CServer::getConnectedEvent(),
							m_primaryClient->getEventTarget(),
							new TMethodEventJob<CInputFilter>(this,
								&CInputFilter::handleEvent));

		for (CRuleList::iterator rule  = m_ruleList.begin();
								 rule != m_ruleList.end(); ++rule) {
			rule->enable(m_primaryClient);
		}
	}
}

CString
CInputFilter::format(const CString& linePrefix) const
{
	CString s;
	for (CRuleList::const_iterator i = m_ruleList.begin();
								i != m_ruleList.end(); ++i) {
		s += linePrefix;
		s += i->format();
		s += "\n";
	}
	return s;
}

void
CInputFilter::handleEvent(const CEvent& event, void*)
{
	// copy event and adjust target
	CEvent myEvent(event.getType(), this, event.getData(),
								event.getFlags() | CEvent::kDontFreeData |
								CEvent::kDeliverImmediately);

	// let each rule try to match the event until one does
	for (CRuleList::iterator rule  = m_ruleList.begin();
							 rule != m_ruleList.end(); ++rule) {
		if (rule->handleEvent(myEvent)) {
			// handled
			return;
		}
	}

	// not handled so pass through
	EVENTQUEUE->addEvent(myEvent);
}
