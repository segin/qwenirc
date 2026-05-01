#include "NetworkManager.h"
#include <QDebug>
#include <QSslSocket>

NetworkManager::NetworkManager(QObject* parent)
    : QObject(parent), m_socket(new QTcpSocket(this)), m_state(Disconnected), m_pingTimer(new QTimer(this)),
      m_capReqTimer(new QTimer(this)), m_prefixSymbols({'@', '%', '+'}) {
    connect(m_socket, &QAbstractSocket::connected, this, &NetworkManager::onConnected);
    connect(m_socket, &QAbstractSocket::disconnected, this, &NetworkManager::onDisconnected);
    connect(m_socket, &QAbstractSocket::readyRead, this, &NetworkManager::onReadyRead);
    connect(m_socket, QOverload<QAbstractSocket::SocketError>::of(&QAbstractSocket::errorOccurred), this,
            &NetworkManager::onError);

    connect(m_pingTimer, &QTimer::timeout, this, &NetworkManager::onPingTimeout);
    m_pingTimer->setInterval(3 * 60 * 1000);

    connect(m_capReqTimer, &QTimer::timeout, this, &NetworkManager::onCapReqTimeout);

    m_capSupported = {"sasl", "server-time", "echo-message", "multi-prefix", "away-notify", "account-notify"};
}

NetworkManager::~NetworkManager() {
    for (auto* channel : m_channels) {
        delete channel;
    }
    m_channels.clear();
    closeTrafficLog();
}

void NetworkManager::connectToServer(const QString& host, quint16 port, const QString& nick, const QString& pass,
                                     const QString& channel, bool useTLS) {
    // Abort any existing hanging connection to prevent silently ignored attempts
    if (m_socket->state() == QAbstractSocket::ConnectingState || m_socket->state() == QAbstractSocket::ConnectedState) {
        m_socket->abort();
    }

    m_host = host;
    m_port = port;
    m_nick = nick;
    m_pass = pass;
    m_currentChannel = channel;
    m_state = Connecting;
    emit stateChanged(m_state);

    if (useTLS) {
        m_socket->blockSignals(true);
        m_socket->disconnect();
        delete m_socket;
        QSslSocket* ssl = new QSslSocket(this);
        m_socket = ssl;
        connect(ssl, &QSslSocket::encrypted, this, &NetworkManager::onConnected);
        connect(ssl, &QAbstractSocket::disconnected, this, &NetworkManager::onDisconnected);
        connect(ssl, &QAbstractSocket::readyRead, this, &NetworkManager::onReadyRead);
        connect(ssl, QOverload<QAbstractSocket::SocketError>::of(&QAbstractSocket::errorOccurred), this,
                &NetworkManager::onError);
        connect(ssl, &QSslSocket::sslErrors, this, &NetworkManager::onSslErrors);
        ssl->connectToHost(host, port);
        ssl->startClientEncryption();
    } else {
        m_socket->connectToHost(host, port);
    }
}

void NetworkManager::disconnect() {
    m_socket->disconnectFromHost();
}

void NetworkManager::sendMessage(const QString& channel, const QString& message) {
    sendCommand("PRIVMSG", QStringList() << channel << message);
}

void NetworkManager::joinChannel(const QString& channel) {
    sendCommand("JOIN", QStringList() << channel);
}

