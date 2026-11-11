# LightnerX

LightnerX é uma biblioteca modular estática em C++ para construir aplicações que se comunicam usando o protocolo semântico INER. O projeto CMake fornece a target `LightnerX`; ele não produz nem exige um executável próprio. A biblioteca oferece tipos para representar mensagens, interpretadores, callbacks, gerenciamento de conexões e uma camada de transporte baseada em GameNetworkingSockets.

O INER define o significado das mensagens trocadas entre uma Autoridade e um Obediente. Ele não define sockets, conexões ou um modelo de execução. Esses recursos são fornecidos pelos componentes de rede do LightnerX e podem ser conectados à aplicação conforme a necessidade do desenvolvedor.

## Biblioteca, não framework

O LightnerX fornece componentes reutilizáveis. A aplicação consumidora decide:

- quando inicializar e encerrar os componentes;
- como executar `update()`;
- quais threads utilizar;
- como organizar o ciclo de vida das conexões;
- quais módulos usar ou substituir por implementações próprias.

É possível, por exemplo, entregar bytes de um transporte próprio a `lightnerx::communication::MessageInterpreter`. Para interpretar mensagens do Obediente, `obedientInterpret` recebe um `lightnerx::network::ConnectionManager`; uma aplicação que gerencie conexões por conta própria deve manter nesse objeto a associação entre handles e IDs utilizada pelo interpretador.

## Conceitos principais

- **Autoridade:** controla estados internos e recebe intenções do Obediente.
- **Obediente:** envia intenções à Autoridade e recebe eventos e respostas.
- **Event:** acontecimento comunicado pela Autoridade.
- **Response:** resultado de uma intenção enviada pelo Obediente.
- **Intention:** solicitação de ação enviada à Autoridade.
- **Notification:** informação enviada à Autoridade sem solicitação de alteração de estado.

No LightnerX, a aplicação conecta os componentes por callbacks: a rede entrega bytes e o `MessageInterpreter` interpreta esses bytes segundo a direção da comunicação. No envio, `lightnerx::network::Target` permite selecionar uma conexão, um grupo ou um broadcast com ou sem exceções.

## Navegação

- [Configuração](docs/setup.md)
- [Primeiros passos](docs/first-steps.md)
- [Comunicação e protocolo INER](docs/communication.md)
- [Referência da API](docs/api.md)

## Exemplo de servidor

Este exemplo recebe notificações do cliente e envia o texto `Hello, World!` em eventos para as conexões registradas. Os trechos abaixo compõem o programa completo ao final do README; as instruções ficam dentro da função principal, após os imports apresentados no código completo.

### Criar os componentes

O gerenciador associa handles aos IDs internos dos clientes. Registre o ID `47` para que o interpretador reconheça as notificações enviadas pelo cliente deste exemplo.

```cpp
using namespace std::chrono_literals;
namespace network = lightnerx::network;
namespace communication = lightnerx::communication;

network::Server server;
network::ConnectionManager connections{30, 30};
communication::MessageInterpreter interpreter;
communication::NotificationRegistry notifications;
notifications.registerId(47);
```

### Acompanhar as conexões

Os callbacks mantêm o gerenciador atualizado. Se o registro de uma conexão falhar, o servidor solicita sua desconexão.

```cpp
server.addClientConnectedCallback([&](HSteamNetConnection connection) {
    if (!connections.add(connection)) {
        if (const auto result = server.disconnectConnection(connection, false); !result) {
            std::println("Falha ao desconectar: {}", result.error().message());
        }
    }
});

server.addClientDisconnectedCallback([&](HSteamNetConnection connection) {
    if (const auto id = connections.getClientId(connection)) {
        connections.remove(*id);
    }
});
```

### Receber mensagens

Encaminhe os bytes recebidos a `obedientInterpret`. O callback do interpretador entrega a mensagem e o ID interno do cliente. Neste exemplo, o payload é texto e é lido durante o callback.

```cpp
server.addClientReceivedMessageCallback(
    [&](std::span<const std::byte> bytes, HSteamNetConnection connection) {
        interpreter.obedientInterpret(bytes, notifications, connection, connections);
    }
);

interpreter.addObedientInterpretedMessageCallback(
    [](communication::Message message, std::uint16_t clientId) {
        const std::string_view text{
            reinterpret_cast<const char*>(message.payload.data()), message.payload.size()
        };
        std::println("Cliente {}: {}", clientId, text);
    }
);
```

### Iniciar o servidor

Inicie a escuta na porta `41333`, com timeout de conexão estabelecida de `3000` ms, e reserve uma posição para a mensagem pendente do batcher.

```cpp
if (const auto result = server.start(41333, 3000); !result) {
    std::println("Falha ao iniciar: {}", result.error().message());
    return 1;
}
network::initMessagesBatcher(1);
```

### Preparar um evento

Converta o texto em bytes e inclua cabeçalho e payload no tamanho total. Para `Event`, o tamanho é deslocado um bit à esquerda. O tipo `990` é escolhido pela aplicação.

