# Configuração

O LightnerX é uma biblioteca estática em C++. O projeto fornece a target `LightnerX` e não possui executável próprio.

## GameNetworkingSockets

O GameNetworkingSockets é mantido como submódulo em `external/GameNetworkingSockets`. Depois de obter o repositório, execute na raiz do LightnerX:

```bash
git submodule update --init --recursive
```

Esse comando obtém o GameNetworkingSockets e seus submódulos recursivos no estado esperado pelo LightnerX.
