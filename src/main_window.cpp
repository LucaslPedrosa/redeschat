#include "main_window.hpp"
#include "network/chat_client.hpp"
#include "network/chat_server.hpp"
#include "ui/theme.hpp"

#include <QFormLayout>
#include <QFrame>
#include <QHBoxLayout>
#include <QComboBox>
#include <QInputDialog>
#include <QFileDialog>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMessageBox>
#include <QProgressBar>
#include <QPushButton>
#include <QSpinBox>
#include <QStackedWidget>
#include <QTabWidget>
#include <QTextEdit>
#include <QTime>
#include <QVBoxLayout>
#include <QWidget>

namespace {
constexpr int modePageIndex = 0;
constexpr int clientPageIndex = 1;
constexpr int authenticationPageIndex = 2;
constexpr int chatPageIndex = 3;
constexpr int serverPageIndex = 4;
constexpr int defaultPort = 5050;

QLabel *createTitle(const QString &text)
{
    auto *title = new QLabel(text);
    title->setObjectName("pageTitle");
    return title;
}

QLabel *createDescription(const QString &text)
{
    auto *description = new QLabel(text);
    description->setObjectName("description");
    description->setWordWrap(true);
    return description;
}

QSpinBox *createPortInput()
{
    auto *port = new QSpinBox;
    port->setRange(1, 65535);
    port->setValue(defaultPort);
    return port;
}
}

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent),
      pages_(new QStackedWidget(this)),
      client_(new ChatClient(this)),
      server_(new ChatServer(this)),
      clientAddress_(nullptr),
      clientPort_(nullptr),
      clientConnectButton_(nullptr),
      clientStatus_(nullptr),
      username_(nullptr),
      password_(nullptr),
      loginButton_(nullptr),
      registerButton_(nullptr),
      authenticationStatus_(nullptr),
      chatIdentity_(nullptr),
      chatHistory_(nullptr),
      roomTabs_(nullptr),
      availableRooms_(nullptr),
      onlineUsers_(nullptr),
      messageInput_(nullptr),
      sendMessageButton_(nullptr),
      sendFileButton_(nullptr),
      fileProgress_(nullptr),
      leaveRoomButton_(nullptr),
      serverPort_(nullptr),
      serverStartButton_(nullptr),
      serverStatus_(nullptr),
      serverClientCount_(nullptr),
      serverLog_(nullptr)
{
    setWindowTitle("RedesChat");
    setMinimumSize(780, 540);
    resize(1040, 680);

    pages_->addWidget(createModePage());
    pages_->addWidget(createClientPage());
    pages_->addWidget(createAuthenticationPage());
    pages_->addWidget(createChatPage());
    pages_->addWidget(createServerPage());
    setCentralWidget(pages_);

    client_->setStatusHandler([this](const QString &status) {
        clientStatus_->setText(status);
        if (pages_->currentIndex() == chatPageIndex && status.startsWith("Mensagem privada:")) {
            chatHistory_->append(QString("<span style='color:#d88b6d'>%1</span>")
                                     .arg(status.toHtmlEscaped()));
        }
        if (pages_->currentIndex() == chatPageIndex && status.startsWith("Sala:")) {
            appendRoomEvent(currentRoom(), status.toHtmlEscaped());
        }
    });
    client_->setConnectionHandler([this](bool connected) {
        setClientConnected(connected);
    });
    client_->setProtocolHandler([this] {
        authenticationStatus_->setText("Informe suas credenciais");
        showPage(authenticationPageIndex);
        username_->setFocus();
    });
    client_->setAuthenticationHandler(
        [this](bool success, const QString &username, const QString &message) {
            loginButton_->setEnabled(true);
            registerButton_->setEnabled(true);
            authenticationStatus_->setText(message);
            if (success) {
                authenticationStatus_->setText(QString("Autenticado como %1").arg(username));
                username_->setEnabled(false);
                password_->setEnabled(false);
                chatIdentity_->setText(QString("# geral  |  %1").arg(username));
                while (roomTabs_->count() > 1) {
                    QWidget *page = roomTabs_->widget(1);
                    roomTabs_->removeTab(1);
                    page->deleteLater();
                }
                roomHistories_.clear();
                roomHistories_.insert("geral", chatHistory_);
                chatHistory_->clear();
                showPage(chatPageIndex);
                messageInput_->setFocus();
            }
        });
    client_->setChatMessageHandler(
        [this](const QString &sender, const QString &text, const QString &timestamp) {
            const QDateTime dateTime = QDateTime::fromString(timestamp, Qt::ISODate);
            const QString time = dateTime.isValid()
                                     ? dateTime.toLocalTime().time().toString("HH:mm")
                                     : QTime::currentTime().toString("HH:mm");
            chatHistory_->append(QString("<b>%1</b> <span style='color:#77746c'>%2</span><br>%3")
                                     .arg(sender.toHtmlEscaped(), time, text.toHtmlEscaped()));
        });
    client_->setPrivateMessageHandler(
        [this](const QString &sender, const QString &recipient, const QString &text,
               const QString &timestamp) {
            const QDateTime dateTime = QDateTime::fromString(timestamp, Qt::ISODate);
            const QString time = dateTime.isValid()
                                     ? dateTime.toLocalTime().time().toString("HH:mm")
                                     : QTime::currentTime().toString("HH:mm");
            chatHistory_->append(
                QString("<span style='color:#c7a6d8'><b>[privado] %1 → %2</b> %3</span><br>%4")
                    .arg(sender.toHtmlEscaped(), recipient.toHtmlEscaped(), time,
                         text.toHtmlEscaped()));
        });
    client_->setPresenceHandler([this](const QString &username, bool joined) {
        chatHistory_->append(QString("<span style='color:#b9a16e'>%1 %2 a sala.</span>")
                                 .arg(username.toHtmlEscaped(), joined ? "entrou na" : "saiu da"));
    });
    client_->setUserListHandler([this](const QStringList &users) {
        onlineUsers_->clear();
        onlineUsers_->addItems(users);
    });
    client_->setRoomListHandler([this](const QStringList &rooms) {
        const QString selected = availableRooms_->currentText();
        availableRooms_->clear();
        availableRooms_->addItems(rooms);
        availableRooms_->setCurrentText(selected);
    });
    client_->setRoomMembershipHandler([this](const QString &room, bool joined) {
        if (joined) {
            QTextEdit *history = ensureRoomTab(room);
            roomTabs_->setCurrentWidget(history);
            appendRoomEvent(room, QString("Você entrou em #%1").arg(room.toHtmlEscaped()));
        } else if (roomHistories_.contains(room)) {
            QWidget *history = roomHistories_.take(room);
            roomTabs_->removeTab(roomTabs_->indexOf(history));
            history->deleteLater();
        }
    });
    client_->setRoomMessageHandler(
        [this](const QString &room, const QString &sender, const QString &text,
               const QString &timestamp) {
            QTextEdit *history = ensureRoomTab(room);
            const QDateTime dateTime = QDateTime::fromString(timestamp, Qt::ISODate);
            const QString time = dateTime.isValid()
                                     ? dateTime.toLocalTime().time().toString("HH:mm")
                                     : QTime::currentTime().toString("HH:mm");
            history->append(QString("<b>%1</b> <span style='color:#77746c'>%2</span><br>%3")
                                .arg(sender.toHtmlEscaped(), time, text.toHtmlEscaped()));
        });
    client_->setRoomPresenceHandler(
        [this](const QString &room, const QString &username, bool joined) {
            if (room != "geral") {
                appendRoomEvent(room, QString("%1 %2 #%3")
                                          .arg(username.toHtmlEscaped(),
                                               joined ? "entrou em" : "saiu de",
                                               room.toHtmlEscaped()));
            }
        });
    client_->setFileOfferHandler(
        [this](const QString &transferId, const QString &sender, const QString &name, qint64 size) {
            const auto answer = QMessageBox::question(
                this, "Arquivo recebido",
                QString("%1 deseja enviar %2 (%3 KiB). Aceitar?")
                    .arg(sender, name)
                    .arg((size + 1023) / 1024));
            if (answer != QMessageBox::Yes) {
                client_->respondToFile(transferId, false);
                return;
            }
            const QString destination = QFileDialog::getSaveFileName(
                this, "Salvar arquivo", name);
            client_->respondToFile(transferId, !destination.isEmpty(), destination);
        });
    client_->setFileProgressHandler([this](const QString &name, qint64 current, qint64 total) {
        fileProgress_->setVisible(true);
        fileProgress_->setMaximum(1000);
        fileProgress_->setValue(total == 0 ? 1000 : static_cast<int>(current * 1000 / total));
        fileProgress_->setFormat(QString("%1 - %p%").arg(name));
    });
    client_->setFileResultHandler([this](bool success, const QString &message) {
        fileProgress_->setVisible(false);
        appendRoomEvent(currentRoom(),
                        QString("%1: %2").arg(success ? "Arquivo" : "Erro", message.toHtmlEscaped()));
    });
    server_->setEventHandler([this](const QString &message) {
        appendServerEvent(message);
    });
    server_->setClientCountHandler([this](int count) {
        serverClientCount_->setText(QString("%1 cliente(s) conectado(s)").arg(count));
    });

    setStyleSheet(Theme::styleSheet());
}

