#include "NetworkManager.h"
#include <QLoggingCategory>

Q_LOGGING_CATEGORY(logIRC, "qwenirc.irc")

NetworkManager::NetworkManager(QObject* parent)
    : QObject(parent)
    , m_socket(new QTcpSocket(this))
    , m_state(Disconnected)
    , m_pingTimer(new QTimer(this))
{
    connect(m_socket, &QAbstractSocket::connected, this, &NetworkManager::onConnected);
    connect(m_socket, &QAbstractSocket::disconnected, this, &NetworkManager::onDisconnected);
    connect(m_socket, &QAbstractSocket::readyRead, this, &NetworkManager::onReadyRead);
    connect(m_socket, QOverload<QAbstractSocket::SocketError>::of(&QAbstractSocket::errorOccurred),
            this, &NetworkManager::onError);

    connect(m_pingTimer, &QTimer::timeout, this, &NetworkManager::onPingTimeout);
    m_pingTimer->setInterval(3 * 60 * 1000);
}

NetworkManager::~NetworkManager() {
    for (auto* channel : m_channels) {
        delete channel;
    }
    m_channels.clear();
}

void NetworkManager::connectToServer(const QString& host, quint16 port,
                                      const QString& nick, const QString& pass,
                                      const QString& channel) {
    // Abort any existing hanging connection to prevent silently ignored attempts
    if (m_socket->state() == QAbstractSocket::ConnectingState ||
        m_socket->state() == QAbstractSocket::ConnectedState) {
        m_socket->abort();
    }
    
    m_host = host;
    m_port = port;
    m_nick = nick;
    m_pass = pass;
    m_currentChannel = channel;
    m_state = Connecting;
    emit stateChanged(m_state);
    m_socket->connectToHost(host, port);
}

void NetworkManager::disconnect() {
    m_socket->disconnectFromHost();
    m_state = Disconnected;
    emit stateChanged(m_state);
    emit disconnected();
}

void NetworkManager::sendMessage(const QString& channel, const QString& message) {
    sendCommand("PRIVMSG", QStringList() << channel << message);
}

void NetworkManager::joinChannel(const QString& channel) {
    sendCommand("JOIN", QStringList() << channel);
    sendCommand("NAMES", QStringList() << channel);
}

void NetworkManager::sendUserInput(const QString& context, const QString& text) {
    if (text.startsWith('/')) {
        QStringList parts = text.mid(1).split(' ', Qt::SkipEmptyParts);
        if (parts.isEmpty()) return;
        
        QString cmd = parts[0].toUpper();
        
        if (cmd == "JOIN" && parts.size() >= 2) {
            joinChannel(parts[1]);
        } else if (cmd == "PART" && parts.size() >= 2) {
            sendCommand("PART", QStringList() << parts[1]);
        } else if (cmd == "NICK" && parts.size() >= 2) {
            setNick(parts[1]);
        } else if (cmd == "QUIT") {
            sendCommand("QUIT", QStringList() << (parts.size() >= 2 ? parts.mid(1).join(' ') : "Leaving"));
        } else if (cmd == "MSG" && parts.size() >= 3) {
            sendMessage(parts[1], parts.mid(2).join(' '));
        } else if (cmd == "QUOTE" || cmd == "RAW") {
            if (parts.size() >= 2) sendRaw(parts.mid(1).join(' ') + "\r\n");
        } else {
            sendCommand(cmd, parts.mid(1));
        }
    } else {
        if (!context.isEmpty() && context != "Server") {
            sendMessage(context, text);
            
            IRCMessage msg(MessageType::Message, text, m_nick);
            msg.setChannel(context);
            emit channelMessage(context, msg);
        }
    }
}

void NetworkManager::sendNotice(const QString& target, const QString& text) {
    sendCommand("NOTICE", QStringList() << target << text);
}

void NetworkManager::setNick(const QString& nick) {
    sendCommand("NICK", QStringList() << nick);
}

void NetworkManager::changeMode(const QString& target, const QString& mode) {
    sendCommand("MODE", QStringList() << target << mode);
}

void NetworkManager::setTopic(const QString& channel, const QString& topic) {
    sendCommand("TOPIC", QStringList() << channel << topic);
}

void NetworkManager::whois(const QString& nick) {
    sendCommand("WHOIS", QStringList() << nick);
}

NetworkManager::State NetworkManager::state() {
    return m_state;
}

QString NetworkManager::nick() const {
    return m_nick;
}

QString NetworkManager::serverHost() const {
    return m_host;
}

