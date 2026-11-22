# Protocolo de aplicação do RedesChat

## Visão geral

O RedesChat utiliza TCP como transporte e JSON UTF-8 como formato de serialização. Todas as decisões de autenticação, autorização e roteamento pertencem ao servidor central.

O cliente nunca é considerado autoridade sobre o remetente. Campos como `sender` são acrescentados pelo servidor com base na sessão associada ao socket autenticado.

## Enquadramento TCP

TCP fornece um fluxo contínuo de bytes, não mensagens isoladas. Uma chamada a `write()` pode chegar fragmentada em vários eventos `readyRead`, e várias escritas podem chegar concatenadas. Por isso, cada documento JSON possui um prefixo de tamanho:

```text
  0                   1                   2                   3
  0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
 +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 |                 tamanho do JSON (uint32)                     |
 +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 |                      JSON em UTF-8 ...                        |
 +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
```

- O cabeçalho possui 4 bytes sem sinal em big-endian, a ordem de rede.
- O valor descreve somente o payload JSON, sem incluir o cabeçalho.
- O JSON é serializado pelo `QJsonDocument` em formato compacto.
- O payload máximo aceito é 1 MiB.
- Cada conexão mantém um buffer de recepção independente.
- Frames incompletos permanecem no buffer até a chegada dos bytes restantes.
- Vários frames completos são retirados e processados em sequência.
- Tamanho inválido ou JSON malformado encerra a conexão para evitar dessincronização.

## Estados da sessão

```text
TCP conectado
      |
      v
handshake confirmado
      |
      v
usuário autenticado
      |
      v
chat, salas e arquivos
```

Antes do handshake, apenas `hello` é aceito. Depois do handshake, são aceitos `register` e `login`. Operações do chat exigem uma sessão autenticada.

## Tipos de mensagem

| Tipo | Direção principal | Finalidade |
|---|---|---|
| `hello` | cliente → servidor | Negocia a versão do protocolo. |
| `hello_ack` | servidor → cliente | Confirma a versão. |
| `error` | servidor → cliente | Informa erro geral do protocolo ou servidor. |
| `register` | cliente → servidor | Cria uma conta. |
| `register_result` | servidor → cliente | Resultado do cadastro. |
| `login` | cliente → servidor | Autentica uma conta existente. |
| `login_result` | servidor → cliente | Resultado do login. |
| `list_users` | cliente → servidor | Solicita atualização dos conectados. |
| `user_list` | servidor → cliente | Lista de usuários autenticados. |
| `user_joined` / `user_left` | servidor → cliente | Presença global. |
| `private_message` | ambas | Envia e entrega mensagem privada. |
| `private_message_result` | servidor → cliente | Erro no envio privado. |
| `create_room` | cliente → servidor | Cria e entra em uma sala. |
| `join_room` / `leave_room` | cliente → servidor | Altera participação em uma sala. |
| `list_rooms` | cliente → servidor | Solicita salas disponíveis. |
| `room_list` | servidor → cliente | Lista de salas persistentes. |
| `room_joined` / `room_left` | servidor → cliente | Confirma a alteração de participação. |
| `room_result` | servidor → cliente | Informa erro de operação de sala. |
| `room_user_joined` / `room_user_left` | servidor → cliente | Presença dentro de uma sala. |
| `room_message` | ambas | Envia e entrega mensagem de sala. |
| `room_history` | servidor → cliente | Entrega as últimas mensagens de uma sala. |
| `file_offer` | ambas | Oferece um arquivo ao destinatário. |
| `file_response` | ambas | Aceita ou recusa a oferta. |
| `file_chunk` | cliente → servidor → cliente | Transporta um bloco Base64. |
| `file_complete` | cliente → servidor → cliente | Finaliza os blocos. |
| `file_result` | servidor → cliente | Informa conclusão ou falha. |

O tipo `general_message` ainda é aceito pelo servidor e usado por testes de compatibilidade. A GUI atual trata `geral` como uma sala normal e envia `room_message` com `room` igual a `geral`.

## Handshake

Cliente:

```json
{"type":"hello","protocolVersion":1}
```

Servidor:

```json
{"type":"hello_ack","protocolVersion":1}
```

Versões incompatíveis recebem `error` com código `unsupported_protocol`, seguido do encerramento da conexão.

## Cadastro e login