QWidget *MainWindow::createModePage()
{
    auto *page = new QWidget;
    auto *pageLayout = new QVBoxLayout(page);
    pageLayout->setContentsMargins(72, 52, 72, 52);
    pageLayout->setSpacing(18);

    auto *brand = new QLabel("REDES DE COMPUTADORES II");
    brand->setObjectName("brand");
    pageLayout->addWidget(brand);
    pageLayout->addWidget(createTitle("Como deseja iniciar?"));
    pageLayout->addWidget(createDescription(
        "O mesmo programa pode se conectar a um servidor existente ou hospedar uma sala para a rede local."));
    pageLayout->addSpacing(16);

    auto *options = new QHBoxLayout;
    options->setSpacing(18);

    auto createOption = [this](const QString &title, const QString &description, int pageIndex) {
        auto *card = new QFrame;
        card->setObjectName("card");
        auto *layout = new QVBoxLayout(card);
        layout->setContentsMargins(24, 24, 24, 24);
        layout->setSpacing(12);

        auto *optionTitle = new QLabel(title);
        optionTitle->setObjectName("chatTitle");
        auto *optionDescription = createDescription(description);
        auto *button = new QPushButton(title);
        connect(button, &QPushButton::clicked, this, [this, pageIndex] { showPage(pageIndex); });

        layout->addWidget(optionTitle);
        layout->addWidget(optionDescription);
        layout->addStretch();
        layout->addWidget(button);
        return card;
    };

    options->addWidget(createOption(
        "Entrar como cliente",
        "Informe o IP da máquina servidora e participe das salas disponíveis.", clientPageIndex));
    options->addWidget(createOption(
        "Hospedar servidor",
        "Abra uma porta nesta máquina e gerencie as conexões dos clientes.", serverPageIndex));

    pageLayout->addLayout(options, 1);
    return page;
}

