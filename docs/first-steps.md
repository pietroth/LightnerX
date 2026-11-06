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
        [](communication::Message message, std::uint16_t clientId) {
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

## Envio em lote

Antes de enfileirar mensagens, inicialize o batcher com a capacidade que a aplicação deseja manter pendente:

```cpp
network::initMessagesBatcher(64);
```

O destino é definido por `network::Target`. O exemplo a seguir cria um `Event`, encaminha-o para um grupo e envia a fila. O mesmo fluxo vale para os outros conversores de mensagem.

```cpp
#include "steam/steamnetworkingtypes.h"

#include <cstddef>
#include <cstdint>
#include <span>
#include <utility>
#include <vector>

import communication.event;
import communication.message;
import network.connection_manager;
import network.message_batch;
import network.target;

void sendEventToGroup(
    std::span<const std::byte> payload,
    std::vector<HSteamNetConnection> receivers,
    const network::ConnectionManager& connections
) {
    const auto totalSize = sizeof(communication::Event) + payload.size();
    const communication::Event event{
        static_cast<std::uint16_t>(totalSize << 1),
        47
    };

    auto outbound = communication::event2OutboundMessage(
        event,
        payload,
        network::Target::group(std::move(receivers))
    );

    const std::span<const std::byte> bytes{
        static_cast<const std::byte*>(outbound.message->m_pData),
        static_cast<std::size_t>(outbound.message->m_cbSize)
    };

    const auto queued = network::queueMessage(
        bytes,
        std::move(outbound.target)
    );
    outbound.message->Release();

    if (!queued) {
        return;
    }

    const auto flushed = network::flushAll(connections);
    if (!flushed) {
        return;
    }
}
```

`queueMessage` copia os bytes para uma mensagem interna do batcher. Por isso, a mensagem criada pelo conversor continua sendo de propriedade do chamador e deve receber `Release()` depois que `queueMessage` terminar, inclusive se o enfileiramento retornar um erro.

`std::move(outbound.target)` transfere o destino para a mensagem enfileirada. Após a transferência, a aplicação não deve reutilizar `outbound.target`; o ponteiro `outbound.message` permanece válido até a chamada de `Release()`.

Os destinos disponíveis são:

```cpp
auto oneClient = network::Target::single(connection);
auto selectedClients = network::Target::group({firstConnection, secondConnection});
auto everyoneElse = network::Target::broadcast({connectionToSkip});
```

## Próximos passos

- Consulte [Comunicação e protocolo INER](communication.md) para entender os cabeçalhos, destinos e fluxos de envio.
- Consulte [Referência da API](api.md) para os componentes de rede, mensagens, `Target` e batcher.