```cpp
const std::string_view greeting = "Hello, World!";
const auto payload = std::as_bytes(std::span{greeting});
const communication::Event event{
    // O bit menos significativo distingue Event de Response.
    static_cast<std::uint16_t>((sizeof(communication::Event) + payload.size()) << 1),
    990
};
```

### Atualizar e enviar

`update()` processa a rede. O conversor cria um `OutboundMessage`, que é entregue diretamente a `queueMessage`. `Target::broadcast()` seleciona as conexões registradas no momento do flush.

```cpp
while (true) {
    if (const auto result = server.update(); !result) {
        std::println("Falha ao atualizar: {}", result.error().message());
        break;
    }
    if (const auto result = network::queueMessage(communication::event2OutboundMessage(
            event, payload, network::Target::broadcast())); !result) {
        std::println("Falha ao enfileirar: {}", result.error().message());
        break;
    }
    if (const auto result = network::flushAll(connections); !result) {
        std::println("Falha ao enviar: {}", result.error().message());
        break;
    }
    std::this_thread::sleep_for(3s);
}
```

`queueMessage` assume a propriedade da mensagem, inclusive quando retorna erro; a aplicação não chama `Release()` depois. `flushAll` processa a fila e libera as mensagens. O intervalo de três segundos é uma escolha deste exemplo; a aplicação define seu próprio ritmo de atualização.

### Encerrar o servidor

Após sair do loop por erro, feche o servidor. O retorno `1` sinaliza a saída por falha neste exemplo.

```cpp
server.close();
return 1;
```

## Exemplo de cliente

O cliente conecta ao servidor em `127.0.0.1:41333`, imprime os eventos recebidos e envia uma notificação com o mesmo texto. Execute servidor e cliente em processos separados.

### Criar os componentes

O gerenciador vazio atende à assinatura de `flushAll`. O envio individual ao servidor usa diretamente o handle de conexão do cliente.

```cpp
using namespace std::chrono_literals;
namespace network = lightnerx::network;
namespace communication = lightnerx::communication;

network::Client client;
network::ConnectionManager connections{0, 0}; // Exigido pelo flush, sem broadcast.
communication::MessageInterpreter interpreter;
```

### Receber mensagens do servidor

Encaminhe os bytes a `authorityInterpret`, que interpreta mensagens da Autoridade. O callback abaixo imprime o payload textual combinado por estes exemplos e trata payloads vazios.

```cpp
client.addServerReceivedMessageCallback([&](std::span<const std::byte> bytes) {
    interpreter.authorityInterpret(bytes);
});

interpreter.addAuthorityInterpretedMessageCallback([](communication::Message message) {
    const auto text = message.payload.empty() ? std::string_view{} : std::string_view{
        reinterpret_cast<const char*>(message.payload.data()), message.payload.size()
    };
    std::println("Servidor: {}", text);
});
```

Os callbacks usam os bytes durante a chamada. Para conservar um payload recebido, copie seus bytes para armazenamento da aplicação antes de retornar.

### Iniciar e conectar

Inicialize o cliente, solicite a conexão com o servidor e reserve espaço no batcher. A conexão será acompanhada nas chamadas de `update()`.

```cpp
if (const auto result = client.start(); !result) {
    std::println("Falha ao iniciar: {}", result.error().message());
    return 1;
}
if (const auto result = client.connect("127.0.0.1", 41333); !result) {
    std::println("Falha ao conectar: {}", result.error().message());
    return 1;
}
network::initMessagesBatcher(1);
```

### Preparar uma notificação

`Notification` usa o tamanho total diretamente, sem deslocamento de bits. O ID `47` corresponde ao registro feito no servidor.

```cpp
const std::string_view greeting = "Hello, World!";
const auto payload = std::as_bytes(std::span{greeting});
const communication::Notification notification{
    static_cast<std::uint16_t>(sizeof(communication::Notification) + payload.size()),
    47 // O servidor deve registrar este ID no NotificationRegistry.
};
```

### Atualizar e enviar ao servidor

Processe até 32 mensagens por atualização e envie quando `isConnected()` indicar a conexão estabelecida. `Target::single` seleciona o handle do servidor.

```cpp
while (true) {
    if (const auto result = client.update(32); !result) {
        std::println("Falha ao atualizar: {}", result.error().message());
        break;
    }
    if (client.isConnected()) {
        if (const auto result = network::queueMessage(communication::notification2OutboundMessage(
                notification, payload, network::Target::single(client.getServerHConn()))); !result) {
            std::println("Falha ao enfileirar: {}", result.error().message());
            break;
        }
        if (const auto result = network::flushAll(connections); !result) {
            std::println("Falha ao enviar: {}", result.error().message());
            break;
        }
    }
    std::this_thread::sleep_for(3000ms);
}
```

### Encerrar o cliente

Ao sair por erro, o destrutor de `Client` fecha a conexão e encerra a rede. O retorno `1` indica a falha que interrompeu o exemplo.

```cpp
return 1; // O destrutor do Client encerra a conexão e a rede.
```

## Códigos completos

Os programas abaixo reúnem os trechos apresentados. O payload textual, os IDs, o loop e os intervalos são escolhas da aplicação consumidora.