QWidget *MainWindow::createClientPage()
{
    auto *page = new QWidget;
    auto *pageLayout = new QVBoxLayout(page);
    pageLayout->setContentsMargins(72, 52, 72, 52);
    pageLayout->setSpacing(18);

    pageLayout->addWidget(createTitle("Conectar ao servidor"));
    pageLayout->addWidget(createDescription(
        "Use o endereço IPv4 da máquina que está hospedando o RedesChat."));

    auto *card = new QFrame;
    card->setObjectName("card");
    auto *cardLayout = new QFormLayout(card);
    cardLayout->setContentsMargins(28, 28, 28, 28);
    cardLayout->setSpacing(16);

    clientAddress_ = new QLineEdit;
    clientAddress_->setPlaceholderText("Ex.: 192.168.0.25");
    clientPort_ = createPortInput();
    cardLayout->addRow("Endereço do servidor", clientAddress_);
    cardLayout->addRow("Porta", clientPort_);
    pageLayout->addWidget(card);

    clientStatus_ = new QLabel("Desconectado");
    clientStatus_->setObjectName("hint");
    pageLayout->addWidget(clientStatus_);
    pageLayout->addStretch();

    auto *actions = new QHBoxLayout;
    auto *back = new QPushButton("Voltar");
    back->setObjectName("secondaryButton");
    connect(back, &QPushButton::clicked, this, [this] {
        client_->disconnectFromServer();
        showPage(modePageIndex);
    });
    clientConnectButton_ = new QPushButton("Conectar");
    connect(clientConnectButton_, &QPushButton::clicked, this, [this] { connectClient(); });
    connect(clientAddress_, &QLineEdit::returnPressed, this, [this] { connectClient(); });
    actions->addWidget(back);
    actions->addStretch();
    actions->addWidget(clientConnectButton_);
    pageLayout->addLayout(actions);

    return page;
}

