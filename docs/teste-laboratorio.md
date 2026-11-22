# Roteiro de teste em laboratório

## Objetivo

Validar o RedesChat em uma rede local real com uma máquina Linux hospedando o servidor e pelo menos duas máquinas Linux executando clientes.

Os testes no WSL não substituem este roteiro. WSL2 normalmente utiliza NAT e uma interface virtual, enquanto o laboratório usará hosts Linux diretamente conectados à rede local.

## 1. Registrar o ambiente

Execute em cada máquina:

```bash
uname -m
cat /etc/os-release
```

Registre arquitetura, distribuição e versão no relatório. O pacote x86_64 é construído no ambiente Ubuntu 22.04 do script `build-portable.sh` e aceita no máximo símbolos `GLIBC_2.35`; confirme a versão da glibc disponível no laboratório com `ldd --version`.

## 2. Identificar o IPv4 do servidor

Na máquina escolhida como servidor:

```bash
ip -brief address
ip route
```

Escolha o IPv4 da interface conectada à rede do laboratório, normalmente `eth0`, `enp...` ou `wlp...`. Ignore:

- `127.0.0.1`, que é loopback.
- `0.0.0.0`, que representa todas as interfaces para escuta, mas não é um destino.
- Interfaces de Docker, VPN ou virtualização que não pertencem à rede do laboratório.

Exemplo de endereço válido:

```text
192.168.10.25
```

## 3. Iniciar o servidor

Execute o programa e selecione **Hospedar servidor**. Use uma porta não privilegiada, como `5050`.

Durante o desenvolvimento:

```bash
xmake run redeschat
```

Na entrega portátil, execute `./RedesChat-x86_64.run`. Alternativamente, extraia todo o ZIP e execute o launcher `./RedesChat`, ou use o AppImage. Caso o AppImage informe ausência de FUSE, execute `APPIMAGE_EXTRACT_AND_RUN=1 ./RedesChat-x86_64.AppImage`.

O painel deve mostrar que o servidor está escutando em:

```text
0.0.0.0:5050
```

Confirme no terminal:

```bash
ss -ltnp
```

Procure uma entrada com `0.0.0.0:5050` ou `*:5050`.

## 4. Verificar o firewall

Primeiro tente conectar sem alterar o firewall. Se a porta estiver bloqueada, identifique o gerenciador usado pela distribuição.

### UFW

```bash
sudo ufw status
sudo ufw allow 5050/tcp
```

Depois da apresentação, a regra pode ser removida com:

```bash
sudo ufw delete allow 5050/tcp
```

### firewalld

```bash
sudo firewall-cmd --state
sudo firewall-cmd --add-port=5050/tcp
```

A regra anterior é temporária e desaparece ao reiniciar ou recarregar o firewalld. Se for necessário removê-la manualmente:

```bash
sudo firewall-cmd --remove-port=5050/tcp
```

Não desative todo o firewall. Libere somente a porta TCP utilizada pelo trabalho e apenas com autorização do laboratório.

## 5. Testar alcance da porta

Em uma máquina cliente:

```bash
ping -c 3 192.168.10.25
nc -vz 192.168.10.25 5050
```

Substitua o endereço pelo IPv4 real. Algumas redes bloqueiam ICMP, portanto falha no `ping` não prova que o TCP está indisponível. O teste com `nc` é mais relevante.

Se `nc` não estiver instalado, tente a conexão pela própria aplicação e observe a mensagem de erro.

## 6. Conectar os clientes

Em cada máquina cliente:

1. Execute o mesmo programa.
2. Selecione **Entrar como cliente**.
3. Informe o IPv4 real do servidor.
4. Informe a porta `5050`.
5. Cadastre usuários diferentes e use senhas não vazias durante a demonstração.

Embora o programa permita senha vazia por decisão de escopo, usar senhas não vazias deixa o requisito de autenticação evidente para a avaliação.

## 7. Casos de teste

