#include "MockGestureManager.hpp"
#include <iostream>

#define CONFIG_SENSITIVITY 1.0

void debug(const std::string& s) {
    std::cout << "[debug] " << s << "\n";
}

bool CMockGestureManager::handleCompletedGesture(const CompletedGestureEvent& gev) {
    std::cout << "gesture triggered: " << gev.to_string() << "\n";
    this->triggered = true;
    return true;
}

bool CMockGestureManager::handleDragGesture(const DragGestureEvent& gev) {
    std::cout << "drag started: " << gev.to_string() << "\n";
    return this->handlesDragEvents;
}

void CMockGestureManager::dragGestureUpdate(const wf::touch::gesture_event_t& gev) {
    std::cout << "drag update" << std::endl;
}

void CMockGestureManager::handleDragGestureEnd(const DragGestureEvent& gev) {
    std::cout << "drag end: " << gev.to_string() << "\n";
    this->dragEnded = true;
}

void CMockGestureManager::handleCancelledGesture() {
    std::cout << "gesture cancelled\n";
    this->cancelled = true;
}

void CMockGestureManager::sendCancelEventsToWindows() {
    std::cout << "cancel touch on windows\n";
}
