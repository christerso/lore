#pragma once

#include <lore/ecs/ecs.hpp>
#include <lore/math/math.hpp>

#include <functional>
#include <memory>
#include <vector>
#include <unordered_map>
#include <unordered_set>
#include <queue>
#include <mutex>
#include <atomic>
#include <chrono>
#include <typeinfo>
#include <typeindex>
#include <string>
#include <algorithm>
#include <thread>
#include <condition_variable>

struct GLFWwindow;

namespace lore::input {

// Forward declarations
class EventDispatcher;
class EventListenerRegistry;
class InputEventProcessor;
template<typename EventType> class EventListener;

// Event priority levels (higher = processed first)
enum class EventPriority : std::uint8_t {
    Lowest = 0,
    Low = 64,
    Normal = 128,
    High = 192,
    Highest = 255
};

// Base event interface - all events must inherit from this
class IEvent {
public:
    using TimePoint = std::chrono::high_resolution_clock::time_point;
    using EventId = std::size_t;

    IEvent()
        : timestamp_(std::chrono::high_resolution_clock::now())
        , frame_number_(0)
        , handled_(false)
        , id_(generate_id())
    {}

    virtual ~IEvent() = default;

    // Event identification
    EventId get_id() const noexcept { return id_; }
    virtual std::type_index get_type() const = 0;
    virtual const char* get_name() const = 0;

    // Timing information
    TimePoint get_timestamp() const noexcept { return timestamp_; }
    std::uint64_t get_frame_number() const noexcept { return frame_number_; }
    void set_frame_number(std::uint64_t frame) noexcept { frame_number_ = frame; }

    // Event lifecycle
    bool is_handled() const noexcept { return handled_.load(); }
    void mark_handled() noexcept { handled_.store(true); }
    void reset_handled() noexcept { handled_.store(false); }

    // Priority
    virtual EventPriority get_priority() const noexcept { return EventPriority::Normal; }

    // Serialization support for debugging/logging
    virtual std::string to_string() const { return get_name(); }

private:
    TimePoint timestamp_;
    std::uint64_t frame_number_;
    std::atomic<bool> handled_;
    EventId id_;

    static EventId generate_id() {
        static std::atomic<EventId> counter{0};
        return ++counter;
    }
};

// Template base for typed events
template<typename Derived>
class Event : public IEvent {
public:
    std::type_index get_type() const override {
        return std::type_index(typeid(Derived));
    }

    const char* get_name() const override {
        return typeid(Derived).name();
    }
};

// Event listener interface
class IEventListener {
public:
    virtual ~IEventListener() = default;
    virtual std::type_index get_event_type() const = 0;
    virtual void handle_event(const IEvent& event) = 0;
    virtual EventPriority get_priority() const noexcept { return EventPriority::Normal; }
    virtual bool is_valid() const noexcept { return true; }
};

// Typed event listener
template<typename EventType>
class EventListener : public IEventListener {
public:
    using HandlerFunction = std::function<void(const EventType&)>;

    explicit EventListener(HandlerFunction handler, EventPriority priority = EventPriority::Normal)
        : handler_(std::move(handler))
        , priority_(priority)
        , valid_(true)
    {}

    std::type_index get_event_type() const override {
        return std::type_index(typeid(EventType));
    }

    void handle_event(const IEvent& event) override {
        if (!valid_ || !handler_) return;

        // Safe downcast - type already verified by dispatcher
        const auto& typed_event = static_cast<const EventType&>(event);
        handler_(typed_event);
    }

    EventPriority get_priority() const noexcept override {
        return priority_;
    }

    bool is_valid() const noexcept override {
        return valid_.load();
    }

    void invalidate() noexcept {
        valid_.store(false);
    }

private:
    HandlerFunction handler_;
    EventPriority priority_;
    std::atomic<bool> valid_;
};

// Listener handle for automatic cleanup
class ListenerHandle {
public:
    ListenerHandle() = default;

    explicit ListenerHandle(std::shared_ptr<IEventListener> listener)
        : listener_(std::move(listener))
    {}

    ~ListenerHandle() {
        disconnect();
    }

    // Move semantics only
    ListenerHandle(const ListenerHandle&) = delete;
    ListenerHandle& operator=(const ListenerHandle&) = delete;

    ListenerHandle(ListenerHandle&& other) noexcept
        : listener_(std::move(other.listener_))
    {}

    ListenerHandle& operator=(ListenerHandle&& other) noexcept {
        if (this != &other) {
            disconnect();
            listener_ = std::move(other.listener_);
        }
        return *this;
    }

    void disconnect() {
        if (auto listener = listener_.lock()) {
            // Invalidate the listener
            if (auto typed_listener = std::dynamic_pointer_cast<IEventListener>(listener)) {
                // Listener will be removed on next cleanup
                if (auto* event_listener = dynamic_cast<EventListener<IEvent>*>(typed_listener.get())) {
                    event_listener->invalidate();
                }
            }
            listener_.reset();
        }
    }

