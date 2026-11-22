# RedesChat

Aplicação de chat cliente-servidor desenvolvida em C++20, Qt 6, TCP e SQLite para o trabalho prático de Redes de Computadores II.

O mesmo programa pode hospedar o servidor central ou atuar como cliente. O endereço do servidor e a porta são configuráveis pela interface; não existe `localhost` fixo no código da aplicação.

## Funcionalidades

- Múltiplos clientes atendidos de forma assíncrona pelo event loop do Qt.
- Cadastro e login com apelido único e senha.
- Sala `geral` e salas temáticas simultâneas.
- Broadcast, mensagens privadas e notificações de presença.
- Lista de usuários conectados.
- Comandos `/lista`, `/sair` e `/msg apelido mensagem`.
- Transferência privada de arquivos de até 10 MiB.
- Interface gráfica Qt Widgets.
- Contas, salas e histórico persistidos em SQLite.
- Últimas 100 mensagens carregadas ao entrar em uma sala.
- Protocolo próprio com framing TCP e documentos JSON.

## Dependências

O projeto usa Xmake para compilação e obtenção do Qt 6.9.1. Na primeira configuração, as dependências são baixadas para o diretório de pacotes do Xmake.

Requisitos do ambiente de desenvolvimento:

- Linux x86_64.
- Compilador com suporte a C++20.
- Xmake disponível no `PATH`.
- WSLg ou servidor X para abrir a GUI dentro do WSL.

Nenhum servidor de banco de dados precisa ser instalado. O driver SQLite faz parte da distribuição do Qt usada pelo projeto.

## Compilação

```bash
xmake f -m debug -y
xmake
xmake run redeschat
```

Para uma compilação otimizada:

```bash
xmake f -m release -y
xmake
```

## Uso

### Servidor

1. Execute `xmake run redeschat`.
2. Selecione **Hospedar servidor**.
3. Escolha uma porta, como `5050`.
4. Clique em **Iniciar servidor**.
5. Informe aos clientes o IPv4 real da máquina servidora.

O servidor escuta em `0.0.0.0`, isto é, em todas as interfaces IPv4. `0.0.0.0` não é o endereço que deve ser digitado pelos clientes.

### Cliente

1. Execute o mesmo programa.
2. Selecione **Entrar como cliente**.
3. Informe o IPv4 da máquina servidora e a porta escolhida.
4. Crie uma conta ou faça login.
5. Use as abas para conversar em várias salas.

Para testar várias instâncias no mesmo computador durante o desenvolvimento, pode-se usar `127.0.0.1`. No laboratório, os clientes devem usar o endereço real da máquina servidora.

## Comandos

| Comando | Ação |
|---|---|
| `/lista` | Atualiza a lista de usuários conectados. |
| `/sair` | Encerra controladamente a conexão. |
| `/msg apelido mensagem` | Envia uma mensagem privada. |

Mensagens comuns são enviadas para a sala cuja aba está selecionada. Um duplo clique em um usuário prepara uma mensagem privada. Para enviar um arquivo, selecione o destinatário e clique em **Arquivo**.

## Banco de dados

O servidor cria automaticamente o arquivo SQLite no diretório retornado por `QStandardPaths::AppDataLocation`. Em uma instalação Linux comum, o caminho é semelhante a:

```text
~/.local/share/Redes 2/RedesChat/redeschat.db
```

O caminho efetivo aparece no log do servidor. O banco contém:

- `users`: apelidos, salts e hashes de senha.
- `rooms`: salas persistentes.
- `room_messages`: histórico das salas.

Participações em salas e usuários online não são persistidos porque representam o estado atual das conexões.

## Testes automatizados

```bash
xmake build protocol_tests
xmake run protocol_tests

xmake build database_tests
xmake run database_tests

xmake build network_smoke
xmake run network_smoke

xmake build file_transfer_smoke
xmake run file_transfer_smoke
```

Os testes cobrem framing fragmentado e concatenado, autenticação, broadcast, mensagens privadas, isolamento de salas, persistência, histórico, transferência em múltiplos blocos e integridade do arquivo.

Esses testes usam a interface de loopback e não substituem o teste obrigatório entre máquinas físicas da rede local.

## Pacote portátil para Linux

Para compilar em um ambiente Ubuntu 22.04 isolado e gerar os três formatos de entrega:

```bash
./scripts/build-portable.sh
```

Na máquina de desenvolvimento, o comando requer `bwrap`, `curl` e `tar` e faz downloads na primeira execução. Alterações em `src/` são incluídas ao executar novamente o mesmo comando. Os resultados são:

- `dist/RedesChat-x86_64.run`: executável autoextraível, sem FUSE.
- `dist/RedesChat-x86_64.AppImage`: AppImage para Linux x86_64.
- `dist/RedesChat-linux-x86_64.zip`: diretório portátil compactado.

Na máquina de destino não é necessário instalar Qt, Xmake ou bibliotecas XCB, nem acessar a internet. A opção mais direta é:

```bash
chmod +x RedesChat-x86_64.run
./RedesChat-x86_64.run
```

O AppImage pode ser iniciado normalmente. Se a distribuição não fornecer FUSE, use o modo de extração integrado:

```bash
chmod +x RedesChat-x86_64.AppImage
APPIMAGE_EXTRACT_AND_RUN=1 ./RedesChat-x86_64.AppImage
```

Para usar o ZIP, extraia todo o conteúdo, mantenha as pastas juntas e execute:

```bash
./RedesChat
```

Os pacotes incluem o executável, Qt, os plugins `xcb`, `offscreen` e `sqlite` e a cadeia de dependências não pertencente à glibc. A glibc continua sendo fornecida pelo Linux de destino; a geração em Ubuntu 22.04 limita os símbolos a `GLIBC_2.35` para preservar compatibilidade.

## Estrutura

```text
src/
  database/    SQLite, contas, salas e histórico
  network/     cliente, servidor, sessões e roteamento
  protocol/    framing TCP e serialização JSON
  ui/          tema visual
  main.cpp
  main_window.cpp
tests/         testes automatizados
docs/          protocolo, relatório e roteiro de laboratório
```

## Documentação

- [Especificação do protocolo](docs/protocol.md)
- [Relatório técnico](docs/relatorio-tecnico.md)
- [Roteiro de teste em laboratório](docs/teste-laboratorio.md)

## Estado de entrega

Os requisitos funcionais principais estão implementados. Os três formatos portáteis foram gerados, tiveram suas dependências auditadas e iniciaram sem rede em modo gráfico `offscreen`. Antes da entrega ainda é necessário:

- Validar o pacote portátil na distribuição Linux do laboratório.
- Executar o roteiro em pelo menos três máquinas Linux reais.
- Registrar no relatório os resultados, versões das distribuições e eventuais ajustes de firewall.
