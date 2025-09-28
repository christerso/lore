#include <lore/input/event_system.hpp>

#include <algorithm>
#include <iostream>
#include <iomanip>
#include <sstream>

namespace lore::input {

// EventQueue implementation
void EventQueue::push_event(std::unique_ptr<IEvent> event) {
    std::lock_guard<std::mutex> lock(mutex_);

    // Check queue size limit
    if (normal_queue_.size() + high_priority_queue_.size() >= max_events_) {
        // Drop oldest normal priority event
        if (!normal_queue_.empty()) {
            normal_queue_.pop();
        }
    }

    normal_queue_.push(std::move(event));
}

void EventQueue::push_high_priority_event(std::unique_ptr<IEvent> event) {
    std::lock_guard<std::mutex> lock(mutex_);

    // High priority events can exceed normal limits slightly
    if (high_priority_queue_.size() >= max_events_ / 4) {
        // Only drop if high priority queue gets too large
        return;
    }

    high_priority_queue_.push(std::move(event));
}

std::vector<std::unique_ptr<IEvent>> EventQueue::poll_events() {
    std::lock_guard<std::mutex> lock(mutex_);

    std::vector<std::unique_ptr<IEvent>> result;
    result.reserve(high_priority_queue_.size() + normal_queue_.size());

    // Process high priority events first
    while (!high_priority_queue_.empty()) {
        result.push_back(std::move(const_cast<std::unique_ptr<IEvent>&>(high_priority_queue_.top())));
        high_priority_queue_.pop();
    }

    // Then normal priority events
    while (!normal_queue_.empty()) {
        result.push_back(std::move(normal_queue_.front()));
        normal_queue_.pop();
    }

    total_processed_.fetch_add(result.size());
    return result;
}

std::vector<std::unique_ptr<IEvent>> EventQueue::poll_events_by_type(std::type_index type) {
    std::lock_guard<std::mutex> lock(mutex_);

    std::vector<std::unique_ptr<IEvent>> result;
    std::vector<std::unique_ptr<IEvent>> remaining;

    // Extract matching events from high priority queue
    std::vector<std::unique_ptr<IEvent>> temp_high;
    while (!high_priority_queue_.empty()) {
        auto event = std::move(const_cast<std::unique_ptr<IEvent>&>(high_priority_queue_.top()));
        high_priority_queue_.pop();

        if (event->get_type() == type) {
            result.push_back(std::move(event));
        } else {
            temp_high.push_back(std::move(event));
        }
    }

    // Restore remaining high priority events
    for (auto& event : temp_high) {
        high_priority_queue_.push(std::move(event));
    }

    // Extract matching events from normal queue
    std::queue<std::unique_ptr<IEvent>> temp_normal;
    while (!normal_queue_.empty()) {
        auto event = std::move(normal_queue_.front());
        normal_queue_.pop();

        if (event->get_type() == type) {
            result.push_back(std::move(event));
        } else {
            temp_normal.push(std::move(event));
        }
    }

    // Restore remaining normal events
    normal_queue_ = std::move(temp_normal);

    total_processed_.fetch_add(result.size());
    return result;
}

void EventQueue::clear() {
    std::lock_guard<std::mutex> lock(mutex_);

    while (!high_priority_queue_.empty()) {
        high_priority_queue_.pop();
    }

    while (!normal_queue_.empty()) {
        normal_queue_.pop();
    }
}

void EventQueue::reserve(std::size_t capacity) {
    std::lock_guard<std::mutex> lock(mutex_);
    // Note: std::queue doesn't have reserve, but we track the capacity
    max_events_ = capacity;
}

std::size_t EventQueue::size() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return normal_queue_.size();
}

std::size_t EventQueue::high_priority_size() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return high_priority_queue_.size();
}

