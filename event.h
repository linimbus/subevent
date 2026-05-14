#include <algorithm>
#include <any>
#include <functional>
#include <iostream>
#include <memory>
#include <mutex>
#include <optional>
#include <shared_mutex>
#include <tuple>
#include <type_traits>
#include <typeindex>
#include <unordered_map>
#include <unordered_set>
#include <vector>

template <typename Event, typename... Events> struct TypeIndex;

template <typename Event, typename... Events>
struct TypeIndex<Event, Event, Events...>
    : std::integral_constant<std::size_t, 0> {};

template <typename Event, typename U, typename... Events>
struct TypeIndex<Event, U, Events...>
    : std::integral_constant<std::size_t,
                             1 + TypeIndex<Event, Events...>::value> {};

class EventFramework {
public:
  template <typename Event> void Publish(const Event &event) {
    std::shared_lock lock(m_rwlock);

    auto it = m_subscribes.find(std::type_index(typeid(Event)));
    if (it != m_subscribes.end()) {
      for (auto &cb : it->second) {
        try {
          cb(std::make_any<Event>(event));
        } catch (const std::exception &e) {
          std::cerr << "[EventFramework] Callback error: " << e.what() << "\n";
        }
      }
    }
  }

  template <typename Event>
  void Subscribe(const std::function<void(const Event &)> &callback) {
    std::unique_lock lock(m_rwlock);
    InternalSubscribe<Event>(callback);
  }

  template <typename... Events>
  void Subscribe(const Events &...conditions,
                 const std::function<void(const Events &...)> &callback) {
    static_assert(sizeof...(Events) > 0,
                  "At least one event type is required.");
    static_assert((std::is_copy_constructible_v<Events> && ...),
                  "Event types must be copy constructible.");

    std::unique_lock lock(m_rwlock);

    auto sub =
        std::make_shared<EventAggregator<Events...>>(conditions..., callback);
    (InternalMultiSubscribe<Events>(sub), ...);
    m_aggregators.emplace_back(sub);
  }

  template <typename Event> void Unsubscribe() {
    std::unique_lock lock(m_rwlock);
    if (m_subscribes.erase(std::type_index(typeid(Event)))) {
      std::cout << "[EventFramework] Unsubscribed from event type: "
                << std::type_index(typeid(Event)).name() << "\n";
    }

    auto find =
        std::find_if(m_aggregators.begin(), m_aggregators.end(),
                     [index = std::type_index(typeid(Event))](const auto &agg) {
                       return agg->IsSubscribe(index);
                     });
    if (find != m_aggregators.end()) {
      m_aggregators.erase(find);
      std::cout << "[EventFramework] Unsubscribed from event type: "
                << std::type_index(typeid(Event)).name() << " (aggregator)\n";
    }
  }

  void UnsubscribeAll() {
    std::unique_lock lock(m_rwlock);
    m_subscribes.clear();
    m_aggregators.clear();
    std::cout << "[EventFramework] Unsubscribed from all events.\n";
  }

private:
  template <typename Event>
  void InternalSubscribe(const std::function<void(const Event &)> &callback) {
    m_subscribes[std::type_index(typeid(Event))].push_back(
        [cb = callback](const std::any &e) {
          try {
            cb(std::any_cast<const Event &>(e));
          } catch (const std::exception &e) {
            std::cerr << "[EventFramework] Callback error: " << e.what()
                      << "\n";
          }
        });
  }

  template <typename Event, typename StatePtr>
  void InternalMultiSubscribe(StatePtr state) {
    InternalSubscribe<Event>([state, this](const Event &e) {
      if (state->NotifyCallback(e)) {
        auto it = std::find(m_aggregators.begin(), m_aggregators.end(), state);
        if (it != m_aggregators.end()) {
          m_aggregators.erase(it);
          std::cout << "[EventFramework] Aggregator completed and removed.\n";
        }
      }
    });
  }

  class EventBase {
  public:
    virtual ~EventBase() = default;
    virtual bool IsSubscribe(std::type_index index) const = 0;
  };

  template <typename... Events> class EventAggregator : public EventBase {
  public:
    EventAggregator(const Events &...conditions,
                    const std::function<void(const Events &...)> &callback)
        : m_conditions(conditions...), m_callback(callback) {}

    template <typename Event> bool NotifyCallback(const Event &e) {
      constexpr size_t idx = TypeIndex<Event, Events...>::value;
      if (idx >= sizeof...(Events)) {
        std::cerr << "[EventAggregator] Received unexpected event type: "
                  << typeid(Event).name() << "\n";
        return false;
      }

      if (std::get<idx>(m_conditions) == e) {
        std::get<TypeIndex<Event, Events...>::value>(m_arrives) = e;
      }

      return invoke();
    }

    bool IsSubscribe(std::type_index index) const override {
      return ((index == std::type_index(typeid(Events))) || ...);
    }

  private:
    bool invoke() {
      bool full = std::apply(
          [](const auto &...opts) { return (... && opts.has_value()); },
          m_arrives);

      if (full) {
        std::apply(
            [this](auto &...opts) {
              try {
                m_callback(opts.value()...);
              } catch (const std::exception &e) {
                std::cerr << "[EventAggregator] Execute callback error: "
                          << e.what() << "\n";
              }
            },
            m_arrives);

        std::apply([](auto &...opts) { (opts.reset(), ...); }, m_arrives);
        return true;
      }
      return false;
    }

    std::tuple<Events...> m_conditions;
    std::tuple<std::optional<Events>...> m_arrives;
    std::function<void(const Events &...)> m_callback;
  };

  std::shared_mutex m_rwlock;
  std::unordered_map<std::type_index,
                     std::vector<std::function<void(const std::any &)>>>
      m_subscribes;
  std::vector<std::shared_ptr<EventBase>> m_aggregators;
};
