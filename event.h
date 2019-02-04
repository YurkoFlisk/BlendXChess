//============================================================
// event.h
// Event system for Console interface of BlendXChess
//============================================================

#pragma once
#ifndef _EVENT_H
#define _EVENT_H
#include <string>
#include <queue>
#include <thread>
#include <variant>
#include "engine.h"

enum class EventSource
{
	CONSOLE, ENGINE
};

//============================================================
// Struct for holding event info
//============================================================

struct Event
{
	using EventInfo = std::variant<std::string, SearchEvent>;
	Event(void) = default;
	Event(EventSource, const EventInfo&);
	EventSource source;
	EventInfo eventInfo;
};

//============================================================
// Class for handling various events (console input and engine events)
//============================================================

class EventLoop
{
public:
	// Constructor
	EventLoop(void);
	// Get function for engine response
	EngineProcesser getEngineProcesser(void);
	// Wait for next event (or immediately return one if queued)
	Event next(void);
private:
	// Pool for reading and queuing console input
	void consoleReader(void);
	// Engine event processer 
	void engineProcesser(const SearchEvent&);
	// Thread for acquiring console input
	std::thread consoleInputThread;
	// Mutex for events queue
	std::mutex eventsMutex;
	// The event queue
	std::queue<Event> events;
};

#endif