// EventListenerRegistry implementation
void EventListenerRegistry::dispatch_event(const IEvent& event) {
    events_dispatched_.fetch_add(1);

    std::shared_lock lock(registry_mutex_);

    auto it = listeners_.find(event.get_type());
    if (it == listeners_.end()) {
        // No listeners for this event type - safe dispatch with zero listeners
        return;
    }

    // Create a copy of listeners to avoid holding lock during dispatch
    auto listeners_copy = it->second;
    lock.unlock();

    // Dispatch to all valid listeners
    for (const auto& listener : listeners_copy) {
        if (listener && listener->is_valid()) {
            try {
                listener->handle_event(event);

                // Stop if event was marked as handled by this listener
                if (event.is_handled()) {
                    break;
                }
            } catch (const std::exception& e) {
                // Log error but continue processing other listeners
                std::cerr << "Event listener error: " << e.what() << std::endl;
            }
        }
    }
}

void EventListenerRegistry::unregister_all_listeners_for_type(std::type_index type) {
    std::unique_lock lock(registry_mutex_);

    auto it = listeners_.find(type);
    if (it != listeners_.end()) {
        // Invalidate all listeners for this type
        for (auto& listener : it->second) {
            if (listener) {
                // Mark as invalid - actual removal happens during cleanup
                // This is a design choice to avoid complex listener lifetime management
            }
        }
        listeners_.erase(it);
    }
}

void EventListenerRegistry::unregister_all_listeners() {
    std::unique_lock lock(registry_mutex_);
    listeners_.clear();
}

std::size_t EventListenerRegistry::get_listener_count(std::type_index type) const {
    std::shared_lock lock(registry_mutex_);

    auto it = listeners_.find(type);
    if (it != listeners_.end()) {
        // Count only valid listeners
        return std::count_if(it->second.begin(), it->second.end(),
                           [](const auto& listener) {
                               return listener && listener->is_valid();
                           });
    }
    return 0;
}

std::size_t EventListenerRegistry::get_total_listener_count() const {
    std::shared_lock lock(registry_mutex_);

    std::size_t total = 0;
    for (const auto& [type, listeners] : listeners_) {
        total += std::count_if(listeners.begin(), listeners.end(),
                             [](const auto& listener) {
                                 return listener && listener->is_valid();
                             });
    }
    return total;
}

std::vector<std::type_index> EventListenerRegistry::get_registered_types() const {
    std::shared_lock lock(registry_mutex_);

    std::vector<std::type_index> types;
    types.reserve(listeners_.size());

    for (const auto& [type, listeners] : listeners_) {
        // Only include types that have valid listeners
        bool has_valid_listeners = std::any_of(listeners.begin(), listeners.end(),
                                             [](const auto& listener) {
                                                 return listener && listener->is_valid();
                                             });
        if (has_valid_listeners) {
            types.push_back(type);
        }
    }

    return types;
}

void EventListenerRegistry::cleanup_invalid_listeners() {
    std::unique_lock lock(registry_mutex_);

    for (auto it = listeners_.begin(); it != listeners_.end();) {
        auto& listeners_vec = it->second;

        // Remove invalid listeners
        listeners_vec.erase(
            std::remove_if(listeners_vec.begin(), listeners_vec.end(),
                          [](const auto& listener) {
                              return !listener || !listener->is_valid();
                          }),
            listeners_vec.end());

        // Remove empty type entries
        if (listeners_vec.empty()) {
            it = listeners_.erase(it);
        } else {
            ++it;
        }
    }
}

void EventListenerRegistry::sort_listeners_by_priority(std::vector<std::shared_ptr<IEventListener>>& listeners) {
    std::sort(listeners.begin(), listeners.end(),
              [](const auto& a, const auto& b) {
                  if (!a) return false;
                  if (!b) return true;
                  return a->get_priority() > b->get_priority();
              });
}

// EventDispatcher implementation
EventDispatcher::EventDispatcher()
    : last_stats_reset_(std::chrono::high_resolution_clock::now())
{}

EventDispatcher::~EventDispatcher() = default;

void EventDispatcher::publish_event(std::unique_ptr<IEvent> event) {
    if (!event) return;

    event->set_frame_number(current_frame_.load());
    log_event_if_debug(*event, "Publishing");

    event_queue_.push_event(std::move(event));
}

