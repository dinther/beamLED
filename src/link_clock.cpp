// Link (and the Asio it brings) is kept to this file.
#include "link_clock.h"

#include <ableton/Link.hpp>

#include <algorithm>
#include <chrono>

// lwIP has no interface names; Link asks for them only for IPv6 scopes (as in Link's ESP32 example)
unsigned int if_nametoindex(const char*) { return 0; }
char* if_indextoname(unsigned int, char*) { return nullptr; }

namespace {
constexpr double kQuantum = 4;
}

struct LinkClock::Impl {
    explicit Impl(double bpm) : link(bpm) {}
    ableton::Link link;
};

LinkClock::LinkClock(double bpm) : impl_(new Impl(bpm)) {}

LinkClock::~LinkClock() { impl_->link.enable(false); }

void LinkClock::enable(bool on) {
    if (impl_->link.isEnabled() != on) impl_->link.enable(on);
}

BeatTime LinkClock::now(double aheadMs) const {
    const auto state = impl_->link.captureAppSessionState();
    const auto time = impl_->link.clock().micros() +
                      std::chrono::microseconds(static_cast<long long>(aheadMs * 1000));
    BeatTime t;
    t.bpm = state.tempo();
    t.beat = state.beatAtTime(time, kQuantum);
    t.phase = state.phaseAtTime(time, kQuantum);
    t.quantum = kQuantum;
    t.peers = static_cast<int>(impl_->link.numPeers());
    t.link = impl_->link.isEnabled();
    return t;
}

void LinkClock::rejoin() {
    if (!impl_->link.isEnabled()) return;
    const auto at = impl_->link.clock().micros();
    const auto old = impl_->link.captureAppSessionState();
    const double bpm = old.tempo();
    const double beat = old.beatAtTime(at, kQuantum);
    impl_->link.enable(false);
    impl_.reset(new Impl(bpm));
    // A new Link starts its beat count at 0: carry the beat over so the bar doesn't jump
    auto state = impl_->link.captureAppSessionState();
    state.requestBeatAtTime(beat, at, kQuantum);
    impl_->link.commitAppSessionState(state);
    impl_->link.enable(true);
}

void LinkClock::setTempo(double bpm) {
    auto state = impl_->link.captureAppSessionState();
    state.setTempo(std::clamp(bpm, 20.0, 999.0), impl_->link.clock().micros());
    impl_->link.commitAppSessionState(state);
}
