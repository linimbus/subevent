#include "MultiEvent.h"

struct MsgEvent {
    int id;
    std::string data;
};

bool operator==(const MsgEvent &lhs, const MsgEvent &rhs) {
    return lhs.id == rhs.id && lhs.data == rhs.data;
}

struct MsgEvent2 {
    double value;
};

bool operator==(const MsgEvent2 &lhs, const MsgEvent2 &rhs) {
    return lhs.value == rhs.value;
}

struct MsgEvent3 {
    bool flag;
};

bool operator==(const MsgEvent3 &lhs, const MsgEvent3 &rhs) {
    return lhs.flag == rhs.flag;
}

int main() {
    MultiEvent bus;

    bus.Subscribe<MsgEvent>(MsgEvent{101, "Hello"}, [](const MsgEvent &e) { std::cout << "[Single] Got: " << e.data << "\n"; });

    bus.Subscribe<MsgEvent2>(MsgEvent2{3.14}, [](const MsgEvent2 &e) { std::cout << "[Single] Got value: " << e.value << "\n"; });

    bus.Subscribe<MsgEvent>(MsgEvent{101, "Hello"}, [](const MsgEvent &e) {
        std::cout << "[Single] Got id: " << e.id << ", data: " << e.data << "\n";
    });

    bus.Subscribe<MsgEvent2, MsgEvent3>(MsgEvent2{3.14}, MsgEvent3{true}, [](const MsgEvent2 &e1, const MsgEvent3 &e2) {
        std::cout << "[Multi] Combined -> val: " << e1.value << ", flag: " << e2.flag << "\n";
    });

    bus.Publish<MsgEvent>(MsgEvent{101, "Hello"});
    bus.Publish<MsgEvent>(MsgEvent{102, "Hello World"});

    bus.Publish<MsgEvent2>(MsgEvent2{3.14});
    bus.Publish<MsgEvent3>(MsgEvent3{true});

    bus.Unsubscribe<MsgEvent>();
    bus.Unsubscribe<MsgEvent2>();
    bus.Unsubscribe<MsgEvent3>();

    return 0;
}