    bool is_connected() const noexcept {
        return !listener_.expired();
    }

private:
    std::weak_ptr<IEventListener> listener_;
};

// Thread-safe event queue with priority support
class EventQueue {
public:
    EventQueue() = default;
    ~EventQueue() = default;

    // Non-copyable, movable
    EventQueue(const EventQueue&) = delete;
    EventQueue& operator=(const EventQueue&) = delete;
    EventQueue(EventQueue&&) = default;
    EventQueue& operator=(EventQueue&&) = default;

    // Event submission
    void push_event(std::unique_ptr<IEvent> event);
    void push_high_priority_event(std::unique_ptr<IEvent> event);

    // Event consumption
    std::vector<std::unique_ptr<IEvent>> poll_events();
    std::vector<std::unique_ptr<IEvent>> poll_events_by_type(std::type_index type);

    // Queue management
    void clear();
    void reserve(std::size_t capacity);

    // Statistics
    std::size_t size() const;
    std::size_t high_priority_size() const;
    std::size_t total_events_processed() const noexcept { return total_processed_.load(); }

    // Configuration
    void set_max_events(std::size_t max_events) noexcept { max_events_ = max_events; }
    std::size_t get_max_events() const noexcept { return max_events_; }

private:
    mutable std::mutex mutex_;

    // Priority queues
    std::priority_queue<std::unique_ptr<IEvent>,
                       std::vector<std::unique_ptr<IEvent>>,
                       std::function<bool(const std::unique_ptr<IEvent>&, const std::unique_ptr<IEvent>&)>> high_priority_queue_{
        [](const std::unique_ptr<IEvent>& a, const std::unique_ptr<IEvent>& b) {
            return a->get_priority() < b->get_priority();
        }
    };

    std::queue<std::unique_ptr<IEvent>> normal_queue_;

    // Statistics
    std::atomic<std::size_t> total_processed_{0};
    std::size_t max_events_{10000};
};

// Event listener registry with type safety and cleanup
class EventListenerRegistry {
public:
    EventListenerRegistry() = default;
    ~EventListenerRegistry() = default;

    // Non-copyable, movable
    EventListenerRegistry(const EventListenerRegistry&) = delete;
    EventListenerRegistry& operator=(const EventListenerRegistry&) = delete;
    EventListenerRegistry(EventListenerRegistry&&) = default;
    EventListenerRegistry& operator=(EventListenerRegistry&&) = default;

    // Listener registration
    template<typename EventType>
    ListenerHandle register_listener(std::function<void(const EventType&)> handler,
                                   EventPriority priority = EventPriority::Normal);

    template<typename EventType>
    ListenerHandle register_listener(std::shared_ptr<EventListener<EventType>> listener);

    // Bulk operations
    void unregister_all_listeners_for_type(std::type_index type);
    void unregister_all_listeners();

    // Listener queries
    std::size_t get_listener_count(std::type_index type) const;
    std::size_t get_total_listener_count() const;
    std::vector<std::type_index> get_registered_types() const;

    // Event dispatch to listeners
    void dispatch_event(const IEvent& event);

    // Cleanup invalid listeners
    void cleanup_invalid_listeners();

    // Statistics
    std::size_t get_events_dispatched() const noexcept { return events_dispatched_.load(); }

private:
    mutable std::shared_mutex registry_mutex_;

    // Type-indexed listener storage
    std::unordered_map<std::type_index, std::vector<std::shared_ptr<IEventListener>>> listeners_;

    // Statistics
    std::atomic<std::size_t> events_dispatched_{0};

    // Helper for sorting listeners by priority
    void sort_listeners_by_priority(std::vector<std::shared_ptr<IEventListener>>& listeners);
};

// Main event dispatcher - coordinates queue and registry
class EventDispatcher {
public:
    EventDispatcher();
    ~EventDispatcher();

    // Non-copyable, movable
    EventDispatcher(const EventDispatcher&) = delete;
    EventDispatcher& operator=(const EventDispatcher&) = delete;
    EventDispatcher(EventDispatcher&&) = default;
    EventDispatcher& operator=(EventDispatcher&&) = default;

    // Event publishing
    template<typename EventType, typename... Args>
    void publish(Args&&... args);

    void publish_event(std::unique_ptr<IEvent> event);
    void publish_high_priority_event(std::unique_ptr<IEvent> event);

    // Listener management
    template<typename EventType>
    ListenerHandle subscribe(std::function<void(const EventType&)> handler,
                           EventPriority priority = EventPriority::Normal);

    template<typename EventType>
    ListenerHandle subscribe(std::shared_ptr<EventListener<EventType>> listener);

    // Event processing
    void process_events();
    void process_events_by_type(std::type_index type);
    std::size_t process_single_event();

    // Configuration
    void set_frame_number(std::uint64_t frame) noexcept { current_frame_.store(frame); }
    std::uint64_t get_frame_number() const noexcept { return current_frame_.load(); }

