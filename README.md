# LightnerX

LightnerX é uma biblioteca modular em C++ para construir aplicações que se comunicam usando o protocolo semântico INER. Ela oferece tipos para representar mensagens, interpretadores, callbacks, gerenciamento de conexões e uma camada de transporte baseada em GameNetworkingSockets.

O INER define o significado das mensagens trocadas entre uma Autoridade e um Obediente. Ele não define sockets, conexões ou um modelo de execução. Esses recursos são fornecidos pelos componentes de rede do LightnerX e podem ser conectados à aplicação conforme a necessidade do desenvolvedor.

## Biblioteca, não framework

O LightnerX fornece componentes reutilizáveis. A aplicação consumidora decide:

- quando inicializar e encerrar os componentes;
- como executar `update()`;
- quais threads utilizar;
- como organizar o ciclo de vida das conexões;
- quais módulos usar ou substituir por implementações próprias.

É possível, por exemplo, usar `communication::MessageInterpreter` com um gerenciador de conexões próprio, desde que ele forneça as informações necessárias pelos contratos das funções utilizadas.

## Conceitos principais

- **Autoridade:** controla estados internos e recebe intenções do Obediente.
- **Obediente:** envia intenções à Autoridade e recebe eventos e respostas.
- **Event:** acontecimento comunicado pela Autoridade.
- **Response:** resultado de uma intenção enviada pelo Obediente.
- **Intention:** solicitação de ação enviada à Autoridade.
- **Notification:** informação enviada à Autoridade sem solicitação de alteração de estado.

No LightnerX, os componentes de rede e comunicação são independentes. A rede entrega bytes; o `MessageInterpreter` interpreta esses bytes segundo a direção da comunicação.

## Navegação

- [Instalação e integração](docs/setup.md)
- [Primeiros passos](docs/first-steps.md)
- [Comunicação e protocolo INER](docs/communication.md)
- [Referência da API](docs/api.md)