QWidget *MainWindow::createServerPage()
{
    auto *page = new QWidget;
    auto *pageLayout = new QVBoxLayout(page);
    pageLayout->setContentsMargins(72, 52, 72, 52);
    pageLayout->setSpacing(18);

    pageLayout->addWidget(createTitle("Hospedar servidor"));
    pageLayout->addWidget(createDescription(
        "O servidor escutará em todas as interfaces IPv4 para aceitar clientes da rede local."));

    auto *card = new QFrame;
    card->setObjectName("card");
    auto *cardLayout = new QFormLayout(card);
    cardLayout->setContentsMargins(28, 28, 28, 28);
    cardLayout->setSpacing(16);
    serverPort_ = createPortInput();
    cardLayout->addRow("Porta de escuta", serverPort_);
    pageLayout->addWidget(card);

    auto *serverSummary = new QHBoxLayout;
    serverStatus_ = new QLabel("Servidor parado");
    serverStatus_->setObjectName("hint");
    serverClientCount_ = new QLabel("0 cliente(s) conectado(s)");
    serverClientCount_->setObjectName("hint");
    serverSummary->addWidget(serverStatus_);
    serverSummary->addStretch();
    serverSummary->addWidget(serverClientCount_);
    pageLayout->addLayout(serverSummary);

    serverLog_ = new QTextEdit;
    serverLog_->setObjectName("serverLog");
    serverLog_->setReadOnly(true);
    serverLog_->setPlaceholderText("Os eventos de conexão aparecerão aqui.");
    pageLayout->addWidget(serverLog_, 1);

    auto *actions = new QHBoxLayout;
    auto *back = new QPushButton("Voltar");
    back->setObjectName("secondaryButton");
    connect(back, &QPushButton::clicked, this, [this] {
        server_->stop();
        serverPort_->setEnabled(true);
        serverStartButton_->setText("Iniciar servidor");
        serverStatus_->setText("Servidor parado");
        showPage(modePageIndex);
    });
    serverStartButton_ = new QPushButton("Iniciar servidor");
    connect(serverStartButton_, &QPushButton::clicked, this, [this] { toggleServer(); });
    actions->addWidget(back);
    actions->addStretch();
    actions->addWidget(serverStartButton_);
    pageLayout->addLayout(actions);

    return page;
}

