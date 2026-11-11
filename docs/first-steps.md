# Primeiros passos

Este guia mostra o fluxo essencial para montar um servidor e um cliente. Os exemplos usam os componentes do LightnerX diretamente e deixam o modelo de execução sob responsabilidade da aplicação. Os dois programas ilustram aplicações consumidoras separadas; a biblioteca continua sendo uma biblioteca estática.

## Servidor

O servidor recebe bytes dos clientes, encaminha esses bytes ao `MessageInterpreter` e recebe mensagens interpretadas por meio de um callback.

```cpp
#include "steam/steamnetworkingtypes.h"

import std;

import lightnerx.communication.intention;
import lightnerx.communication.message;
import lightnerx.communication.notification;
import lightnerx.network.connection_manager;
import lightnerx.network.server;

int main() {
    lightnerx::network::Server server;
    lightnerx::network::ConnectionManager connections{10, 10};
    lightnerx::communication::MessageInterpreter interpreter;
    lightnerx::communication::NotificationRegistry registry;

    registry.registerId(47);

    server.addClientConnectedCallback(
        [&connections, &server](HSteamNetConnection connection) {
            if (!connections.add(connection)) {
                const auto disconnected = server.disconnectConnection(connection, false);
                if (!disconnected) {
                    std::println("Falha ao desconectar: {}", disconnected.error().message());
                }
            }
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
        [](lightnerx::communication::Message message, std::uint16_t clientId) {
            std::println("Mensagem recebida do cliente {}", clientId);
            std::visit([](const auto& header) {
                using Header = std::remove_cvref_t<decltype(header)>;
                if constexpr (std::is_same_v<Header, lightnerx::communication::Intention>) {
                    std::println("Intenção {}, correlação {}",
                                 header.mbt.type, header.correlationId);
                } else if constexpr (std::is_same_v<Header, lightnerx::communication::Notification>) {
                    std::println("Notificação {}", header.mbt.type);
                }
            }, message.header);
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

As lambdas capturam os componentes por referência para conectá-los. Mantenha esses objetos vivos enquanto seus callbacks puderem ser executados. O `payload` recebido aponta para os bytes da mensagem de rede; copie os bytes durante o callback se precisar usá-los depois.

## Cliente

O cliente encaminha bytes recebidos do servidor ao método `authorityInterpret`.

```cpp
#include "steam/steamnetworkingtypes.h"

import std;

import lightnerx.communication.event;
import lightnerx.communication.message;
import lightnerx.communication.response;
import lightnerx.network.client;

int main() {
    lightnerx::network::Client client;
    lightnerx::communication::MessageInterpreter interpreter;

    client.addServerReceivedMessageCallback(
        [&interpreter](std::span<const std::byte> bytes) {
            interpreter.authorityInterpret(bytes);
        }
    );

    interpreter.addAuthorityInterpretedMessageCallback(
        [](lightnerx::communication::Message message) {
            std::visit([](const auto& header) {
                using Header = std::remove_cvref_t<decltype(header)>;
                if constexpr (std::is_same_v<Header, lightnerx::communication::Event>) {
                    std::println("Evento {}", header.mbt.type);
                } else if constexpr (
                    std::is_same_v<Header, lightnerx::communication::Response> ||
                    std::is_same_v<Header, lightnerx::communication::UnsuccessfulResponse>
                ) {
                    std::println("Resposta à intenção {}: status {}",
                                 header.correlationId, header.status);
                    if constexpr (std::is_same_v<Header, lightnerx::communication::UnsuccessfulResponse>) {
                        std::println("Motivo {}", header.reasonCode);
                    }
                }
            }, message.header);
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

Os callbacks de recebimento executam durante `update()`, e os de interpretação executam na thread que chama o interpretador. Ao compartilhar componentes entre threads, organize a sincronização na aplicação. Para o batcher, serialize as chamadas de inicialização, enfileiramento e flush, que operam sobre a mesma fila.

## Envio em lote

Com a rede inicializada por `Server::start()`, `Client::start()` ou `lightnerx::network::networkInit()`, reserve espaço no batcher antes do primeiro enfileiramento. Importe `lightnerx.network.message_batch` e chame:

```cpp
lightnerx::network::initMessagesBatcher(64);
```

O destino é definido por `lightnerx::network::Target`. A função a seguir cria um `Event`, encaminha-o para um grupo e envia a fila. Ela pressupõe a inicialização acima e devolve o resultado do enfileiramento ou do flush ao chamador. O mesmo fluxo vale para os outros conversores de mensagem.

```cpp
#include "steam/steamnetworkingtypes.h"

import std;

import lightnerx.communication.event;
import lightnerx.communication.message;
import lightnerx.network.connection_manager;
import lightnerx.network.message_batch;
import lightnerx.network.target;

std::expected<void, std::error_code> sendEventToGroup(
    std::span<const std::byte> payload,
    std::vector<HSteamNetConnection> receivers,
    const lightnerx::network::ConnectionManager& connections
) {
    const auto totalSize = sizeof(lightnerx::communication::Event) + payload.size();
    if (totalSize > 0x7fff) {
        return std::unexpected(std::make_error_code(std::errc::message_size));
    }

    const lightnerx::communication::Event event{
        static_cast<std::uint16_t>(totalSize << 1),
        47
    };

    auto outbound = lightnerx::communication::event2OutboundMessage(
        event,
        payload,
        lightnerx::network::Target::group(std::move(receivers))
    );

    const std::span<const std::byte> bytes{
        static_cast<const std::byte*>(outbound.message->m_pData),
        static_cast<std::size_t>(outbound.message->m_cbSize)
    };

    const auto queued = lightnerx::network::queueMessage(
        bytes,
        std::move(outbound.target)
    );
    outbound.message->Release();

    if (!queued) {
        return queued;
    }

    return lightnerx::network::flushAll(connections);
}
```

`queueMessage` copia os bytes para uma mensagem interna do batcher. Por isso, a mensagem criada pelo conversor continua sendo de propriedade do chamador e deve receber `Release()` depois que `queueMessage` terminar, inclusive se o enfileiramento retornar um erro.

`std::move(outbound.target)` transfere somente o destino. Trate esse destino como consumido e crie outro `Target` para um novo envio. O ponteiro `outbound.message` permanece válido até `Release()`; após essa chamada, nem o ponteiro nem o span `bytes` devem ser acessados. O cabeçalho e o payload fornecidos ao conversor continuam pertencendo à aplicação.

Com handles obtidos pelo cliente ou pelos callbacks de conexão do servidor, os destinos disponíveis são:

```cpp
auto oneClient = lightnerx::network::Target::single(connection);
auto selectedClients = lightnerx::network::Target::group({firstConnection, secondConnection});
auto everyone = lightnerx::network::Target::broadcast();
auto everyoneElse = lightnerx::network::Target::broadcast({connectionToSkip});
```

Essas chamadas apenas descrevem destinos. Você pode enfileirar várias mensagens e executar `flushAll(connections)` ao final de uma etapa da aplicação; o flush imediato mostrado na função é uma escolha do exemplo.

## Próximos passos

- Consulte [Comunicação e protocolo INER](lightnerx.communication.md) para entender os cabeçalhos, destinos e fluxos de envio.
- Consulte [Referência da API](api.md) para os componentes de rede, mensagens, `Target` e batcher.
