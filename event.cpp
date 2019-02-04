//============================================================
// event.cpp
// Event system for Console interface of BlendXChess
//============================================================

#include <iostream>
#include <chrono>
#include "event.h"
#include "engine.h"

//============================================================
// Constructor
//============================================================
Event::Event(EventSource src, const EventInfo& ei)
	: source(src), eventInfo(ei)
{}

//============================================================
// Constructor
//============================================================
EventLoop::EventLoop(void)
	: consoleInputThread(std::thread(&EventLoop::consoleReader, this))
{}

//============================================================
// Get function for engine response
//============================================================
EngineProcesser EventLoop::getEngineProcesser(void)
{
	return [this](const SearchEvent& se) {this->engineProcesser(se); };
}

//============================================================
// Wait for next event (or immediately return one if queued)
//============================================================
Event EventLoop::next(void)
{
	using namespace std::chrono_literals;
	while (events.empty())
		std::this_thread::sleep_for(200ms); // TODO maybe std::condition_variable?
	std::lock_guard lock(eventsMutex);
	Event&& ret = std::move(events.front());
	events.pop();
	return std::move(ret);
}

//============================================================
// Pool for reading and queuing console input
//============================================================
void EventLoop::consoleReader(void)
{
	std::string input;
	while (std::getline(std::cin, input))
	{
		std::lock_guard lock(eventsMutex);
		events.emplace(EventSource::CONSOLE, input);
	}
}

//============================================================
// Engine event processer 
//============================================================
void EventLoop::engineProcesser(const SearchEvent& searchEvent)
{
	std::lock_guard lock(eventsMutex);
	events.emplace(EventSource::ENGINE, searchEvent);
}