#include <algorithm>
#include <any>
#include <functional>
#include <iostream>
#include <memory>
#include <mutex>
#include <optional>
#include <shared_mutex>
#include <sstream>
#include <tuple>
#include <type_traits>
#include <typeindex>
#include <unordered_map>
#include <unordered_set>
#include <vector>
#include <sstream>

template <typename Event, typename... Events>
struct TypeIndex;

template <typename Event, typename... Events>
struct TypeIndex<Event, Event, Events...> : std::integral_constant<std::size_t, 0> {};

template <typename Event, typename U, typename... Events>
struct TypeIndex<Event, U, Events...> : std::integral_constant<std::size_t, 1 + TypeIndex<Event, Events...>::value> {};

class MultiEvent {
   public:
    template <typename Event>
    void Publish(const Event &event) {
        std::vector<ExecuteFunc> executes;


        {
            std::shared_lock lock(m_rwlock);
            const auto index = std::type_index(typeid(Event));

            std::cout << "[MultiEvent] Publishing event of type <" << index.name() << ">" << std::endl;

            auto it = m_subscribes.find(index);
            if (it != m_subscribes.end()) {
                for (auto &cb : it->second) {
                    cb(std::make_any<Event>(event));
                }
            }

            for (auto it = m_aggregators.begin(); it != m_aggregators.end();) {
                auto exec = (*it)->GetExecute();
                if (exec) {
                    executes.push_back(*exec);
                    std::cout << "[MultiEvent] Aggregator triggered for type <" << (*it)->GetName() << ">" << std::endl;
                    it = m_aggregators.erase(it);
                } else {
                    ++it;
                }
            }
        }

        for (auto &exec : executes) {
            exec();
        }
    }

    template <typename... Events>
    void Subscribe(const Events &...conditions, const std::function<void(const Events &...)> &callback) {
        static_assert(sizeof...(Events) > 0, "At least one event type is required.");
        static_assert((std::is_copy_constructible_v<Events> && ...), "Event types must be copy constructible.");

        std::unique_lock lock(m_rwlock);

        auto agg = std::make_shared<EventAggregator<Events...>>(conditions..., callback);
        (InternalMultiSubscribe<Events>(agg), ...);
        m_aggregators.emplace_back(agg);
    }

    template <typename Event>
    void Unsubscribe() {
        std::unique_lock lock(m_rwlock);

        const auto index = std::type_index(typeid(Event));
        if (m_subscribes.erase(index)) {
            std::cout << "[MultiEvent] Unsubscribed from event type: " << index.name() << std::endl;
        }

        auto find = std::find_if(m_aggregators.begin(), m_aggregators.end(),
                                 [index](const auto &agg) { return agg->IsSubscribe(index); });
        if (find != m_aggregators.end()) {
            m_aggregators.erase(find);
            std::cout << "[MultiEvent] Unsubscribed from event type: " << index.name() << " (aggregator)\n";
        }
    }

    void UnsubscribeAll() {
        std::unique_lock lock(m_rwlock);
        m_subscribes.clear();
        m_aggregators.clear();
        std::cout << "[MultiEvent] Unsubscribed from all events.\n";
    }

   private:
    using ExecuteFunc = std::function<void()>;
    using ExecuteAnyFunc = std::function<void(const std::any &)>;

    class EventBase {
       public:
        virtual ~EventBase() = default;
        virtual bool IsSubscribe(std::type_index index) const = 0;
        virtual std::unique_ptr<ExecuteFunc> GetExecute() const = 0;
        virtual std::string GetName() const = 0;
    };

    static std::string StringJoin(const std::vector<std::string> &parts, const std::string &delimiter) {
        std::stringstream result;
        for (size_t i = 0; i < parts.size(); ++i) {
            result << parts[i];
            if (i < parts.size() - 1) {
                result << delimiter;
            }
        }
        return result.str();
    }

    template <typename Event>
    void InternalSubscribe(const std::function<void(const Event &)> &cb) {
        const auto index = std::type_index(typeid(Event));
        m_subscribes[index].push_back([cb, index](const std::any &e) {
            const Event *event = std::any_cast<Event>(&e);
            if (event == nullptr) {
                std::cerr << "[MultiEvent] any cast error for event type: " << index.name() << std::endl;
                return;
            }
            cb(*event);
        });
    }

    template <typename Event, typename StatePtr>
    void InternalMultiSubscribe(StatePtr state) {
        InternalSubscribe<Event>([state](const Event &e) { state->Notify(e); });
    }

    template <typename... Events>
    class EventAggregator : public EventBase {
       public:
        EventAggregator(const Events &...conditions, const std::function<void(const Events &...)> &callback)
            : m_conditions(conditions...), m_callback(callback) {
        }

        template <typename Event>
        void Notify(const Event &e) {
            constexpr size_t idx = TypeIndex<Event, Events...>::value;
            if (idx >= sizeof...(Events)) {
                std::cerr << "[EventAggregator] Received unexpected event type: " << typeid(Event).name() << std::endl;
                return;
            }
            if (std::get<idx>(m_conditions) == e) {
                std::get<TypeIndex<Event, Events...>::value>(m_arrives) = e;
            }
        }

        bool IsSubscribe(std::type_index index) const override {
            return ((index == std::type_index(typeid(Events))) || ...);
        }

        std::unique_ptr<ExecuteFunc> GetExecute() const override {
            auto ready = ((std::get<TypeIndex<Events, Events...>::value>(m_arrives).has_value()) && ...);
            if (!ready) {
                return nullptr;
            }
            return std::make_unique<ExecuteFunc>(ExecuteFunc{[cb = m_callback, events = m_arrives]() {
                std::apply([&cb](const auto &...args) { cb(args.value()...); }, events);
            }});
        }

        std::string GetName() const {
            std::vector<std::string> typeNames = {typeid(Events).name()...};

            return StringJoin(typeNames, ",");
        }

       private:
        std::tuple<Events...> m_conditions;
        std::tuple<std::optional<Events>...> m_arrives;
        std::function<void(const Events &...)> m_callback;
    };

    std::shared_mutex m_rwlock;
    std::unordered_map<std::type_index, std::vector<ExecuteAnyFunc>> m_subscribes;
    std::vector<std::shared_ptr<EventBase>> m_aggregators;
};
