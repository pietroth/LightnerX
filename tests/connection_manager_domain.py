#!/usr/bin/env python3
"""Exercise all 65535 IDs with a fake GNS interface and the actual manager body.

The normal C++ test imports the real module and uses real GNS socket pairs. This
additional test strips only module/import/include declarations and substitutes a
minimal GNS interface to avoid requiring 65536 live network connections.
"""
import argparse
from pathlib import Path
import shlex
import subprocess
import tempfile

parser = argparse.ArgumentParser()
parser.add_argument("--compiler", default="c++")
parser.add_argument("--source", type=Path, default=Path(__file__).resolve().parents[1] / "network/connection_manager.cppm")
parser.add_argument("--flags", default="")
args = parser.parse_args()
source = args.source.read_text()
source = source[source.index("export namespace"):].replace("export namespace", "namespace", 1)
preamble = r'''
#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <limits>
#include <optional>
#include <set>
#include <stdexcept>
#include <unordered_map>
#include <vector>
using HSteamNetConnection = uint32_t;
using int64 = int64_t;
struct SteamNetConnectionInfo_t {};
struct ISteamNetworkingSockets {
    std::unordered_map<uint32_t, int64_t> userdata;
    bool GetConnectionInfo(uint32_t handle, SteamNetConnectionInfo_t*) { return handle > 0; }
    bool SetConnectionUserData(uint32_t handle, int64_t id) { userdata[handle] = id; return handle > 0; }
    int64_t GetConnectionUserData(uint32_t handle) { return userdata.contains(handle) ? userdata[handle] : -1; }
};
ISteamNetworkingSockets sockets;
ISteamNetworkingSockets* SteamNetworkingSockets() { return &sockets; }
namespace lightnerx::network { bool isNetworkInitialized() { return true; } }
void check(bool ok, const char* text) { if (!ok) throw std::runtime_error(text); }
'''
test = r'''
int main() {
    using namespace lightnerx::network;
    try {
        ConnectionManager manager(0, std::numeric_limits<int>::max());
        for (uint32_t handle = 1; handle <= 65535; ++handle) {
            auto id = manager.add(handle);
            check(id && *id == handle, "ID domain allocation");
        }
        check(!manager.add(65536), "LX-04: exhausted ID domain must reject add");
        size_t visits = 0;
        manager.forEach([&](uint16_t id, uint32_t handle) {
            check(++visits <= 65535 && id == handle, "LX-04: full-domain enumeration must terminate");
        });
        check(visits == 65535, "full-domain visit count");
        manager.remove(65535);
        auto last = manager.add(65536);
        check(last && *last == 65535, "LX-04: largest ID recycling");
        check(!manager.getClientId(65535), "removed handle lookup");
        check(manager.getClientId(65536) == last, "recycled handle lookup");
        manager.forEach([&](uint16_t id, uint32_t) { manager.remove(id); });
        check(manager.getOnlineCount() == 0, "remove all during iteration");
        check(manager.add(70000).value_or(0) == 1, "reuse lowest free ID");

        ConnectionManager growing(1, 65535);
        check(growing.add(1).has_value(), "seed reentrant iteration");
        visits = 0;
        growing.forEach([&](uint16_t id, uint32_t) {
            ++visits;
            if (id == 1) for (uint32_t h = 2; h <= 1000; ++h) check(growing.add(h).has_value(), "grow from callback");
        });
        check(visits > 0 && visits < 1000 && growing.getOnlineCount() == 1000, "LX-14: reentrant growth stays bounded");
        std::puts("ConnectionManager full-domain regressions passed");
    } catch (const std::exception& e) {
        std::fprintf(stderr, "%s\n", e.what());
        return 1;
    }
}
'''
with tempfile.TemporaryDirectory(prefix="lightnerx-id-domain-") as directory:
    cpp = Path(directory) / "domain.cpp"
    executable = Path(directory) / "domain"
    cpp.write_text(preamble + source + test)
    subprocess.run([args.compiler, "-std=c++26", *shlex.split(args.flags), "-fno-sanitize-recover=all", str(cpp), "-o", str(executable)], check=True)
    subprocess.run([str(executable)], check=True, timeout=30)
