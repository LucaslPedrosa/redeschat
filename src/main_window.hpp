#pragma once

#include <QHash>
#include <QMainWindow>

class QStackedWidget;
class QLabel;
class QLineEdit;
class QPushButton;
class QSpinBox;
class QTextEdit;
class QListWidget;
class QComboBox;
class QTabWidget;
class QProgressBar;
class QWidget;
class ChatClient;
class ChatServer;

class MainWindow final : public QMainWindow
{
public:
    explicit MainWindow(QWidget *parent = nullptr);

private:
    QWidget *createModePage();
    QWidget *createClientPage();
    QWidget *createAuthenticationPage();
    QWidget *createChatPage();
    QWidget *createServerPage();
    void showPage(int pageIndex);
    void connectClient();
    void authenticate(bool createAccount);
    void sendChatMessage();
    QTextEdit *ensureRoomTab(const QString &room);
    QString currentRoom() const;
    void appendRoomEvent(const QString &room, const QString &message);
    void toggleServer();
    void setClientConnected(bool connected);
    void appendServerEvent(const QString &message);

    QStackedWidget *pages_;
    ChatClient *client_;
    ChatServer *server_;
    QLineEdit *clientAddress_;
    QSpinBox *clientPort_;
    QPushButton *clientConnectButton_;
    QLabel *clientStatus_;
    QLineEdit *username_;
    QLineEdit *password_;
    QPushButton *loginButton_;
    QPushButton *registerButton_;
    QLabel *authenticationStatus_;
    QLabel *chatIdentity_;
    QTextEdit *chatHistory_;
    QTabWidget *roomTabs_;
    QComboBox *availableRooms_;
    QListWidget *onlineUsers_;
    QLineEdit *messageInput_;
    QPushButton *sendMessageButton_;
    QPushButton *sendFileButton_;
    QProgressBar *fileProgress_;
    QPushButton *leaveRoomButton_;
    QHash<QString, QTextEdit *> roomHistories_;
    QSpinBox *serverPort_;
    QPushButton *serverStartButton_;
    QLabel *serverStatus_;
    QLabel *serverClientCount_;
    QTextEdit *serverLog_;
};