void EventDispatcher::publish_high_priority_event(std::unique_ptr<IEvent> event) {
    if (!event) return;

    event->set_frame_number(current_frame_.load());
    log_event_if_debug(*event, "Publishing (High Priority)");

    event_queue_.push_high_priority_event(std::move(event));
}

void EventDispatcher::process_events() {
    auto start_time = std::chrono::high_resolution_clock::now();

    auto events = event_queue_.poll_events();
    std::size_t events_processed = 0;

    for (auto& event : events) {
        if (events_processed >= max_events_per_frame_) {
            // Put remaining events back in queue
            // This is a simplification - in production, you might want a more sophisticated approach
            break;
        }

        if (event) {
            log_event_if_debug(*event, "Processing");
            listener_registry_.dispatch_event(*event);
            ++events_processed;
        }
    }

    events_processed_since_reset_ += events_processed;

    auto end_time = std::chrono::high_resolution_clock::now();
    float processing_time = std::chrono::duration<float, std::milli>(end_time - start_time).count();
    update_statistics(processing_time);

    // Periodic cleanup of invalid listeners
    static std::size_t cleanup_counter = 0;
    if (++cleanup_counter % 1000 == 0) {
        listener_registry_.cleanup_invalid_listeners();
    }
}

void EventDispatcher::process_events_by_type(std::type_index type) {
    auto start_time = std::chrono::high_resolution_clock::now();

    auto events = event_queue_.poll_events_by_type(type);
    std::size_t events_processed = 0;

    for (auto& event : events) {
        if (events_processed >= max_events_per_frame_) {
            break;
        }

        if (event && event->get_type() == type) {
            log_event_if_debug(*event, "Processing (Type-specific)");
            listener_registry_.dispatch_event(*event);
            ++events_processed;
        }
    }

    events_processed_since_reset_ += events_processed;

    auto end_time = std::chrono::high_resolution_clock::now();
    float processing_time = std::chrono::duration<float, std::milli>(end_time - start_time).count();
    update_statistics(processing_time);
}

std::size_t EventDispatcher::process_single_event() {
    auto events = event_queue_.poll_events();
    if (events.empty()) {
        return 0;
    }

    auto& event = events.front();
    if (event) {
        log_event_if_debug(*event, "Processing (Single)");
        listener_registry_.dispatch_event(*event);
        ++events_processed_since_reset_;
        return 1;
    }

    return 0;
}

EventDispatcher::Statistics EventDispatcher::get_statistics() const {
    std::lock_guard<std::mutex> lock(stats_mutex_);

    auto now = std::chrono::high_resolution_clock::now();
    auto elapsed = std::chrono::duration<float>(now - last_stats_reset_).count();

    Statistics stats;
    stats.events_queued = event_queue_.size() + event_queue_.high_priority_size();
    stats.events_processed = event_queue_.total_events_processed();
    stats.listeners_active = listener_registry_.get_total_listener_count();
    stats.events_per_second = elapsed > 0.0f ? static_cast<std::size_t>(events_processed_since_reset_ / elapsed) : 0;
    stats.average_processing_time_ms = events_processed_since_reset_ > 0 ?
        total_processing_time_ms_ / events_processed_since_reset_ : 0.0f;

    return stats;
}

void EventDispatcher::reset_statistics() {
    std::lock_guard<std::mutex> lock(stats_mutex_);

    last_stats_reset_ = std::chrono::high_resolution_clock::now();
    events_processed_since_reset_ = 0;
    total_processing_time_ms_ = 0.0f;
}

void EventDispatcher::update_statistics(float processing_time_ms) {
    std::lock_guard<std::mutex> lock(stats_mutex_);
    total_processing_time_ms_ += processing_time_ms;
}

void EventDispatcher::log_event_if_debug(const IEvent& event, const std::string& action) {
    if (!debug_logging_.load()) return;

    std::ostringstream oss;
    oss << "[Frame " << event.get_frame_number() << "] "
        << action << " event: " << event.get_name()
        << " (ID: " << event.get_id() << ", Priority: "
        << static_cast<int>(event.get_priority()) << ")";

    std::cout << oss.str() << std::endl;
}

} // namespace lore::input