### Servidor completo

```cpp
#include <steam/steamnetworkingtypes.h>

import std;

import lightnerx.network.server;
import lightnerx.network.connection_manager;
import lightnerx.network.message_batch;
import lightnerx.network.target;
import lightnerx.communication.message;
import lightnerx.communication.notification;
import lightnerx.communication.event;

int main() {
    using namespace std::chrono_literals;
    namespace network = lightnerx::network;
    namespace communication = lightnerx::communication;

    network::Server server;
    network::ConnectionManager connections{30, 30};
    communication::MessageInterpreter interpreter;
    communication::NotificationRegistry notifications;
    notifications.registerId(47);

    server.addClientConnectedCallback([&](HSteamNetConnection connection) {
        if (!connections.add(connection)) {
            if (const auto result = server.disconnectConnection(connection, false); !result) {
                std::println("Falha ao desconectar: {}", result.error().message());
            }
        }
    });

    server.addClientDisconnectedCallback([&](HSteamNetConnection connection) {
        if (const auto id = connections.getClientId(connection)) {
            connections.remove(*id);
        }
    });

    server.addClientReceivedMessageCallback(
        [&](std::span<const std::byte> bytes, HSteamNetConnection connection) {
            interpreter.obedientInterpret(bytes, notifications, connection, connections);
        }
    );

    interpreter.addObedientInterpretedMessageCallback(
        [](communication::Message message, std::uint16_t clientId) {
            const std::string_view text{
                reinterpret_cast<const char*>(message.payload.data()), message.payload.size()
            };
            std::println("Cliente {}: {}", clientId, text);
        }
    );

    if (const auto result = server.start(41333, 3000); !result) {
        std::println("Falha ao iniciar: {}", result.error().message());
        return 1;
    }
    network::initMessagesBatcher(1);

    const std::string_view greeting = "Hello, World!";
    const auto payload = std::as_bytes(std::span{greeting});
    const communication::Event event{
        // O bit menos significativo distingue Event de Response.
        static_cast<std::uint16_t>((sizeof(communication::Event) + payload.size()) << 1),
        990
    };

    while (true) {
        if (const auto result = server.update(); !result) {
            std::println("Falha ao atualizar: {}", result.error().message());
            break;
        }
        if (const auto result = network::queueMessage(communication::event2OutboundMessage(
                event, payload, network::Target::broadcast())); !result) {
            std::println("Falha ao enfileirar: {}", result.error().message());
            break;
        }
        if (const auto result = network::flushAll(connections); !result) {
            std::println("Falha ao enviar: {}", result.error().message());
            break;
        }
        std::this_thread::sleep_for(3s);
    }

    server.close();
    return 1;
}
```

### Cliente completo

```cpp
#include <steam/steamnetworkingtypes.h>

import std;

import lightnerx.network.client;
import lightnerx.network.connection_manager;
import lightnerx.network.message_batch;
import lightnerx.network.target;
import lightnerx.communication.message;
import lightnerx.communication.notification;

int main() {
    using namespace std::chrono_literals;
    namespace network = lightnerx::network;
    namespace communication = lightnerx::communication;

    network::Client client;
    network::ConnectionManager connections{0, 0}; // Exigido pelo flush, sem broadcast.
    communication::MessageInterpreter interpreter;

    client.addServerReceivedMessageCallback([&](std::span<const std::byte> bytes) {
        interpreter.authorityInterpret(bytes);
    });

    interpreter.addAuthorityInterpretedMessageCallback([](communication::Message message) {
        const auto text = message.payload.empty() ? std::string_view{} : std::string_view{
            reinterpret_cast<const char*>(message.payload.data()), message.payload.size()
        };
        std::println("Servidor: {}", text);
    });

    if (const auto result = client.start(); !result) {
        std::println("Falha ao iniciar: {}", result.error().message());
        return 1;
    }
    if (const auto result = client.connect("127.0.0.1", 41333); !result) {
        std::println("Falha ao conectar: {}", result.error().message());
        return 1;
    }
    network::initMessagesBatcher(1);

    const std::string_view greeting = "Hello, World!";
    const auto payload = std::as_bytes(std::span{greeting});
    const communication::Notification notification{
        static_cast<std::uint16_t>(sizeof(communication::Notification) + payload.size()),
        47 // O servidor deve registrar este ID no NotificationRegistry.
    };

    while (true) {
        if (const auto result = client.update(32); !result) {
            std::println("Falha ao atualizar: {}", result.error().message());
            break;
        }
        if (client.isConnected()) {
            if (const auto result = network::queueMessage(communication::notification2OutboundMessage(
                    notification, payload, network::Target::single(client.getServerHConn()))); !result) {
                std::println("Falha ao enfileirar: {}", result.error().message());
                break;
            }
            if (const auto result = network::flushAll(connections); !result) {
                std::println("Falha ao enviar: {}", result.error().message());
                break;
            }
        }
        std::this_thread::sleep_for(3000ms);
    }

    return 1; // O destrutor do Client encerra a conexão e a rede.
}
```
