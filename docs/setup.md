# Setup

LightnerX is a static C++ library. The project provides the `LightnerX` target and does not have its own executable.

## GameNetworkingSockets

GameNetworkingSockets is maintained as a submodule in `external/GameNetworkingSockets`. After obtaining the repository, run the following from the LightnerX root directory:

```bash
git submodule update --init --recursive
```

This command retrieves GameNetworkingSockets and its recursive submodules in the state expected by LightnerX.