```json
{"type":"register","username":"alice","password":"senha"}
```

```json
{"type":"login","username":"alice","password":"senha"}
```

Respostas:

```json
{
  "type":"register_result",
  "success":true,
  "username":"alice",
  "message":"Conta criada e autenticada"
}
```

`login_result` possui a mesma estrutura. O apelido não pode ser vazio, é sensível a maiúsculas e não pode estar conectado em duas sessões simultâneas. A senha pode ser vazia por decisão de escopo do projeto.

## Usuários e presença

Solicitação:

```json
{"type":"list_users"}
```

Resposta:

```json
{"type":"user_list","users":["alice","bob"]}
```

Entrada e saída são notificadas com `user_joined` e `user_left`, contendo `username`.

## Mensagens privadas

O cliente envia apenas destinatário e texto:

```json
{"type":"private_message","recipient":"bob","text":"Oi"}
```

O servidor entrega ao destinatário e envia uma cópia ao remetente:

```json
{
  "type":"private_message",
  "sender":"alice",
  "recipient":"bob",
  "text":"Oi",
  "timestamp":"2026-11-20T20:00:00.000Z"
}
```

Destinatário offline ou campos inválidos produzem `private_message_result` com `success: false`.

## Salas

Todo usuário autenticado entra automaticamente em `geral`. Essa sala não pode ser abandonada. Um usuário pode participar de várias salas simultaneamente.

```json
{"type":"create_room","room":"jogos"}
{"type":"join_room","room":"jogos"}
{"type":"leave_room","room":"jogos"}
{"type":"list_rooms"}
```

Mensagem enviada pelo cliente:

```json
{"type":"room_message","room":"jogos","text":"Vamos jogar?"}
```

Mensagem distribuída somente aos membros:

```json
{
  "type":"room_message",
  "room":"jogos",
  "sender":"alice",
  "text":"Vamos jogar?",
  "timestamp":"2026-11-20T20:00:00.000Z"
}
```

Ao entrar, o cliente recebe até 100 mensagens em ordem cronológica:

```json
{
  "type":"room_history",
  "room":"jogos",
  "messages":[
    {"sender":"alice","text":"Olá","timestamp":"2026-11-20T19:59:00.000Z"}
  ]
}
```

## Transferência de arquivos

Arquivos são transferidos privadamente através do servidor. O servidor mantém apenas metadados temporários e encaminha os blocos; ele não grava o arquivo.

Limites:

- Arquivo máximo: 10 MiB.
- Bloco original: até 32 KiB.
- Dados do bloco: Base64 dentro do JSON.
- Integridade: SHA-256 calculado pelo remetente e verificado pelo destinatário.

Fluxo:

```text
remetente     servidor       destinatário
    | file_offer  |                |
    |------------>| file_offer     |
    |              |-------------->|
    |              | file_response |
    | file_response|<--------------|
    |<-------------|                |
    | file_chunk   | file_chunk     |
    |------------->|--------------->|
    | file_complete| file_complete  |
    |------------->|--------------->|
```

Oferta:

```json
{
  "type":"file_offer",
  "transferId":"uuid",
  "recipient":"bob",
  "name":"trabalho.pdf",
  "size":12345,
  "sha256":"hash hexadecimal"
}
```

Bloco:

```json
{
  "type":"file_chunk",
  "transferId":"uuid",
  "offset":0,
  "data":"conteúdo em Base64"
}
```

O servidor valida a identidade do remetente, o destinatário, offsets e tamanho acumulado. O destinatário só grava após validar tamanho e hash. Recusa, erro de bloco ou desconexão cancela a transferência.

## Persistência

O servidor usa SQLite com as tabelas:

```text
users(username, password_hash, salt)
rooms(name, created_at)
room_messages(id, room, sender, content, created_at)
```

Contas, salas e mensagens de sala persistem. Participações, usuários online e transferências existem somente durante a execução.

## Erros e limites de segurança

Erros gerais usam:

```json
{"type":"error","code":"not_authenticated","message":"Autenticação necessária"}
```

O banco não guarda senhas em texto puro. Cada conta recebe salt aleatório e SHA-256 iterado. Essa solução é didática; produção deveria usar Argon2id, scrypt ou PBKDF2. O transporte atual não usa TLS, portanto credenciais e mensagens não são confidenciais contra captura de tráfego.
