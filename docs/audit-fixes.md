# Correções da auditoria LightnerX

Revisão de origem: `8e5a352`. Dependência GNS: `d534c19`. Auditoria lida:
`LightnerX-audit.md`. Trabalho validado em 12/11/2026 com GCC/libstdc++ 15.2,
CMake 4.2.3 e Ninja 1.13.2, em Linux.

Os 14 itens foram confrontados com a revisão de origem e receberam correções.
C++26, módulos, GameNetworkingSockets, representação binária INER e
`queueMessage(OutboundMessage)` com `Target` foram preservados. Não houve edição
de terceiros; a alteração preexistente em
`external/GameNetworkingSockets/examples/trivial_signaling_client.h` permanece.

## Resultado por item

Os nomes de testes abaixo são os sufixos dos testes CTest `lightnerx.*`.
Os caminhos de módulos sem prefixo estão em `network/`.

| LX | Causa | Correção implementada | Arquivos de implementação/documentação | Testes executados | Classificação | Pendências |
| --- | --- | --- | --- | --- | --- | --- |
| LX-01 | Wrappers destruíam GNS diretamente, deixando flag e ponteiros inconsistentes. | Encerramento central, ownership explícito, limpeza de membros e rollback de inicialização. | `network_manager.cppm`, `client.cppm`, `server.cppm`, `docs/api.md` | `lifecycle`, `lifecycle_faults`, `integration` | Corrigido e validado | Nenhuma funcional. |
| LX-02 | Consulta de identidade dependia de userdata em handle já fechado. | Mapa reverso local mantém a identidade até `remove()`, inclusive nos callbacks após o fechamento. | `connection_manager.cppm`, `server.cppm`, `docs/api.md` | `connection_manager`, `integration` com desconexões pelos dois lados | Corrigido e validado | Nenhuma. |
| LX-03 | `return false` construía `optional<uint16_t>` presente com valor zero. | Lookup ausente retorna `nullopt`; zero não é um ID cadastrado. | `connection_manager.cppm` | `connection_manager`, `messages` | Corrigido e validado | Nenhuma. |
| LX-04 | Último ID não era reciclado; contador e índice de 16 bits transbordavam. | Reciclagem de todos os IDs removidos, contador mais largo, domínio 1–65535 e iteração limitada com `size_t`. | `connection_manager.cppm`, `docs/api.md` | `connection_manager`: 70.000 ciclos; `connection_manager_domain`: domínio completo | Corrigido e validado | Domínio completo usa interface controlada, não 65.535 sockets reais simultâneos. |
| LX-05 | Cópia implícita duplicava ownership e ponteiros de callbacks. | Cópia e movimentação excluídas em Client/Server; Server também encerra recursos no destrutor. | `client.cppm`, `server.cppm`, `docs/api.md` | `lifecycle` e `consumer`, com verificações de tipos em compilação | Corrigido e validado | Código consumidor que copiava wrappers deve ser adaptado. |
| LX-06 | Desconexão local não limpava handle e estado. | Fechamento bem-sucedido invalida handle e limpa `connected`, permitindo reconexão. | `client.cppm`, `docs/api.md` | `lifecycle_faults`, `integration` | Corrigido e validado | Nenhuma. |
| LX-07 | Recebimento negativo era tratado como fila vazia. | Retorna `io_error`; cliente também fecha/limpa sua conexão inválida. | `client.cppm`, `server.cppm`, `docs/api.md` | `lifecycle_faults`, com retorno -1 nos dois caminhos | Corrigido e validado | Falha rara provocada por injeção controlada. |
| LX-08 | Cinco conversores desreferenciavam retorno nulo de `AllocateMessage`. | Verificação comum lança `bad_alloc`; tamanho irrepresentável lança `length_error` antes da alocação. | `communication/message.cppm`, `docs/api.md`, `docs/communication.md` | `message_faults`: cinco falhas de alocação e overflow; `messages`: GNS real | Corrigido e validado | Não foi necessário esgotar a memória do sistema. |
| LX-09 | C++26 e requisitos GNS não eram públicos no target; setup incompleto. | `cxx_std_26` e GNS públicos; configuração de `import std` e toolchain documentadas; consumidor independente. | `CMakeLists.txt`, `docs/setup.md`, `tests/consumer/` | `consumer`, compilação completa CMake/Ninja | Corrigido e validado | Outras toolchains não foram homologadas. |
| LX-10 | Dois módulos exportavam constantes distintas com o mesmo nome. | Uma declaração de `k_invalidConnection` no gerenciador global, reexportada por cliente e transporte. | `network_manager.cppm`, `client.cppm`, `transport.cppm` | `consumer`, `lifecycle` | Corrigido e validado | Nenhuma. |
| LX-11 | Instâncias competiam pelos callbacks e pela destruição global. | Um wrapper iniciado por processo; conflitos e repetição de start são rejeitados; shutdown respeita o owner. | `network_manager.cppm`, `client.cppm`, `server.cppm`, `README.md`, `docs/api.md`, `docs/first-steps.md` | `lifecycle`, `integration` | Corrigido e validado | Coexistência de vários wrappers não faz parte do contrato suportado. |
| LX-12 | Userdata global permitia interferência entre managers e cadastros duplicados. | Mapeamento bidirecional próprio, pertencimento local, rejeição de duplicatas e validação do handle GNS na admissão. | `connection_manager.cppm`, `README.md`, `docs/api.md` | `connection_manager`: duplicatas, managers independentes, userdata preservada | Corrigido e validado | Transporte externo não ganha suporte a IDs arbitrários; restrição documentada. |
| LX-13 | Parser ignorava divergência entre tamanho declarado e buffer. | Exige uma mensagem completa, tamanho decodificado igual ao span e header mínimo completo. | `communication/message.cppm`, `docs/api.md`, `docs/communication.md` | `messages`, `message_faults`: truncamentos, concatenação, tamanhos inválidos e limites 15/16 bits | Corrigido e validado | Nenhuma mudança de framing ou formato binário. |
| LX-14 | Exceções vazavam buffers; registro reentrante invalidava vetores; `forEach` terminava o processo. | RAII em recebimento e batching, callbacks em armazenamento estável, propagação de exceções, captura na fronteira GNS, bloqueio de update recursivo e fechamento adiado. | `client.cppm`, `server.cppm`, `connection_manager.cppm`, `network_manager.cppm`, `message_batch.cppm`, `communication/message.cppm`, `docs/api.md` | `messages`, `message_faults`, `lifecycle_faults`, `connection_manager`, `integration`; ASan e UBSan conforme limites abaixo | Corrigido, mas parcialmente validado | UBSan modular indisponível no GCC 15.2; checagem vptr incompatível com GNS sem RTTI. |

