#include <any>
#include <functional>
#include <iostream>
#include <memory>
#include <mutex>
#include <shared_mutex>
#include <type_traits>
#include <typeindex>
#include <unordered_map>
#include <vector>

class SingleEvent {
   public:
    template <typename Event>
    void Publish(const Event &event) {
        std::vector<Execute> executes;
        {
            std::shared_lock lock(m_rwlock);

            auto index = std::type_index(typeid(Event));
            auto it = m_subscribes.find(index);
            if (it != m_subscribes.end()) {
                for (auto &cb : it->second) {
                    executes.push_back({std::make_any<Event>(event), cb});
                }
            }
        }

        for (auto &exec : executes) {
            exec.callback(exec.event);
        }
    }

    template <typename Event>
    void Subscribe(const std::function<void(const Event &)> &callback) {
        std::unique_lock lock(m_rwlock);

        auto index = std::type_index(typeid(Event));
        m_subscribes[index].push_back([cb = callback](const std::any &e) {
            const Event *event = std::any_cast<Event>(&e);
            if (event == nullptr) {
                std::cerr << "[SingleEvent] any cast error for event type: " << std::type_index(typeid(Event)).name()
                          << std::endl;
                return;
            }
            cb(*event);
        });
    }

    template <typename Event>
    void Unsubscribe() {
        std::unique_lock lock(m_rwlock);

        auto index = std::type_index(typeid(Event));
        if (m_subscribes.erase(index)) {
            std::cout << "[SingleEvent] Unsubscribed from event type: " << std::type_index(typeid(Event)).name()
                      << std::endl;
        }
    }

    void UnsubscribeAll() {
        std::unique_lock lock(m_rwlock);

        m_subscribes.clear();
        std::cout << "[SingleEvent] Unsubscribed from all events.\n";
    }

   private:
    struct Execute {
        std::any event;
        std::function<void(const std::any &)> callback;
    };

    std::shared_mutex m_rwlock;
    std::unordered_map<std::type_index, std::vector<std::function<void(const std::any &)>>> m_subscribes;
};
