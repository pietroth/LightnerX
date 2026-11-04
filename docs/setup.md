# Instalação e integração

O LightnerX é distribuído como código-fonte modular C++. A integração consiste em adicionar os módulos `.cppm` ao target da aplicação e vinculá-lo ao GameNetworkingSockets usado pelos componentes de rede.

## Requisitos

- CMake 3.28 ou posterior;
- compilador C++ com suporte a módulos C++ e às bibliotecas usadas pela implementação;
- padrão C++26;
- uma ferramenta de build, como Ninja ou GNU Make;
- GameNetworkingSockets;
- OpenSSL e Protobuf, usados pela configuração padrão do GameNetworkingSockets;
- Threads do sistema.

O projeto utiliza o GameNetworkingSockets como submódulo em `external/GameNetworkingSockets`. Ao obter o código-fonte, inicialize os submódulos antes de configurar a aplicação:

```bash
git submodule update --init --recursive
```

## Configuração do compilador

O padrão do projeto é definido no CMake:

```cmake
set(CMAKE_CXX_STANDARD 26)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
```

O compilador precisa conseguir compilar os módulos C++ listados pelo target. A aplicação também deve ter acesso aos headers do GameNetworkingSockets, fornecidos pela target `GameNetworkingSockets`.

## Integração por código-fonte

O exemplo abaixo mostra a organização necessária para adicionar os módulos do LightnerX a um executável próprio. Ajuste `LIGHTNERX_ROOT` para o caminho do checkout da biblioteca.

```cmake
cmake_minimum_required(VERSION 3.28)

project(minha_aplicacao LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 26)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(LIGHTNERX_ROOT "/caminho/para/LightnerX" CACHE PATH "Raiz do LightnerX")

add_subdirectory(
    "${LIGHTNERX_ROOT}/external/GameNetworkingSockets"
    "${CMAKE_BINARY_DIR}/GameNetworkingSockets"
)

add_executable(minha_aplicacao app.cpp)

target_sources(minha_aplicacao
    PRIVATE
        FILE_SET CXX_MODULES
        BASE_DIRS "${LIGHTNERX_ROOT}"
        FILES
            "${LIGHTNERX_ROOT}/communication/base_protocol.cppm"
            "${LIGHTNERX_ROOT}/communication/mbt.cppm"
            "${LIGHTNERX_ROOT}/communication/event.cppm"
            "${LIGHTNERX_ROOT}/communication/intention.cppm"
            "${LIGHTNERX_ROOT}/communication/notification.cppm"
            "${LIGHTNERX_ROOT}/communication/response.cppm"
            "${LIGHTNERX_ROOT}/communication/message.cppm"
            "${LIGHTNERX_ROOT}/network/network_manager.cppm"
            "${LIGHTNERX_ROOT}/network/connection_manager.cppm"
            "${LIGHTNERX_ROOT}/network/transport.cppm"
            "${LIGHTNERX_ROOT}/network/message_batch.cppm"
            "${LIGHTNERX_ROOT}/network/server.cppm"
            "${LIGHTNERX_ROOT}/network/client.cppm"
)

target_include_directories(minha_aplicacao PRIVATE "${LIGHTNERX_ROOT}")
target_link_libraries(minha_aplicacao PRIVATE GameNetworkingSockets)
```

Os módulos são importados no código C++ com `import`, por exemplo:

```cpp
import communication.message;
import network.client;
```

Os headers `steam/...` continuam sendo incluídos normalmente quando a aplicação precisa usar tipos do GameNetworkingSockets, como `HSteamNetConnection`.

## Configuração e compilação

Em um projeto consumidor que contenha o CMake acima, a configuração e a compilação podem ser feitas com:

```bash
cmake -S . -B build -G Ninja
cmake --build build
```

A configuração atual do projeto LightnerX também desativa os testes e exemplos do GameNetworkingSockets ao adicioná-lo como subdiretório. O consumidor pode ajustar essas opções antes de adicionar a dependência quando precisar de outra configuração.

## Dependências do transporte

O módulo `network` usa as funções do GameNetworkingSockets para inicializar a rede, abrir sockets de escuta, conectar clientes e enviar mensagens. O transporte padrão utilizado por `sendToConnection` é confiável.

O LightnerX não substitui as dependências do transporte nem define uma implementação própria de socket. Ele organiza o uso dessas dependências e expõe componentes que podem ser conectados à aplicação.
