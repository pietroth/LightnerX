# Primeiros passos

Este guia mostra o fluxo essencial para montar um servidor e um cliente. Os exemplos usam os componentes do LightnerX diretamente e deixam o modelo de execução sob responsabilidade da aplicação.

## Servidor

O servidor recebe bytes dos clientes, encaminha esses bytes ao `MessageInterpreter` e recebe mensagens interpretadas por meio de um callback.

```cpp
#include "steam/steamnetworkingtypes.h"

#include <cstddef>
#include <cstdint>
#include <print>
#include <span>

import communication.message;
import communication.notification;
import network.connection_manager;
import network.server;

int main() {
    network::Server server;
    network::ConnectionManager connections{10, 10};
    communication::MessageInterpreter interpreter;
    communication::NotificationRegistry registry;

    registry.registerId(47);

    server.addClientConnectedCallback(
        [&connections](HSteamNetConnection connection) {
            connections.add(connection);
        }
    );

    server.addClientDisconnectedCallback(
        [&connections](HSteamNetConnection connection) {
            if (const auto id = connections.getClientId(connection)) {
                connections.remove(*id);
            }
        }
    );

    server.addClientReceivedMessageCallback(
        [&interpreter, &registry, &connections](
            std::span<const std::byte> bytes,
            HSteamNetConnection connection
        ) {
            interpreter.obedientInterpret(
                bytes,
                registry,
                connection,
                connections
            );
        }
    );

    interpreter.addObedientInterpretedMessageCallback(
        [](communication::Message message, uint16_t clientId) {
            std::println("Mensagem recebida do cliente {}", clientId);
            // Use std::visit(message.header) para examinar o tipo.
        }
    );

    const auto started = server.start(45333, 2500);
    if (!started) {
        std::println("Falha ao iniciar: {}", started.error().message());
        return 1;
    }

    while (true) {
        if (const auto updated = server.update(); !updated) {
            std::println("Falha na atualização: {}", updated.error().message());
            break;
        }
    }

    server.close();
}
```

O callback de conexão deve atualizar o `ConnectionManager`, porque o interpretador usa esse objeto para associar a conexão ao ID entregue ao callback de mensagem interpretada.

## Cliente

O cliente encaminha bytes recebidos do servidor ao método `authorityInterpret`.

```cpp
#include "steam/steamnetworkingtypes.h"

#include <cstddef>
#include <print>
#include <span>

import communication.message;
import network.client;

int main() {
    network::Client client;
    communication::MessageInterpreter interpreter;

    client.addServerReceivedMessageCallback(
        [&interpreter](std::span<const std::byte> bytes) {
            interpreter.authorityInterpret(bytes);
        }
    );

    if (const auto started = client.start(); !started) {
        std::println("Falha ao iniciar: {}", started.error().message());
        return 1;
    }

    if (const auto connected = client.connect("127.0.0.1", 45333);
        !connected) {
        std::println("Falha ao conectar: {}", connected.error().message());
        return 1;
    }

    interpreter.addAuthorityInterpretedMessageCallback(
        [](communication::Message message) {
            std::println("Mensagem recebida do servidor");
            // Use std::visit(message.header) para examinar o tipo.
        }
    );

    while (true) {
        if (const auto updated = client.update(1); !updated) {
            std::println("Falha na atualização: {}", updated.error().message());
            break;
        }
    }
}
```

`connect()` inicia a conexão e `update()` processa os callbacks da rede e as mensagens disponíveis. `isConnected()` pode ser usado para consultar o estado observado pelo cliente.

## Modelo de execução

Os métodos `server.update()` e `client.update()` são chamados pela aplicação. Eles podem ser executados na thread principal, em uma thread dedicada ou dentro do loop de execução já existente no projeto consumidor.

A biblioteca não exige uma quantidade específica de threads nem um intervalo fixo entre atualizações.

## Próximos passos

- Consulte [Comunicação e protocolo INER](communication.md) para entender os cabeçalhos e os papéis semânticos.
- Consulte [Referência da API](api.md) para os componentes de rede, mensagens e batcher.
- Use `notification2OutboundMessage`, `event2OutboundMessage`, `intention2OutboundMessage`, `response2OutboundMessage` e `unsuccessfulResponse2OutboundMessage` para montar mensagens de saída.