const QMap<QString, IRCChannel*>& NetworkManager::channels() const {
    return m_channels;
}

IRCChannel* NetworkManager::channel(const QString& name) {
    auto it = m_channels.find(name);
    if (it != m_channels.end()) {
        return it.value();
    }
    return nullptr;
}

void NetworkManager::setCurrentChannel(const QString& name) {
    m_currentChannel = name;
    emit channelJoined(name);
}

void NetworkManager::registerChannel(const QString& name) {
    if (!m_channels.contains(name)) {
        IRCChannel* ch = new IRCChannel(name);
        m_channels.insert(name, ch);
        emit channelRegistered(name);
    }
}

void NetworkManager::removeChannel(const QString& name) {
    if (m_channels.contains(name)) {
        delete m_channels.take(name);
        emit channelUnregistered(name);
    }
}

void NetworkManager::onConnected() {
    m_state = Connected;
    emit stateChanged(m_state);
    emit connected();

    // Request capabilities
    QStringList caps = m_capSupported;
    QString capReq = "CAP REQ :" + caps.join(' ') + "\r\n";
    sendRaw(capReq);

    if (!m_pass.isEmpty()) {
        sendCommand("PASS", QStringList() << m_pass);
    }

    m_username = m_nick;
    m_realname = m_nick;

    sendCommand("NICK", QStringList() << m_nick);
    sendCommand("USER", QStringList() << m_username << "8" << "*" << m_realname);

    m_pingTimer->start();
}

void NetworkManager::onDisconnected() {
    m_state = Disconnected;
    emit stateChanged(m_state);
    emit disconnected();
    m_pingTimer->stop();
}

void NetworkManager::onReadyRead() {
    QTcpSocket* socket = qobject_cast<QTcpSocket*>(sender());
    if (!socket) return;

    QByteArray data = socket->readAll();
    m_lineBuffer += data;

    const int MAX_BUFFER_SIZE = 10 * 1024 * 1024;
    if (m_lineBuffer.size() > MAX_BUFFER_SIZE) {
        m_lineBuffer.truncate(MAX_BUFFER_SIZE / 2);
    }

    while (true) {
        int endPos = m_lineBuffer.indexOf('\n');
        if (endPos < 0) return;

        QString line = QString::fromUtf8(m_lineBuffer.left(endPos + 1));
        m_lineBuffer = m_lineBuffer.mid(endPos + 1);

        line.replace('\r', QChar(' '));
        if (line.isEmpty()) continue;

        parseLine(line);
    }
}

void NetworkManager::onError(QAbstractSocket::SocketError error) {
    // Sync manual state with actual socket state
    switch (m_socket->state()) {
    case QAbstractSocket::ConnectedState:
        m_state = Connected;
        break;
    case QAbstractSocket::ConnectingState:
        m_state = Connecting;
        break;
    default:
        m_state = Error;
        break;
    }
    emit stateChanged(m_state);
    emit serverError(m_socket->errorString());
    Q_UNUSED(error);
}

void NetworkManager::onPingTimeout() {
    emit serverChannelMessage("PING: " + m_host);
    sendRaw("PING :" + m_host + "\r\n");
}

void NetworkManager::parseLine(const QString& line) {
    // Handle IRCv3 message tags: @key1=val1;key2=val2 :nick!id@host CMD params
    if (line.startsWith('@')) {
        int spaceIdx = line.indexOf(' ');
        if (spaceIdx > 0) {
            QString rest = line.mid(spaceIdx + 1);
            parseMessage(rest);
            return;
        }
    }

    parseMessage(line);
}