void NetworkManager::sendUserInput(const QString& context, const QString& text) {
    if (text.startsWith('/')) {
        QStringList parts = text.mid(1).split(' ', Qt::SkipEmptyParts);
        if (parts.isEmpty())
            return;

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
            emit queryTabNeeded(parts[1]);
        } else if (cmd == "QUOTE" || cmd == "RAW") {
            if (parts.size() >= 2)
                sendRaw(parts.mid(1).join(' ') + "\r\n");
        } else if (cmd == "TOPIC") {
            QChar chanType = m_isupport["CHANTYPES"].isEmpty() ? QChar('#') : m_isupport["CHANTYPES"].front();
            if (!context.isEmpty() && context != "Server" && context[0] == chanType) {
                if (parts.size() >= 2) {
                    setTopic(context, parts.mid(1).join(' '));
                } else {
                    sendCommand("TOPIC", QStringList() << context);
                }
            } else {
                emit serverChannelMessage("Error: /topic is only valid inside a channel tab.");
            }
        } else if (cmd == "ME" && parts.size() >= 2) {
            if (!context.isEmpty() && context != "Server") {
                sendCommand("PRIVMSG", QStringList() << context << "\001ACTION " + parts.mid(1).join(' ') + "\001");
            }
        } else {
            sendCommand(cmd, parts.mid(1));
        }
    } else {
        if (!context.isEmpty() && context != "Server") {
            sendMessage(context, text);

            // Only emit local echo when echo-message is not active
            if (!m_activeCaps.contains("echo-message")) {
                IRCMessage msg(MessageType::Message, text, m_nick);
                msg.setChannel(context);
                emit channelMessage(context, msg);
            }
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

NetworkManager::State NetworkManager::state() const {
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
    sendRaw("CAP LS 302\r\n");

    // Don't send NICK/USER yet - wait for CAP END
    // Registration will happen in handleCapCommand when we receive CAP END
}

void NetworkManager::onDisconnected() {
    m_state = Disconnected;
    emit stateChanged(m_state);
    emit disconnected();
    m_pingTimer->stop();
}

void NetworkManager::onReadyRead() {
    QByteArray data = m_socket->readAll();
    m_lineBuffer += data;
    logTraffic(QString::fromUtf8(data), false);

    const int MAX_BUFFER_SIZE = 10 * 1024 * 1024;
    if (m_lineBuffer.size() > MAX_BUFFER_SIZE) {
        m_lineBuffer = m_lineBuffer.right(m_lineBuffer.size() / 2);
    }

    while (true) {
        int endPos = m_lineBuffer.indexOf('\n');
        if (endPos < 0)
            return;

        QByteArray rawLine = m_lineBuffer.left(endPos);
        m_lineBuffer = m_lineBuffer.mid(endPos + 1);

        QString line = QString::fromUtf8(rawLine);
        line.remove('\r');
        if (line.contains(QChar(0xFFFD))) {
            line = QString::fromLatin1(rawLine);
        }
        if (line.isEmpty())
            continue;

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

void NetworkManager::onSslErrors(const QList<QSslError>& errors) {
    if (errors.isEmpty()) {
        emit serverError("SSL error");
        return;
    }
    for (const auto& e : errors) {
        emit serverError("SSL: " + e.errorString());
    }
    m_socket->disconnectFromHost();
}

void NetworkManager::onPingTimeout() {
    sendRaw("PING :" + m_host + "\r\n");
}

void NetworkManager::onCapReqTimeout() {
    if (m_serverCaps.isEmpty())
        return;

    QStringList acceptedCaps;
    for (const auto& cap : m_capSupported) {
        if (m_serverCaps.contains(cap)) {
            acceptedCaps.append(cap);
        }
    }

    if (!acceptedCaps.isEmpty()) {
        sendCommand("CAP", QStringList() << "REQ" << acceptedCaps.join(' '));
    } else {
        sendRaw("CAP END\r\n");
        sendRegistration();
    }
    m_serverCaps = QSet<QString>();
    m_capReqTimer->stop();
}

void NetworkManager::parseLine(const QString& line) {
    // Handle IRCv3 message tags: @key1=val1;key2=val2 :nick!id@host CMD params
    QString serverTime;
    if (line.startsWith('@')) {
        int spaceIdx = line.indexOf(' ');
        if (spaceIdx > 0) {
            QString tags = line.mid(1, spaceIdx - 1);
            QString rest = line.mid(spaceIdx + 1);

            // Extract server-time tag
            QStringList tagPairs = tags.split(';', Qt::SkipEmptyParts);
            for (const QString& tag : tagPairs) {
                int eq = tag.indexOf('=');
                QString key = (eq < 0) ? tag : tag.left(eq);
                QString value = (eq < 0) ? QString() : tag.mid(eq + 1);
                if (key == "time") {
                    serverTime = value;
                }
            }
            parseMessage(rest, serverTime);
            return;
        }
    }

    parseMessage(line, serverTime);
}

void NetworkManager::parseMessage(const QString& line, const QString& serverTime) {
    QString prefix;
    QString cmd;
    QStringList params;

    if (line.startsWith(':')) {
        int spaceIdx = line.indexOf(' ');
        if (spaceIdx <= 0)
            return;

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
                QString trailing = rest.mid(1);
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
        for (int i = 0; i < params.size(); ++i) {
            if (params[i].startsWith(':')) {
                QString trailing = params.takeAt(i);
                if (trailing.startsWith(':'))
                    trailing = trailing.mid(1);
                while (i < params.size()) {
                    trailing += ' ' + params.takeAt(i);
                    ++i;
                }
                params.append(trailing);
                break;
            }
        }
    }

    if (cmd == "PING") {
        if (params.size() >= 1) {
            sendCommand("PONG", QStringList() << params[0]);
        }
    } else if (cmd == "CAP") {
        handleCapCommand(params);
    } else if (cmd == "PRIVMSG") {
        handlePrivMsg(prefix, params, serverTime);
    } else if (cmd == "NICK") {
        handleNick(prefix, params, serverTime);
    } else if (cmd == "JOIN") {
        handleJoin(prefix, params, serverTime);
    } else if (cmd == "PART") {
        handlePart(prefix, params, serverTime);
    } else if (cmd == "MODE") {
        handleMode(prefix, params);
    } else if (cmd == "TOPIC") {
        handleTopic(prefix, params, serverTime);
    } else if (cmd == "QUIT") {
        handleQuit(prefix, params, serverTime);
    } else if (cmd == "NOTICE") {
        handleNotice(prefix, params, serverTime);
    } else if (cmd == "KICK") {
        handleKick(prefix, params, serverTime);
    } else if (cmd == "PONG") {
        // silently consume keepalive PONG
    } else {
        bool ok;
        long num = cmd.toLong(&ok);
        if (ok && num >= 1 && num <= 999) {
            handleNumericReply(QString::number(num), prefix, params, serverTime);
        } else {
            QString sender = prefix.isEmpty() ? "Server" : prefix.section('!', 0, 0);
            emit serverChannelMessage("[" + sender + "] " + cmd + " " + params.join(' '));
        }
    }
}

void NetworkManager::handlePrivMsg(const QString& prefix, const QStringList& params, const QString& serverTime) {
    if (params.size() < 2)
        return;

    QString chanName = params[0];
    QString message = params[1];
    QString senderNick = prefix.section('!', 0, 0);
    bool isPrivateMsg = (chanName == m_nick);

    IRCMessage msg(MessageType::Message, message, senderNick);
    msg.setChannel(chanName);
    if (!serverTime.isEmpty()) {
        msg.setTimestamp(QDateTime::fromString(serverTime, Qt::ISODate));
    }

    // Handle CTCP messages (content wrapped in \001 delimiters)
    if (isCtcpMessage(message)) {
        QString ctcpCommand;
        QString ctcpText;
        parseCtcpMessage(message, ctcpCommand, ctcpText);

        if (ctcpCommand.toUpper() == "VERSION") {
            sendCtcpVersionReply(senderNick);
            emit ctcpRequest(senderNick, "VERSION", ctcpText);
        } else if (ctcpCommand.toUpper() == "FINGER") {
            emit ctcpRequest(senderNick, "FINGER", ctcpText);
        } else if (ctcpCommand.toUpper() == "ACTION") {
            IRCMessage actionMsg(MessageType::Message, "*" + ctcpText + "*", senderNick);
            actionMsg.setChannel(chanName);
            if (!serverTime.isEmpty()) {
                actionMsg.setTimestamp(QDateTime::fromString(serverTime, Qt::ISODate));
            }
            if (isPrivateMsg) {
                emit channelMessage(senderNick, actionMsg);
            } else {
                emit channelMessage(chanName, actionMsg);
            }

            auto* ch = channel(chanName);
            if (ch) {
                ch->addMessage(actionMsg);
            }
        } else {
            emit ctcpRequest(senderNick, ctcpCommand, ctcpText);
        }
        return;
    }

    // Route private messages (target is own nick) to query tab
    if (isPrivateMsg) {
        emit channelMessage(senderNick, msg);
        emit queryTabNeeded(senderNick);
    } else {
        emit channelMessage(chanName, msg);

        auto* ch = channel(chanName);
        if (ch) {
            ch->addMessage(msg);
        }
    }
}

void NetworkManager::handleNotice(const QString& prefix, const QStringList& params, const QString& serverTime) {
    if (params.size() < 2)
        return;

    QString target = params[0];
    QString text = params[1];
    QString sender = prefix.section('!', 0, 0);

    // Handle CTCP notices
    if (isCtcpMessage(text)) {
        QString ctcpCommand;
        QString ctcpText;
        parseCtcpMessage(text, ctcpCommand, ctcpText);

        if (ctcpCommand.toUpper() == "VERSION") {
            emit ctcpReply(sender, "VERSION", ctcpText);
        } else {
            emit ctcpReply(sender, ctcpCommand, ctcpText);
        }
        return;
    }

    // Check if a channel tab exists for the target
    IRCMessage msg(MessageType::Notice, text, sender);
    msg.setChannel(target);
    if (!serverTime.isEmpty()) {
        msg.setTimestamp(QDateTime::fromString(serverTime, Qt::ISODate));
    }

    QChar chantype = m_isupport["CHANTYPES"].isEmpty() ? QChar('#') : m_isupport["CHANTYPES"].front();
    bool isServerSender = !prefix.contains('!') && !prefix.contains('@');
    bool isPreRegTarget = (target == "*" || target == "AUTH");

    if (target.startsWith(chantype) && channel(target)) {
        emit channelMessage(target, msg);
    } else if (isServerSender || isPreRegTarget) {
        emit serverChannelMessage(QString("[%1] %2").arg(sender).arg(text));
    } else {
        emit channelMessage(sender, msg);
        emit queryTabNeeded(sender);
    }
}

void NetworkManager::handleNick(const QString& prefix, const QStringList& params, const QString& serverTime) {
    if (params.isEmpty())
        return;

    QString newNick = params[0];
    QString oldNick = prefix.section('!', 0, 0);

    if (oldNick == m_nick) {
        m_nick = newNick;
        emit nickSet(m_nick);
    }

    emit userChangedNick(oldNick, newNick);

    IRCMessage msg(MessageType::NickChange, QString("%1 -> %2").arg(oldNick).arg(newNick), oldNick);
    if (!serverTime.isEmpty()) {
        msg.setTimestamp(QDateTime::fromString(serverTime, Qt::ISODate));
    }

    QList<QString> keysToNotify;
    for (auto it = m_channels.begin(); it != m_channels.end(); ++it) {
        if (it.value()->findUser(oldNick) != nullptr) {
            keysToNotify.append(it.key());
        }
    }
    for (const QString& key : keysToNotify) {
        IRCChannel* ch = m_channels.value(key);
        if (!ch)
            continue;
        IRCUser* user = ch->findUser(oldNick);
        QString ident = user->ident();
        QString host = user->host();
        ch->removeUser(oldNick);
        ch->addUser(IRCUser(newNick, ident, host));
        emit channelMessage(key, msg);
    }
}

void NetworkManager::handleJoin(const QString& prefix, const QStringList& params, const QString& serverTime) {
    if (params.isEmpty())
        return;

    QString chanName = params[0];
    QString nick = prefix.section('!', 0, 0);

    if (nick == m_nick) {
        registerChannel(chanName);
    }

    IRCUser user(nick);
    {
        QString ident = prefix.section('!', 1, 1).section('@', 0, 0);
        QString host = prefix.section('@', 1);
        if (!ident.isEmpty()) {
            user.setIdent(ident);
        }
        if (!host.isEmpty()) {
            user.setHost(host);
        }
    }

    IRCMessage msg(MessageType::Join, chanName, nick);
    if (!serverTime.isEmpty()) {
        msg.setTimestamp(QDateTime::fromString(serverTime, Qt::ISODate));
    }

    emit channelMessage(chanName, msg);
    emit userJoined(chanName, user);

    auto* ch = channel(chanName);
    if (ch) {
        ch->addUser(user);
        ch->addMessage(msg);
    }
}

void NetworkManager::handlePart(const QString& prefix, const QStringList& params, const QString& serverTime) {
    if (params.isEmpty())
        return;

    QString chanName = params[0];
    QString nick = prefix.section('!', 0, 0);

    QString reason;
    if (params.size() >= 2) {
        reason = params[1];
    }

    IRCMessage msg(MessageType::Part, reason, nick);
    msg.setChannel(chanName);
    if (!serverTime.isEmpty()) {
        msg.setTimestamp(QDateTime::fromString(serverTime, Qt::ISODate));
    }

    emit channelMessage(chanName, msg);
    emit userLeft(chanName, nick, reason);

    auto* ch = channel(chanName);
    if (ch) {
        ch->removeUser(nick);
        ch->addMessage(msg);
    }

    if (nick == m_nick) {
        removeChannel(chanName);
    }
}

void NetworkManager::handleMode(const QString& prefix, const QStringList& params) {
    if (params.size() < 2)
        return;

    QString target = params[0];
    QString mode = params[1];
    QStringList modeParams = params.mid(2);

    QString modeStr = modeParams.isEmpty() ? mode : (mode + " " + modeParams.join(' '));
    emit channelMode(target, modeStr);

    auto* ch = channel(target);
    if (ch) {
        QString prefixSpec = m_isupport.value("PREFIX", "(ohv)@%+");
        ch->applyMode(mode, modeParams, prefix.section('!', 0, 0), prefixSpec);
    }

    IRCMessage msg(MessageType::Mode, modeStr, prefix.section('!', 0, 0));
    emit channelMessage(target, msg);
}

void NetworkManager::handleTopic(const QString& prefix, const QStringList& params, const QString& serverTime) {
    if (params.size() < 2)
        return;

    QString chanName = params[0];
    QString topic = params[1];

    emit channelTopic(chanName, topic);

    auto* ch = channel(chanName);
    if (ch) {
        ch->setTopic(topic);
    }

    IRCMessage msg(MessageType::Topic, topic, prefix.section('!', 0, 0));
    if (!serverTime.isEmpty()) {
        QString cleanTime = serverTime;
        if (cleanTime.endsWith('Z'))
            cleanTime.chop(1);
        msg.setTimestamp(QDateTime::fromString(cleanTime, Qt::ISODate));
    }
    emit channelMessage(chanName, msg);
}

void NetworkManager::handleQuit(const QString& prefix, const QStringList& params, const QString& serverTime) {
    QString nick = prefix.section('!', 0, 0);
    QString reason = params.isEmpty() ? "" : params[0];

    IRCMessage msg(MessageType::Quit, reason, nick);
    if (!serverTime.isEmpty()) {
        msg.setTimestamp(QDateTime::fromString(serverTime, Qt::ISODate));
    }

    QList<QString> channelsWithUser;
    for (auto it = m_channels.begin(); it != m_channels.end(); ++it) {
        if (it.value()->findUser(nick) != nullptr) {
            channelsWithUser.append(it.key());
        }
    }
    for (const QString& chanName : channelsWithUser) {
        IRCChannel* ch = m_channels.value(chanName);
        if (!ch) continue;
        emit channelMessage(chanName, msg);
        ch->removeUser(nick);
        emit userLeft(chanName, nick, reason);
    }
}

void NetworkManager::handleKick(const QString& prefix, const QStringList& params, const QString& serverTime) {
    if (params.size() < 2)
        return;

    QString kicker = prefix.section('!', 0, 0);
    QString chanName = params[0];
    QString kicked = params[1];
    QString reason;
    if (params.size() >= 3) {
        reason = params[2];
    }

    IRCMessage msg(
        MessageType::Kick,
        QString("%1 was kicked by %2%3").arg(kicked).arg(kicker).arg(reason.isEmpty() ? "" : " (" + reason + ")"),
        kicker);
    msg.setChannel(chanName);
    if (!serverTime.isEmpty()) {
        msg.setTimestamp(QDateTime::fromString(serverTime, Qt::ISODate));
    }

    emit channelMessage(chanName, msg);
    emit userLeft(chanName, kicked, reason);

    auto* ch = channel(chanName);
    if (ch) {
        ch->removeUser(kicked);
    }
}

void NetworkManager::handleCapCommand(const QStringList& params) {
    if (params.size() < 2)
        return;

    QString action = params[1].toUpper();

    if (action == "LS") {
        QStringList rawParams = params.mid(2);

        QSet<QString> availableCaps;
        for (const auto& param : rawParams) {
            if (param == "*")
                continue;
            QStringList tokens = param.split(' ', Qt::SkipEmptyParts);
            for (const auto& token : tokens) {
                QString capName = token;
                if (capName.startsWith('*')) {
                    capName = capName.mid(1);
                }
                availableCaps.insert(capName);
            }
        }

        m_serverCaps.unite(availableCaps);

        // Start/restart timer to send REQ after all LS lines are received
        m_capReqTimer->start(50);
    } else if (action == "ACK") {
        // Server acknowledges requested capabilities
        QStringList caps;
        if (params.size() > 2) {
            caps = params[2].split(' ', Qt::SkipEmptyParts);
        }
        for (const auto& cap : caps) {
            QString capName = cap;
            if (capName.startsWith('-')) {
                capName = capName.mid(1);
                m_activeCaps.remove(capName);
                continue;
            }
            if (capName.endsWith(':')) {
                capName = capName.left(capName.size() - 1);
            }
            int eqPos = capName.indexOf('=');
            if (eqPos >= 0) {
                capName = capName.left(eqPos);
            }
            m_activeCaps.insert(capName);
        }
        QStringList activeCapsList;
        for (const auto& cap : m_activeCaps) {
            activeCapsList.append(cap);
        }
        qDebug() << "Capabilities acknowledged:" << activeCapsList;
        sendRaw("CAP END\r\n");
        sendRegistration();
    } else if (action == "NAK") {
        qDebug() << "Capabilities rejected";
        sendRaw("CAP END\r\n");
        sendRegistration();
    }
}

void NetworkManager::handleNumericReply(const QString& numeric, const QString& prefix, const QStringList& params,
                                        const QString& serverTime) {
    bool ok;
    long num = numeric.toLong(&ok);
    if (!ok)
        return;

    if (num == 1) {
        QString assignedNick = params.value(0, "");
        if (!assignedNick.isEmpty() && assignedNick != m_nick) {
            m_nick = assignedNick;
            emit nickSet(m_nick);
        }
        QString welcome = params.value(1, "");
        if (welcome.startsWith(':'))
            welcome = welcome.mid(1);
        emit serverChannelMessage("RPL 001 (Welcome): " + welcome);
    } else if (num == 2) {
        QString welcome = params.value(0, "");
        if (welcome.startsWith(':')) {
            welcome = welcome.mid(1);
        }
        emit serverChannelMessage("RPL 2 (Welcome): " + welcome);
    } else if (num == 375) {
        QString motd = params.value(0, "");
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
        QString channel = params.value(1, "");
        emit namesComplete(channel);
        sendCommand("TOPIC", QStringList() << channel);
        sendCommand("MODE", QStringList() << channel);
    } else if (num == 324) {
        if (params.size() >= 3) {
            QString channel = params.value(1, "");
            QString mode = params.value(2, "");
            emit channelMode(channel, mode);
        }
    } else if (num == 332) {
        QString channel = params.value(1, "");
        QString topic = params.value(2, "");
        if (topic.startsWith(':')) {
            topic = topic.mid(1);
        }
        emit channelTopic(channel, topic);
        IRCMessage msg(MessageType::Topic, topic, prefix.section('!', 0, 0));
        if (!serverTime.isEmpty()) {
            QString cleanTime = serverTime;
            if (cleanTime.endsWith('Z'))
                cleanTime.chop(1);
            msg.setTimestamp(QDateTime::fromString(cleanTime, Qt::ISODate));
        }
        emit channelMessage(channel, msg);
    } else if (num == 333) {
        QString channel = params.value(1, "");
        QString setter  = params.value(2, "");
        QString tsStr   = params.value(3, "");
        QDateTime when = QDateTime::fromSecsSinceEpoch(tsStr.toLongLong());
        QString msgText = QString("Topic set by %1 on %2")
                              .arg(setter.section('!', 0, 0))
                              .arg(when.toString(Qt::ISODate));
        IRCMessage msg(MessageType::Topic, msgText, "");
        msg.setChannel(channel);
        emit channelMessage(channel, msg);
    } else if (num == 329) {
        QString channel = params.value(1, "");
        QString timestamp = params.value(2, "");
    } else if (num == 367) {
        QString channel = params.value(1, "");
    } else if (num == 368) {
        QString channel = params.value(1, "");
    } else if (num == 401) {
        QString missingNick = params.value(1, "");
        emit serverError("No such nick: " + missingNick);
    } else if (num == 433) {
        m_nickRetries++;
        if (m_nickRetries > 3) {
            emit serverError("Could not get nickname. Try again with a different nick.");
            m_nickRetries = 0;
        } else {
            setNick(m_nick + "_");
        }
    } else if (num == 315) {
        QString channel = params.value(1, "");
    } else if (num == 353) {
        QString channel = params.value(1, "");
        QString users;
        int paramIdx = 2;
        if (channel == "=" || channel == "@" || channel == "*") {
            channel = params.value(2, "");
            paramIdx = 3;
        }
        users = params.value(paramIdx, "");
        if (users.startsWith(':')) {
            users = users.mid(1);
        }
        QStringList userParts = users.split(' ');
        QStringList nonEmpty;
        for (const QString& u : userParts) {
            if (!u.isEmpty()) {
                nonEmpty.append(u);
            }
        }
        QList<IRCUser> userList;
        for (const QString& u : nonEmpty) {
            QString nick = u;
            QString mode;
            QChar modeChar = extractModePrefix(nick);
            if (!nick.isEmpty() && !nick.isEmpty() && modeChar != QChar() && nick.startsWith(modeChar)) {
                mode = QString(modeChar);
                nick = nick.mid(1);
            }
            if (!nick.isEmpty()) {
                emit userReceived(channel, nick);
                IRCUser user(nick);
                user.setUserPrefix(mode);
                userList.append(user);
            }
        }
        emit namesReceived(channel, userList);
        emit serverChannelMessage("RPL 353 (Users list): " + channel + " " + users);
    } else if (num == 331) {
        QString channel = params.value(1, "");
    } else if (num >= 400 && num <= 420) {
        QString text2 = params.value(2, "");
        if (text2.startsWith(':')) {
            text2 = text2.mid(1);
        }
    } else if (num >= 421 && num <= 499) {
        QString errText = params.value(2, "");
        if (errText.startsWith(':')) {
            errText = errText.mid(1);
        }
        emit serverError("RPL " + QString::number(num) + ": " + errText);
    } else if (num == 5) {
        for (int i = 1; i < params.size(); ++i) {
            QString token = params[i];
            if (token.startsWith(':')) {
                continue;
            }
            if (token.contains('=')) {
                int eqPos = token.indexOf('=');
                QString key = token.left(eqPos);
                QString value = token.mid(eqPos + 1);
                m_isupport.insert(key, value);
                if (key == "PREFIX") {
                    m_prefixSymbols.clear();
                    int parenEnd = value.indexOf(')');
                    if (parenEnd >= 0) {
                        QString symbols = value.mid(parenEnd + 1);
                        for (QChar c : symbols) {
                            m_prefixSymbols.insert(c);
                        }
                    }
                }
            }
        }
        emit serverChannelMessage("RPL 005 (ISUPPORT): " + params.mid(2).join(' '));
    } else {
        QStringList displayParams = params.mid(1);
        if (!displayParams.isEmpty() && displayParams.last().startsWith(':')) {
            displayParams.last() = displayParams.last().mid(1);
        }
        emit serverChannelMessage("RPL " + QString::number(num) + ": " + displayParams.join(' '));
    }
}

QChar NetworkManager::extractModePrefix(const QString& nick) {
    if (nick.isEmpty()) {
        return {};
    }

    if (m_prefixSymbols.isEmpty()) {
        return {};
    }

    if (m_prefixSymbols.contains(nick[0])) {
        return nick[0];
    }

    return {};
}

void NetworkManager::sendRaw(const QString& data) {
    logTraffic(data, true);
    m_socket->write(data.toUtf8());
    m_socket->flush();
}

bool NetworkManager::isCtcpMessage(const QString& message) {
    if (message.length() < 3)
        return false;
    return message.startsWith('\001') && message.endsWith('\001');
}

void NetworkManager::parseCtcpMessage(const QString& message, QString& command, QString& text) {
    QString inner = message.mid(1, message.length() - 2);
    int spaceIdx = inner.indexOf(' ');
    if (spaceIdx > 0) {
        command = inner.mid(0, spaceIdx);
        text = inner.mid(spaceIdx + 1);
    } else {
        command = inner;
        text = "";
    }
}

void NetworkManager::sendCtcpVersionReply(const QString& target) {
    QString reply = "\001VERSION QwenIRC 0.1.0\001";
    sendCommand("NOTICE", QStringList() << target << reply);
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

void NetworkManager::sendRegistration() {
    if (!m_pass.isEmpty()) {
        sendCommand("PASS", QStringList() << m_pass);
    }
    m_username = m_nick;
    m_realname = m_nick;
    sendCommand("NICK", QStringList() << m_nick);
    sendCommand("USER", QStringList() << m_username << "8" << "*" << m_realname);
    m_pingTimer->start();
}

void NetworkManager::setTrafficLogDir(const QString& dir) {
    m_trafficLogDir = dir;
    QDir dirObj(dir);
    if (!dirObj.exists()) {
        dirObj.mkpath(".");
    }
    closeTrafficLog();
    m_trafficLog = new QFile(dirObj.absoluteFilePath("traffic.log"));
    if (m_trafficLog->open(QFile::Append | QFile::Text)) {
        m_trafficLogStream = new QTextStream(m_trafficLog);
        *m_trafficLogStream << QString("=== Traffic log started %1 ===\n").arg(QDateTime::currentDateTime().toString());
    }
}

void NetworkManager::closeTrafficLog() {
    if (m_trafficLogStream) {
        m_trafficLogStream->flush();
        delete m_trafficLogStream;
        m_trafficLogStream = nullptr;
    }
    if (m_trafficLog) {
        m_trafficLog->close();
        delete m_trafficLog;
        m_trafficLog = nullptr;
    }
}

void NetworkManager::clearTrafficLog() {
    closeTrafficLog();
    m_trafficLog = new QFile(m_trafficLogDir + "/traffic.log");
    if (m_trafficLog->open(QFile::WriteOnly | QFile::Text)) {
        m_trafficLogStream = new QTextStream(m_trafficLog);
        *m_trafficLogStream << QString("=== Traffic log started %1 ===\n").arg(QDateTime::currentDateTime().toString());
    }
}

void NetworkManager::logTraffic(const QString& data, bool outgoing) {
    if (!m_trafficLogStream)
        return;
    QString prefix = outgoing ? "C:" : "S:";
    *m_trafficLogStream << prefix << " " << data;
    m_trafficLogStream->flush();
}
