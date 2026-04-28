#ifndef NETWORKMANAGER_H
#define NETWORKMANAGER_H

#include "IRCChannel.h"
#include "IRCUser.h"
#include "IRCMessage.h"
#include <QSet>
#include <QTcpSocket>
#include <QSslSocket>
#include <QTimer>
#include <QMap>
#include <QStringList>
#include <QByteArray>

class NetworkManager : public QObject {
    Q_OBJECT

public:
    enum State {
        Disconnected,
        Connecting,
        Connected,
        Error
    };

    explicit NetworkManager(QObject* parent = nullptr);
    ~NetworkManager() override;

  void connectToServer(const QString& host, quint16 port,
                          const QString& nick, const QString& pass = {},
                          const QString& channel = {}, bool useTLS = false);
    void disconnect();
    void sendMessage(const QString& channel, const QString& message);
    void joinChannel(const QString& channel);
    void sendUserInput(const QString& context, const QString& text);
    void sendNotice(const QString& target, const QString& text);
    void setNick(const QString& nick);
    void changeMode(const QString& target, const QString& mode);
    void setTopic(const QString& channel, const QString& topic);
    void whois(const QString& nick);

    State state();
    QString nick() const;
    QString serverHost() const;

    const QMap<QString, IRCChannel*>& channels() const;
    IRCChannel* channel(const QString& name);

    void setCurrentChannel(const QString& name);
    void registerChannel(const QString& name);
    void removeChannel(const QString& name);

signals:
    void stateChanged(NetworkManager::State state);
    void connected();
    void disconnected();
    void serverError(const QString& error);
    void serverMessage(const QString& message);
    void serverChannelMessage(const QString& message);

    void channelRegistered(const QString& name);
    void channelJoined(const QString& name);
    void channelUnregistered(const QString& name);
    void channelMessage(const QString& channel, const IRCMessage& msg);
    void channelTopic(const QString& channel, const QString& topic);
    void channelMode(const QString& channel, const QString& mode);
    void userJoined(const QString& channel, const IRCUser& user);
    void userLeft(const QString& channel, const QString& nick, const QString& reason);
    void userChangedNick(const QString& oldNick, const QString& newNick);
    void nickSet(const QString& nick);
    void noticeReceived(const QString& sender, const QString& text);
    void namesComplete(const QString& channel);
    void namesReceived(const QString& channel, const QList<IRCUser>& users);
    void userReceived(const QString& channel, const QString& nick);
    void whoisIdent(const QString& nick, const QString& ident);
    void whoisAccount(const QString& nick, const QString& account);
    void whoisChannels(const QString& nick, const QString& channels);
    void whoisDone(const QString& nick);
    void ctcpRequest(const QString& nick, const QString& command, const QString& text);
    void ctcpReply(const QString& nick, const QString& command, const QString& text);
    void queryTabNeeded(const QString& nick);

private slots:
    void onConnected();
    void onDisconnected();
    void onReadyRead();
    void onError(QAbstractSocket::SocketError error);
    void onPingTimeout();

private:
    void parseLine(const QString& line);
    void parseMessage(const QString& line);
    void handlePrivMsg(const QString& prefix, const QStringList& params);
    void handleNotice(const QString& prefix, const QStringList& params);
    void handleNick(const QString& prefix, const QStringList& params);
    void handleJoin(const QString& prefix, const QStringList& params);
    void handlePart(const QString& prefix, const QStringList& params);
    void handleMode(const QString& prefix, const QStringList& params);
    void handleTopic(const QString& prefix, const QStringList& params);
    void handleQuit(const QString& prefix, const QStringList& params);
    void handleKick(const QString& prefix, const QStringList& params);
    void handleCapCommand(const QStringList& params);
    void handleNumericReply(const QString& numeric, const QString& prefix, const QStringList& params);

    void sendRaw(const QString& data);
    void sendCommand(const QString& cmd, const QStringList& params);
    void sendCtcpVersionReply(const QString& target);
    bool isCtcpMessage(const QString& message);
    void parseCtcpMessage(const QString& message, QString& command, QString& text);

    QAbstractSocket* m_socket;
    QString m_host;
    quint16 m_port;
    QString m_nick;
    QString m_pass;
    QString m_currentChannel;

    QMap<QString, IRCChannel*> m_channels;
    State m_state;
    QString m_username;
    QString m_realname;
    QTimer* m_pingTimer;
    QStringList m_capSupported;
    QSet<QString> m_activeCaps;
    QByteArray m_lineBuffer;
    int m_nickRetries = 0;
    bool m_waitingCaps = false;
};

#endif // NETWORKMANAGER_H