void NetworkManager::parseMessage(const QString& line) {
    QString prefix;
    QString cmd;
    QStringList params;

    if (line.startsWith(':')) {
        int spaceIdx = line.indexOf(' ');
        if (spaceIdx <= 0) return;

        prefix = line.mid(1, spaceIdx - 1);
        QString rest = line.mid(spaceIdx + 1);

        spaceIdx = rest.indexOf(' ');
        if (spaceIdx > 0) {
            cmd = rest.mid(0, spaceIdx);
            rest = rest.mid(spaceIdx + 1);
        } else {
            cmd = rest;
        }

        while (!rest.isEmpty()) {
            while (!rest.isEmpty() && rest.startsWith(' ')) {
                rest = rest.mid(1);
            }
            if (rest.startsWith(':')) {
                int endIdx = rest.indexOf('\n');
                QString trailing = rest.mid(1);
                if (endIdx > 0) {
                    trailing = trailing.mid(0, endIdx);
                }
                params.append(trailing);
                break;
            } else {
                int endIdx = rest.indexOf(' ');
                QString param = rest;
                if (endIdx > 0) {
                    param = rest.mid(0, endIdx);
                }
                params.append(param);
                if (endIdx <= 0) {
                    break;
                }
                rest = rest.mid(endIdx);
            }
        }
    } else {
        QStringList tokens = line.split(' ', Qt::SkipEmptyParts);
        cmd = tokens.takeFirst();
        params = tokens;
    }

    if (cmd == "PING") {
        if (params.size() >= 1) {
            emit serverChannelMessage("PONG: " + params[0]);
            sendCommand("PONG", QStringList() << params[0]);
        }
    } else if (cmd == "CAP") {
        handleCapCommand(params);
    } else if (cmd == "PRIVMSG") {
        handlePrivMsg(prefix, params);
    } else if (cmd == "NICK") {
        handleNick(prefix, params);
    } else if (cmd == "JOIN") {
        handleJoin(prefix, params);
    } else if (cmd == "PART") {
        handlePart(prefix, params);
    } else if (cmd == "MODE") {
        handleMode(prefix, params);
    } else if (cmd == "TOPIC") {
        handleTopic(prefix, params);
    } else if (cmd == "QUIT") {
        handleQuit(prefix, params);
    } else if (cmd == "NOTICE") {
        handleNotice(prefix, params);
    } else if (cmd == "KICK") {
        handleKick(prefix, params);
    } else if (cmd == "PONG") {
        emit serverChannelMessage("PONG: " + params.join(' '));
    } else {
        bool ok;
        long num = cmd.toLong(&ok);
        if (ok && num >= 1 && num <= 999) {
            handleNumericReply(QString::number(num), prefix, params);
        } else {
            QString sender = prefix.isEmpty() ? "Server" : prefix.section('!', 0, 0);
            emit serverChannelMessage("[" + sender + "] " + cmd + " " + params.join(' '));
        }
    }
}

void NetworkManager::handlePrivMsg(const QString& prefix, const QStringList& params) {
    if (params.size() < 2) return;

    QString chanName = params[0];
    QString message = params[1];
    QString senderNick = prefix.section('!', 0, 0);

    IRCMessage msg(MessageType::Message, message, senderNick);
    msg.setChannel(chanName);
    emit channelMessage(chanName, msg);

    auto* ch = channel(chanName);
    if (ch) {
        ch->addMessage(msg);
    }
}

void NetworkManager::handleNotice(const QString& prefix, const QStringList& params) {
    if (params.size() < 2) return;

    QString target = params[0];
    QString text = params[1];
    QString sender = prefix.section('!', 0, 0);

    // Check if a channel tab exists for the target
    if (target.startsWith('#') && (m_channels.contains(target) || channel(target))) {
        IRCMessage msg(MessageType::Notice, text, sender);
        msg.setChannel(target);
        emit channelMessage(target, msg);
    } else {
        emit serverChannelMessage("[" + sender + "] " + text);
    }
}

void NetworkManager::handleNick(const QString& prefix, const QStringList& params) {
    if (params.isEmpty()) return;

    QString newNick = params[0];
    QString oldNick = prefix.section('!', 0, 0);

    if (oldNick == m_nick) {
        m_nick = newNick;
        emit nickSet(m_nick);
    }

    emit userChangedNick(oldNick, newNick);

    IRCMessage msg(MessageType::NickChange,
                  QString("%1 -> %2").arg(oldNick).arg(newNick),
                  oldNick);

    for (auto it = m_channels.begin(); it != m_channels.end(); ++it) {
        emit channelMessage(it.key(), msg);
    }
}

void NetworkManager::handleJoin(const QString& prefix, const QStringList& params) {
    if (params.isEmpty()) return;

    QString chanName = params[0];
    QString nick = prefix.section('!', 0, 0);

    if (nick == m_nick) {
        registerChannel(chanName);
    }

    IRCUser user(nick);

    IRCMessage msg(MessageType::Join, chanName, nick);
    emit channelMessage(chanName, msg);
    emit userJoined(chanName, user);

    auto* ch = channel(chanName);
    if (ch) {
        ch->addUser(user);
        ch->addMessage(msg);
    }
}

void NetworkManager::handlePart(const QString& prefix, const QStringList& params) {
    if (params.isEmpty()) return;

    QString chanName = params[0];
    QString nick = prefix.section('!', 0, 0);

    QString reason;
    if (params.size() >= 2) {
        reason = params[1];
    }

    IRCMessage msg(MessageType::Part, chanName + " " + reason, nick);
    emit channelMessage(chanName, msg);
    emit userLeft(chanName, nick, reason);

    auto* ch = channel(chanName);
    if (ch) {
        ch->removeUser(nick);
        ch->addMessage(msg);
    }
}