## Evidências e testes

A suíte normal passou nos oito testes, sem testes ignorados. Resultados locais:
`build/audit/test-results.xml` e `build/audit/Testing/Temporary/LastTest.log`.
A execução instrumentada final também passou nos oito testes, sem testes
ignorados e sem diagnósticos dos sanitizadores habilitados (53,47 segundos).
Resultados: `build/audit-sanitizers/test-results.xml` e
`build/audit-sanitizers/Testing/Temporary/LastTest.log`. A suíte normal levou
131,90 segundos, incluindo a compilação do consumidor. Os limites abaixo são
parte da validação, não uma garantia de ausência de todos os defeitos possíveis.
A classificação final é de 13 itens corrigidos e validados e LX-14 corrigido,
mas parcialmente validado pelas restrições de instrumentação explicitadas.

- Os executáveis `connection_manager`, `messages` e `lifecycle` importam os
  módulos reais e usam GNS real. Há testes de reinicialização, estados inválidos,
  identidade, serialização, ownership e callbacks.
- `integration` executa Client e Server em processos separados sobre UDP local.
  Testa três sessões, mensagens, reconexão, desconexão iniciada pelos dois lados,
  identidade após fechamento, exceção de admissão, registro reentrante de
  callbacks, update recursivo e encerramento/reinicialização do servidor.
- `consumer` configura um projeto pai independente, inclui headers Steam e
  importa módulos ligando somente o target `LightnerX`.
- Os três testes Python suplementares compilam corpos de produção sem as
  declarações de módulos. Interfaces controladas permitem esgotar os 65.535 IDs,
  devolver alocação nula e provocar erros de recebimento/liberação. Eles não
  substituem os testes dos módulos reais. Contadores verificam a liberação de
  mensagens, inclusive após uma parte do lote já ter sido processada.

Reproduções anteriores às correções incluíram: Client copiável; três mensagens
retidas após exceção; desconexão com `connected=true`; erro de recebimento
reportado como sucesso; inicialização global verdadeira com interface nula;
start simultâneo/repetido aceito; optional inválido com valor zero; duplicatas;
overflow de IDs; tamanho INER inválido entregue; alocação nula causando falha.
O consumidor original não encontrava o header Steam e compilava em C++17.
O teste de domínio também foi aplicado ao arquivo original de `8e5a352` e falhou
em `exhausted ID domain must reject add` (`build/audit/id-baseline.log`).