QWidget *MainWindow::createChatPage()
{
    auto *page = new QWidget;
    auto *pageLayout = new QVBoxLayout(page);
    pageLayout->setContentsMargins(18, 16, 18, 16);
    pageLayout->setSpacing(10);

    auto *headerFrame = new QFrame;
    headerFrame->setObjectName("headerBar");
    auto *header = new QHBoxLayout(headerFrame);
    header->setContentsMargins(14, 8, 10, 8);
    header->setSpacing(12);
    auto *appName = new QLabel("REDESCHAT");
    appName->setObjectName("sectionTitle");
    chatIdentity_ = new QLabel("# geral");
    chatIdentity_->setObjectName("chatTitle");
    auto *connectionStatus = new QLabel("CONECTADO");
    connectionStatus->setObjectName("statusPill");
    auto *leave = new QPushButton("Sair");
    leave->setObjectName("compactButton");
    connect(leave, &QPushButton::clicked, this, [this] {
        client_->disconnectFromServer();
        showPage(clientPageIndex);
    });
    header->addWidget(appName);
    header->addSpacing(12);
    header->addWidget(chatIdentity_);
    header->addStretch();
    header->addWidget(connectionStatus);
    header->addWidget(leave);
    pageLayout->addWidget(headerFrame);

    auto *content = new QHBoxLayout;
    content->setSpacing(10);

    auto *roomsPanel = new QFrame;
    roomsPanel->setObjectName("sidebar");
    roomsPanel->setFixedWidth(185);
    auto *roomsColumn = new QVBoxLayout(roomsPanel);
    roomsColumn->setContentsMargins(12, 14, 12, 12);
    roomsColumn->setSpacing(8);
    auto *roomsTitle = new QLabel("SALAS DISPONÍVEIS");
    roomsTitle->setObjectName("sectionTitle");
    availableRooms_ = new QComboBox;
    auto *joinRoom = new QPushButton("Entrar na sala");
    auto *createRoom = new QPushButton("Nova sala");
    auto *roomsHint = new QLabel("As salas abertas aparecem como abas na conversa.");
    roomsHint->setObjectName("muted");
    roomsHint->setWordWrap(true);
    leaveRoomButton_ = new QPushButton("Sair da sala atual");
    joinRoom->setObjectName("compactButton");
    createRoom->setObjectName("compactButton");
    leaveRoomButton_->setObjectName("compactButton");
    leaveRoomButton_->setEnabled(false);
    connect(joinRoom, &QPushButton::clicked, this, [this] {
        if (!availableRooms_->currentText().isEmpty()) {
            client_->joinRoom(availableRooms_->currentText());
        }
    });
    connect(createRoom, &QPushButton::clicked, this, [this] {
        bool accepted = false;
        const QString room = QInputDialog::getText(
            this, "Criar sala", "Nome da sala", QLineEdit::Normal, {}, &accepted);
        if (accepted && !room.isEmpty()) {
            client_->createRoom(room);
        }
    });
    connect(leaveRoomButton_, &QPushButton::clicked, this, [this] {
        client_->leaveRoom(currentRoom());
    });
    roomsColumn->addWidget(roomsTitle);
    roomsColumn->addWidget(availableRooms_);
    roomsColumn->addWidget(joinRoom);
    roomsColumn->addWidget(createRoom);
    roomsColumn->addSpacing(6);
    roomsColumn->addWidget(roomsHint);
    roomsColumn->addStretch();
    roomsColumn->addWidget(leaveRoomButton_);
    content->addWidget(roomsPanel);

    auto *conversationColumn = new QVBoxLayout;
    conversationColumn->setSpacing(8);
    chatHistory_ = new QTextEdit;
    chatHistory_->setReadOnly(true);
    chatHistory_->setPlaceholderText("As mensagens da sala geral aparecerão aqui.");
    roomTabs_ = new QTabWidget;
    roomTabs_->addTab(chatHistory_, "# geral");
    roomHistories_.insert("geral", chatHistory_);
    connect(roomTabs_, &QTabWidget::currentChanged, this, [this](int) {
        const QString room = currentRoom();
        chatIdentity_->setText(QString("# %1").arg(room));
        leaveRoomButton_->setEnabled(room != "geral");
    });
    conversationColumn->addWidget(roomTabs_, 1);

    auto *composerFrame = new QFrame;
    composerFrame->setObjectName("composer");
    auto *composer = new QHBoxLayout(composerFrame);
    composer->setContentsMargins(8, 6, 6, 6);
    composer->setSpacing(6);
    messageInput_ = new QLineEdit;
    messageInput_->setPlaceholderText("Mensagem ou /msg apelido texto");
    sendMessageButton_ = new QPushButton("Enviar");
    sendFileButton_ = new QPushButton("Arquivo");
    sendFileButton_->setObjectName("compactButton");
    connect(sendFileButton_, &QPushButton::clicked, this, [this] {
        QListWidgetItem *recipient = onlineUsers_->currentItem();
        if (recipient == nullptr) {
            appendRoomEvent(currentRoom(), "Selecione um usuário para enviar o arquivo");
            return;
        }
        const QString path = QFileDialog::getOpenFileName(this, "Selecionar arquivo");
        if (path.isEmpty()) {
            return;
        }
        QString errorMessage;
        if (!client_->offerFile(recipient->text(), path, &errorMessage)) {
            appendRoomEvent(currentRoom(), errorMessage.toHtmlEscaped());
        } else {
            appendRoomEvent(currentRoom(),
                            QString("Arquivo oferecido para %1").arg(recipient->text().toHtmlEscaped()));
        }
    });
    connect(sendMessageButton_, &QPushButton::clicked, this, [this] { sendChatMessage(); });
    connect(messageInput_, &QLineEdit::returnPressed, this, [this] { sendChatMessage(); });
    composer->addWidget(sendFileButton_);
    composer->addWidget(messageInput_, 1);
    composer->addWidget(sendMessageButton_);
    conversationColumn->addWidget(composerFrame);

    fileProgress_ = new QProgressBar;
    fileProgress_->setVisible(false);
    conversationColumn->addWidget(fileProgress_);
    content->addLayout(conversationColumn, 1);

    auto *usersPanel = new QFrame;
    usersPanel->setObjectName("sidebar");
    usersPanel->setFixedWidth(185);
    auto *usersColumn = new QVBoxLayout(usersPanel);
    usersColumn->setContentsMargins(12, 14, 12, 12);
    usersColumn->setSpacing(8);
    auto *usersTitle = new QLabel("CONECTADOS");
    usersTitle->setObjectName("sectionTitle");
    onlineUsers_ = new QListWidget;
    onlineUsers_->setToolTip("Duplo clique para iniciar uma mensagem privada");
    connect(onlineUsers_, &QListWidget::itemDoubleClicked, this, [this](QListWidgetItem *item) {
        messageInput_->setText(QString("/msg %1 ").arg(item->text()));
        messageInput_->setFocus();
        messageInput_->setCursorPosition(messageInput_->text().size());
    });
    usersColumn->addWidget(usersTitle);
    usersColumn->addWidget(onlineUsers_, 1);
    auto *usersHint = new QLabel("Duplo clique para mensagem privada. Selecione para enviar arquivo.");
    usersHint->setObjectName("muted");
    usersHint->setWordWrap(true);
    usersColumn->addWidget(usersHint);
    content->addWidget(usersPanel);
    pageLayout->addLayout(content, 1);
    return page;
}