void NetworkManager::handleMode(const QString& prefix, const QStringList& params) {
    if (params.size() < 2) return;

    QString target = params[0];
    QString mode = params[1];

    emit channelMode(target, mode);

    auto* ch = channel(target);
    if (ch) {
        ch->applyMode(mode);
    }

    IRCMessage msg(MessageType::Mode, mode, prefix.section('!', 0, 0));
    emit channelMessage(target, msg);
}

void NetworkManager::handleTopic(const QString& prefix, const QStringList& params) {
    if (params.size() < 2) return;

    QString chanName = params[0];
    QString topic = params[1];

    emit channelTopic(chanName, topic);

    auto* ch = channel(chanName);
    if (ch) {
        ch->setTopic(topic);
    }

    IRCMessage msg(MessageType::Topic, topic, prefix.section('!', 0, 0));
    emit channelMessage(chanName, msg);
}

void NetworkManager::handleQuit(const QString& prefix, const QStringList& params) {
    QString nick = prefix.section('!', 0, 0);
    QString reason = params.isEmpty() ? "" : params[0];

    IRCMessage msg(MessageType::Quit, "Quit", nick);

    for (auto it = m_channels.begin(); it != m_channels.end(); ++it) {
        emit channelMessage(it.key(), msg);
    }

    emit userLeft(m_currentChannel, nick, reason);
}

void NetworkManager::handleKick(const QString& prefix, const QStringList& params) {
    if (params.size() < 2) return;

    QString kicker = prefix.section('!', 0, 0);
    QString chanName = params[0];
    QString kicked = params[1];
    QString reason;
    if (params.size() >= 3) {
        reason = params[2];
    }

    emit userLeft(chanName, kicked, reason);

    auto* ch = channel(chanName);
    if (ch) {
        ch->removeUser(kicked);
    }

    IRCMessage msg(MessageType::Kick, "Kicked by " + kicker, chanName);

    emit channelMessage(chanName, msg);
}

void NetworkManager::handleCapCommand(const QStringList& params) {
    if (params.size() < 2) return;

    QString action = params[1].toUpper();

    if (action == "LS") {
        QStringList caps = params.mid(2);
        for (const auto& cap : caps) {
            QString capName = cap;
            if (capName.startsWith('*')) {
                capName = capName.mid(1);
            }
        }
        QStringList acceptedCaps;
        for (const auto& cap : m_capSupported) {
            acceptedCaps.append(cap);
        }
        emit serverChannelMessage("Capabilities requested: " + acceptedCaps.join(", "));
        sendCommand("CAP", QStringList() << "REQ" << acceptedCaps.join(' '));
    } else if (action == "ACK") {
        emit serverChannelMessage("Capabilities acknowledged");
        sendRaw("CAP END\r\n");
    } else if (action == "NAK") {
        emit serverChannelMessage("Capabilities rejected");
        sendRaw("CAP END\r\n");
    }
}