## Reprodução

```bash
cmake -S . -B build/audit -G Ninja \
  -DCMAKE_BUILD_TYPE=Debug -DLIGHTNERX_BUILD_TESTS=ON
cmake --build build/audit --parallel 2
ctest --test-dir build/audit --output-on-failure --output-junit test-results.xml -j 2

cmake -S . -B build/audit-sanitizers -G Ninja \
  -DCMAKE_BUILD_TYPE=Debug -DLIGHTNERX_BUILD_TESTS=ON \
  -DLIGHTNERX_SANITIZERS=address
cmake --build build/audit-sanitizers --parallel 2
ctest --test-dir build/audit-sanitizers --output-on-failure --output-junit test-results.xml -j 2
```

O teste UDP e LeakSanitizer precisam de permissões que o sandbox inicial não
oferecia. A execução final usa o ambiente autorizado fora dessa restrição.
O limite dos testes que compilam código foi ampliado para 180 segundos após um
timeout de compilação concorrente; os limites de runtime permanecem delimitados.

## Limites dos sanitizadores

Os módulos LightnerX e os consumidores foram compilados com ASan. Os três testes
suplementares usam ASan e UBSan, com `-fno-sanitize-recover=all`, para que qualquer
diagnóstico habilitado resulte em falha. A biblioteca padrão e o GNS não foram
instrumentados nem tiveram seus fontes alterados.

GCC 15.2 aborta com erro interno `cp/module.cc:10037` ao compilar estes módulos
com UBSan. Isso foi reproduzido tanto instrumentando `std` quanto deixando `std`
sem instrumentação; portanto não se declara cobertura UBSan dos módulos reais.

O GNS vendorizado usa `-fno-rtti`. Uma reprodução independente, contendo apenas
Init/CreatePollGroup/DestroyPollGroup/Kill de GNS, já produz o diagnóstico vptr
do UBSan, sem usar LightnerX. O mesmo programa passa quando somente essa checagem
é desabilitada (`build/audit/gns-ubsan-probe.cpp` e `.log`). Por isso apenas o
teste suplementar de ciclo de vida, que chama a biblioteca real, usa
`-fno-sanitize=vptr`. Os outros dois testes suplementares não precisam dessa
exclusão. Esse diagnóstico foi investigado mesmo quando CTest inicialmente
reportou sucesso por causa do modo recuperável padrão do UBSan.

## API pública e contratos

- Client e Server não podem mais ser copiados ou movidos. Server ganha destrutor
  com limpeza automática; `close()` é idempotente e `noexcept`.
- Apenas um Client ou Server iniciado possui a rede por processo. Start repetido
  retorna `operation_in_progress`; outro owner retorna `device_or_resource_busy`.
  `networkKill()` não invalida um wrapper ativo; o encerramento ocorre quando o
  owner libera os recursos. Raw GNS Kill/RunCallbacks durante esse ownership não
  são operações suportadas.
- `disconnect()` limpa o estado local imediatamente. Erros de recebimento passam
  a retornar `io_error`. Callbacks podem lançar; buffers são liberados e a
  exceção é propagada. Não se deve destruir o objeto que está despachando um
  callback. `forEach` deixa de ser incondicionalmente `noexcept`.
- `getClientId()` consulta pertencimento ao manager e não userdata GNS. A
  identidade persiste até remoção explícita. O manager deixa userdata para a
  aplicação. Capacidades negativas são rejeitadas; o domínio é limitado a 65535.
- Conversores mantêm assinaturas e lançam `bad_alloc`/`length_error` em falhas.
  Mensagens recebidas com tamanho inconsistente são rejeitadas sem callback,
  conforme a interface de parsing existente, que retorna `void`.
- Adicionados `clearMessagesBatcher()` e `Target::isGroup()`. A API
  `queueMessage()`/Target permanece; mensagens pendentes são liberadas no
  encerramento e após exceções. `initMessagesBatcher()` não descarta a fila.
- Há uma única constante pública `k_invalidConnection`. CMake propaga C++26 e
  os requisitos públicos GNS, com setup explícito de `import std` no consumidor.

Não há decisão arquitetural pendente para as correções adotadas. As restrições
de múltiplas instâncias, transporte próprio, callbacks e framing foram
explicitadas de forma conservadora. A revisão final examinou ownership,
ponteiros, liberação parcial de lotes, limites de IDs, reinicialização,
composição de módulos e consistência entre código e documentação.
