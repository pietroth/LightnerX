#include <dlfcn.h>

#include <cerrno>
#include <iostream>
#include <memory>
#include <stdexcept>

namespace {
template<class Function>
Function* load(void* plugin, const char* name) {
    auto* symbol = dlsym(plugin, name);
    if (!symbol) throw std::runtime_error(dlerror());
    return reinterpret_cast<Function*>(symbol);
}

struct Plugin {
    struct CloseLibrary {
        void operator()(void* library) const noexcept { dlclose(library); }
    };

    std::unique_ptr<void, CloseLibrary> handle;
    int (*start)();
    void (*stop)();
    bool (*running)();

    explicit Plugin(const char* path)
        : handle(dlopen(path, RTLD_NOW | RTLD_LOCAL)) {
        if (!handle) throw std::runtime_error(dlerror());
        start = load<int()>(handle.get(), "startClient");
        stop = load<void()>(handle.get(), "stopClient");
        running = load<bool()>(handle.get(), "networkRunning");
    }

    ~Plugin() { stop(); }
};
}

int main(int argc, char** argv) {
    if (argc != 3) return 1;
    try {
        Plugin first(argv[1]);
        Plugin second(argv[2]);
        if (first.start() != 0 || second.start() != EBUSY || !second.running()) {
            throw std::runtime_error("loaded modules did not share active network ownership");
        }
        first.stop();
        if (second.running() || second.start() != 0 || !first.running()) {
            throw std::runtime_error("network ownership did not transfer after shutdown");
        }
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