QWidget *MainWindow::createAuthenticationPage()
{
    auto *page = new QWidget;
    auto *pageLayout = new QVBoxLayout(page);
    pageLayout->setContentsMargins(72, 52, 72, 52);
    pageLayout->setSpacing(18);

    pageLayout->addWidget(createTitle("Identificação"));
    pageLayout->addWidget(createDescription(
        "Entre com uma conta existente ou crie uma nova. A senha pode ser vazia."));

    auto *card = new QFrame;
    card->setObjectName("card");
    auto *cardLayout = new QFormLayout(card);
    cardLayout->setContentsMargins(28, 28, 28, 28);
    cardLayout->setSpacing(16);

    username_ = new QLineEdit;
    username_->setPlaceholderText("Apelido");
    password_ = new QLineEdit;
    password_->setPlaceholderText("Senha opcional");
    password_->setEchoMode(QLineEdit::Password);
    cardLayout->addRow("Apelido", username_);
    cardLayout->addRow("Senha", password_);
    pageLayout->addWidget(card);

    authenticationStatus_ = new QLabel("Informe suas credenciais");
    authenticationStatus_->setObjectName("hint");
    authenticationStatus_->setWordWrap(true);
    pageLayout->addWidget(authenticationStatus_);
    pageLayout->addStretch();

    auto *actions = new QHBoxLayout;
    auto *back = new QPushButton("Desconectar");
    back->setObjectName("secondaryButton");
    connect(back, &QPushButton::clicked, this, [this] {
        client_->disconnectFromServer();
        username_->setEnabled(true);
        password_->setEnabled(true);
        showPage(clientPageIndex);
    });
    registerButton_ = new QPushButton("Criar conta");
    registerButton_->setObjectName("secondaryButton");
    loginButton_ = new QPushButton("Entrar");
    connect(registerButton_, &QPushButton::clicked, this, [this] { authenticate(true); });
    connect(loginButton_, &QPushButton::clicked, this, [this] { authenticate(false); });
    connect(password_, &QLineEdit::returnPressed, this, [this] { authenticate(false); });

    actions->addWidget(back);
    actions->addStretch();
    actions->addWidget(registerButton_);
    actions->addWidget(loginButton_);
    pageLayout->addLayout(actions);
    return page;
}

void MainWindow::showPage(int pageIndex)
{
    pages_->setCurrentIndex(pageIndex);
}