    void set_max_events_per_frame(std::size_t max) noexcept { max_events_per_frame_ = max; }
    std::size_t get_max_events_per_frame() const noexcept { return max_events_per_frame_; }

    // Queue access
    EventQueue& get_queue() noexcept { return event_queue_; }
    const EventQueue& get_queue() const noexcept { return event_queue_; }

    // Registry access
    EventListenerRegistry& get_registry() noexcept { return listener_registry_; }
    const EventListenerRegistry& get_registry() const noexcept { return listener_registry_; }

    // Statistics and debugging
    struct Statistics {
        std::size_t events_queued = 0;
        std::size_t events_processed = 0;
        std::size_t listeners_active = 0;
        std::size_t events_per_second = 0;
        float average_processing_time_ms = 0.0f;
    };

    Statistics get_statistics() const;
    void reset_statistics();

    // Debug/logging
    void set_debug_logging(bool enabled) noexcept { debug_logging_.store(enabled); }
    bool is_debug_logging_enabled() const noexcept { return debug_logging_.load(); }

private:
    EventQueue event_queue_;
    EventListenerRegistry listener_registry_;

    // Frame management
    std::atomic<std::uint64_t> current_frame_{0};
    std::size_t max_events_per_frame_{1000};

    // Performance tracking
    mutable std::mutex stats_mutex_;
    std::chrono::high_resolution_clock::time_point last_stats_reset_;
    std::size_t events_processed_since_reset_{0};
    float total_processing_time_ms_{0.0f};

    // Debug
    std::atomic<bool> debug_logging_{false};

    // Helper methods
    void update_statistics(float processing_time_ms);
    void log_event_if_debug(const IEvent& event, const std::string& action);
};

// Event filter for conditional processing
template<typename EventType>
class EventFilter {
public:
    using FilterFunction = std::function<bool(const EventType&)>;

    explicit EventFilter(FilterFunction filter) : filter_(std::move(filter)) {}

    bool should_process(const EventType& event) const {
        return filter_ ? filter_(event) : true;
    }

private:
    FilterFunction filter_;
};

// Scoped event listener for RAII cleanup
template<typename EventType>
class ScopedEventListener {
public:
    ScopedEventListener(EventDispatcher& dispatcher,
                       std::function<void(const EventType&)> handler,
                       EventPriority priority = EventPriority::Normal)
        : handle_(dispatcher.subscribe<EventType>(std::move(handler), priority))
    {}

    ~ScopedEventListener() = default;

    // Non-copyable, movable
    ScopedEventListener(const ScopedEventListener&) = delete;
    ScopedEventListener& operator=(const ScopedEventListener&) = delete;
    ScopedEventListener(ScopedEventListener&&) = default;
    ScopedEventListener& operator=(ScopedEventListener&&) = default;

    void disconnect() { handle_.disconnect(); }
    bool is_connected() const { return handle_.is_connected(); }

private:
    ListenerHandle handle_;
};

// Template implementations

template<typename EventType>
ListenerHandle EventListenerRegistry::register_listener(std::function<void(const EventType&)> handler,
                                                       EventPriority priority) {
    auto listener = std::make_shared<EventListener<EventType>>(std::move(handler), priority);

    {
        std::unique_lock lock(registry_mutex_);
        auto& type_listeners = listeners_[std::type_index(typeid(EventType))];
        type_listeners.push_back(listener);
        sort_listeners_by_priority(type_listeners);
    }

    return ListenerHandle(listener);
}

template<typename EventType>
ListenerHandle EventListenerRegistry::register_listener(std::shared_ptr<EventListener<EventType>> listener) {
    {
        std::unique_lock lock(registry_mutex_);
        auto& type_listeners = listeners_[std::type_index(typeid(EventType))];
        type_listeners.push_back(listener);
        sort_listeners_by_priority(type_listeners);
    }

    return ListenerHandle(listener);
}

template<typename EventType, typename... Args>
void EventDispatcher::publish(Args&&... args) {
    auto event = std::make_unique<EventType>(std::forward<Args>(args)...);
    event->set_frame_number(current_frame_.load());

    if constexpr (std::is_base_of_v<Event<EventType>, EventType>) {
        if (event->get_priority() > EventPriority::Normal) {
            event_queue_.push_high_priority_event(std::move(event));
        } else {
            event_queue_.push_event(std::move(event));
        }
    } else {
        event_queue_.push_event(std::move(event));
    }
}

template<typename EventType>
ListenerHandle EventDispatcher::subscribe(std::function<void(const EventType&)> handler,
                                         EventPriority priority) {
    return listener_registry_.register_listener<EventType>(std::move(handler), priority);
}

template<typename EventType>
ListenerHandle EventDispatcher::subscribe(std::shared_ptr<EventListener<EventType>> listener) {
    return listener_registry_.register_listener<EventType>(listener);
}

} // namespace lore::input