Marque cada item e registre observações ou capturas.

| Caso | Procedimento | Resultado esperado | Executado |
|---|---|---|---|
| Conexão simultânea | Conectar dois clientes ao servidor | Ambos aparecem na lista | [ ] |
| Apelido duplicado | Tentar cadastrar o mesmo apelido | Cadastro rejeitado | [ ] |
| Login | Sair e entrar com a conta criada | Autenticação aceita | [ ] |
| Senha incorreta | Entrar com senha diferente | Login rejeitado | [ ] |
| Broadcast | Enviar em `geral` | Todos em `geral` recebem | [ ] |
| Privada | Usar `/msg usuário texto` | Somente os dois envolvidos recebem | [ ] |
| Presença | Conectar e desconectar um usuário | Entrada e saída notificadas | [ ] |
| Lista | Usar `/lista` | Lista atualizada | [ ] |
| Saída controlada | Usar `/sair` | Conexão encerrada sem travar servidor | [ ] |
| Criar sala | Criar `laboratorio` | Sala aparece para os clientes | [ ] |
| Isolamento | Enviar na nova sala | Somente membros recebem | [ ] |
| Várias salas | Participar de `geral` e outra sala | Abas funcionam simultaneamente | [ ] |
| Histórico | Enviar, sair e entrar novamente | Mensagens anteriores aparecem | [ ] |
| Arquivo | Enviar arquivo entre máquinas | Destino aceita e arquivo é idêntico | [ ] |
| Queda abrupta | Fechar um cliente sem `/sair` | Servidor remove e notifica | [ ] |
| IP incorreto | Informar endereço inexistente | Erro ou timeout em até 5 segundos | [ ] |
| Porta incorreta | Informar porta fechada | Conexão recusada ou timeout | [ ] |
| Servidor reiniciado | Reiniciar e fazer login | Contas, salas e histórico persistem | [ ] |

## 8. Conferir o arquivo transferido

Para demonstrar integridade fora da aplicação, calcule o SHA-256 nas duas máquinas:

```bash
sha256sum arquivo-original
sha256sum arquivo-recebido
```

Os hashes devem ser iguais.

## 9. Diagnóstico de problemas

### Conecta localmente, mas não em outra máquina

Verifique:

- O cliente está usando o IPv4 correto, não `127.0.0.1`.
- O servidor mostra `0.0.0.0:porta`.
- A porta aparece em `ss -ltnp`.
- As máquinas estão na mesma rede ou VLAN.
- O firewall permite a porta TCP.
- A rede do laboratório não possui isolamento entre clientes.

### Timeout imediato ou servidor indisponível

Confirme se o servidor ainda está executando e se IP e porta são iguais nos dois lados. Uma porta ocupada será informada na tela do servidor.

### Executável não inicia

Execute no terminal para observar erros e verifique bibliotecas:

```bash
./redeschat
ldd ./redeschat
```

Se aparecer uma versão de `GLIBC` ausente, reconstrua em uma distribuição mais antiga ou compatível com o laboratório. Se faltar plugin Qt, revise o empacotamento dos plugins `platforms` e `sqldrivers`.

### Banco de dados

O caminho aparece no log do servidor. Para começar uma demonstração com banco limpo, primeiro faça backup do arquivo e só então remova-o intencionalmente com a aplicação encerrada. Não apague o banco de uma instalação que contenha dados necessários.

## 10. Evidências para o relatório

Registrar:

- Foto ou captura das três máquinas conectadas.
- IPv4 e porta utilizados.
- Lista de usuários simultâneos.
- Mensagem em sala e mensagem privada.
- Transferência com hashes iguais.
- Histórico após reiniciar o servidor.
- Erro exibido para IP ou porta incorretos.
- Comando de firewall utilizado, se necessário.
- Distribuição e arquitetura de cada máquina.

Depois do ensaio, copie os resultados para a seção **Testes em rede real** de `relatorio-tecnico.md`.