void NetworkManager::handleNumericReply(const QString& numeric, [[maybe_unused]] const QString& prefix, const QStringList& params) {
    bool ok;
    long num = numeric.toLong(&ok);
    if (!ok) return;

    if (num == 2) {
        QString welcome = params.value(1, "");
        if (welcome.startsWith(':')) {
            welcome = welcome.mid(1);
        }
        emit serverChannelMessage("RPL 2 (Welcome): " + welcome);
    } else if (num == 375) {
        QString motd = params.value(1, "");
        if (motd.startsWith(':')) {
            motd = motd.mid(1);
        }
        emit serverChannelMessage("RPL 375 (MOTD): " + motd);
    } else if (num == 376) {
        emit serverChannelMessage("RPL 376 (End of MOTD)");
        if (!m_currentChannel.isEmpty()) {
            joinChannel(m_currentChannel);
            m_currentChannel.clear();
        }
    } else if (num == 436) {
        emit serverChannelMessage("RPL 436 (Caps fail)");
    } else if (num == 422) {
        emit serverChannelMessage("RPL 422 (End of MOTD)");
        if (!m_currentChannel.isEmpty()) {
            joinChannel(m_currentChannel);
            m_currentChannel.clear();
        }
    } else if (num == 366) {
        QString channel = params.value(0, "");
        emit namesComplete(channel);
        sendCommand("TOPIC", QStringList() << channel);
        sendCommand("MODE", QStringList() << channel);
        emit serverChannelMessage("RPL 366 (End of channel name): " + channel);
    } else if (num == 324) {
        if (params.size() >= 2) {
            emit channelMode(params[0], params[1]);
        }
        emit serverChannelMessage("RPL 324 (Channel mode): " + params.value(0, "") + " " + params.value(1, ""));
    } else if (num == 332) {
        QString channel = params.value(0, "");
        QString topic = params.value(1, "");
        if (topic.startsWith(':')) {
            topic = topic.mid(1);
        }
        emit channelTopic(channel, topic);
    } else if (num == 333) {
        QString setter = params.value(2, "");
        QString timestamp = params.value(3, "");
        emit serverChannelMessage("RPL 333 (Topic set by): " + setter + " at " + timestamp);
    } else if (num == 329) {
        QString timestamp = params.value(2, "");
        emit serverChannelMessage("RPL 329 (Channel created): " + timestamp);
    } else if (num == 367) {
        QString banInfo = params.mid(1).join(' ');
        if (banInfo.startsWith(':')) {
            banInfo = banInfo.mid(1);
        }
        emit serverChannelMessage("RPL 367 (Channel bans): " + banInfo);
    } else if (num == 368) {
        QString excInfo = params.mid(1).join(' ');
        if (excInfo.startsWith(':')) {
            excInfo = excInfo.mid(1);
        }
        emit serverChannelMessage("RPL 368 (Channel ban exceptions): " + excInfo);
    } else if (num == 401) {
        emit serverError("No such nick: " + params.value(0, ""));
    } else if (num == 433) {
        setNick(m_nick + "_");
    } else if (num == 315) {
        emit namesComplete(params.value(0, ""));
        emit serverChannelMessage("RPL 315 (End of names): " + params.value(0, ""));
    } else if (num == 353) {
        QString channel = params.value(0, "");
        QString users = params.value(2, "");
        QStringList userParts = users.split(' ');
        QList<IRCUser> userList;
        for (const QString& u : userParts) {
            QString nick = u;
            QString mode = "";
            if (!nick.isEmpty() && !nick.startsWith('#')) {
                QChar first = nick.front();
                if (first == '@' || first == '+' || first == '~' || first == '%' || first == '&') {
                    mode = QString(first);
                    nick = nick.mid(1);
                }
                if (!nick.isEmpty()) {
                    emit userReceived(channel, nick);
                    IRCUser user(nick);
                    user.setMode(mode);
                    userList.append(user);
                }
            }
        }
        emit namesReceived(channel, userList);
        emit serverChannelMessage("RPL 353 (Users list): " + channel + " " + users);
    } else if (num == 331) {
        QString text = params.value(1, "");
        if (text.startsWith(':')) {
            text = text.mid(1);
        }
        emit serverChannelMessage("RPL 331 (Channel topic): " + text);
    } else if (num >= 400 && num <= 420) {
        QString text2 = params.value(1, "");
        if (text2.startsWith(':')) {
            text2 = text2.mid(1);
        }
        emit serverChannelMessage("RPL " + QString::number(num) + ": " + text2);
    } else if (num >= 421 && num <= 499) {
        QString errText = params.value(1, "");
        if (errText.startsWith(':')) {
            errText = errText.mid(1);
        }
        emit serverError("RPL " + QString::number(num) + ": " + errText);
   } else if (num == 5) {
        QStringList displayParams = params.mid(1);
        if (!displayParams.isEmpty() && displayParams.last().startsWith(':')) {
            displayParams.last() = displayParams.last().mid(1);
        }
        emit serverChannelMessage("RPL 005 (ISUPPORT): " + displayParams.join(' '));
    } else {
        QStringList displayParams = params.mid(1);
        if (!displayParams.isEmpty() && displayParams.last().startsWith(':')) {
            displayParams.last() = displayParams.last().mid(1);
        }
        emit serverChannelMessage("RPL " + QString::number(num) + ": " + displayParams.join(' '));
    }
}

void NetworkManager::sendRaw(const QString& data) {
    m_socket->write(data.toUtf8());
    m_socket->flush();
}

void NetworkManager::sendCommand(const QString& cmd, const QStringList& params) {
    QString line = cmd;
    int i = 0;

    for (; i < params.size() - 1; ++i) {
        line += " " + params[i];
    }

    if (i < params.size()) {
        line += " :" + params[i];
    }

    line += "\r\n";
    sendRaw(line);
}