void MainWindow::connectClient()
{
    if (client_->isConnected()) {
        client_->disconnectFromServer();
        return;
    }

    const QString host = clientAddress_->text().trimmed();
    if (host.isEmpty()) {
        clientStatus_->setText("Informe o endereço do servidor");
        clientAddress_->setFocus();
        return;
    }

    clientConnectButton_->setEnabled(false);
    client_->connectToServer(host, static_cast<quint16>(clientPort_->value()));
}

void MainWindow::authenticate(bool createAccount)
{
    const QString username = username_->text();
    if (username.isEmpty()) {
        authenticationStatus_->setText("O apelido não pode ser vazio");
        username_->setFocus();
        return;
    }

    loginButton_->setEnabled(false);
    registerButton_->setEnabled(false);
    authenticationStatus_->setText(createAccount ? "Criando conta..." : "Entrando...");
    if (createAccount) {
        client_->registerAccount(username, password_->text());
    } else {
        client_->login(username, password_->text());
    }
}

void MainWindow::sendChatMessage()
{
    const QString text = messageInput_->text();
    if (text.isEmpty()) {
        return;
    }
    if (text == "/lista") {
        client_->requestUserList();
    } else if (text == "/sair") {
        client_->disconnectFromServer();
        showPage(clientPageIndex);
    } else if (text.startsWith("/msg ")) {
        const qsizetype recipientEnd = text.indexOf(' ', 5);
        if (recipientEnd < 0 || recipientEnd == 5 || recipientEnd == text.size() - 1) {
            chatHistory_->append(
                "<span style='color:#d88b6d'>Uso: /msg apelido mensagem</span>");
        } else {
            const QString recipient = text.mid(5, recipientEnd - 5);
            const QString privateText = text.mid(recipientEnd + 1);
            client_->sendPrivateMessage(recipient, privateText);
        }
    } else {
        client_->sendRoomMessage(currentRoom(), text);
    }
    messageInput_->clear();
}

QTextEdit *MainWindow::ensureRoomTab(const QString &room)
{
    if (roomHistories_.contains(room)) {
        return roomHistories_.value(room);
    }

    auto *history = new QTextEdit;
    history->setReadOnly(true);
    history->setPlaceholderText(QString("As mensagens de #%1 aparecerão aqui.").arg(room));
    roomHistories_.insert(room, history);
    roomTabs_->addTab(history, QString("# %1").arg(room));
    return history;
}

QString MainWindow::currentRoom() const
{
    const int index = roomTabs_->currentIndex();
    return index >= 0 ? roomTabs_->tabText(index).mid(2) : QString("geral");
}

void MainWindow::appendRoomEvent(const QString &room, const QString &message)
{
    ensureRoomTab(room)->append(QString("<span style='color:#b9a16e'>%1</span>").arg(message));
}

void MainWindow::toggleServer()
{
    if (server_->isListening()) {
        server_->stop();
        serverPort_->setEnabled(true);
        serverStartButton_->setText("Iniciar servidor");
        serverStatus_->setText("Servidor parado");
        return;
    }

    QString errorMessage;
    if (!server_->start(static_cast<quint16>(serverPort_->value()), &errorMessage)) {
        serverStatus_->setText(QString("Falha ao iniciar: %1").arg(errorMessage));
        return;
    }

    serverPort_->setEnabled(false);
    serverStartButton_->setText("Parar servidor");
    serverStatus_->setText(QString("Escutando em 0.0.0.0:%1").arg(server_->port()));
}

void MainWindow::setClientConnected(bool connected)
{
    clientConnectButton_->setEnabled(true);
    clientConnectButton_->setText(connected ? "Desconectar" : "Conectar");
    clientAddress_->setEnabled(!connected);
    clientPort_->setEnabled(!connected);
    if (!connected) {
        username_->setEnabled(true);
        password_->setEnabled(true);
        loginButton_->setEnabled(true);
        registerButton_->setEnabled(true);
        if (pages_->currentIndex() == authenticationPageIndex) {
            showPage(clientPageIndex);
        }
        if (pages_->currentIndex() == chatPageIndex) {
            showPage(clientPageIndex);
        }
    }
}

void MainWindow::appendServerEvent(const QString &message)
{
    const QString timestamp = QTime::currentTime().toString("HH:mm:ss");
    serverLog_->append(QString("[%1] %2").arg(timestamp, message));
}
