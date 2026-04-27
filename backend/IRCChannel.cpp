#include "IRCChannel.h"
#include <QRegularExpression>

IRCChannel::IRCChannel(const QString& name)
    : m_name(name)
{
}

const IRCUser* IRCChannel::findUser(const QString& nick) const {
    for (int i = 0; i < m_users.size(); ++i) {
        if (m_users[i].nick() == nick) {
            return &m_users[i];
        }
    }
   return nullptr;
}

void IRCChannel::setTopic(const QString& topic) {
    m_topic = topic;
}

void IRCChannel::setPrefix(const QString& prefix) {
    m_prefix = prefix;
}

void IRCChannel::addUser(const IRCUser& user) {
    m_userSet.insert(user.nick());
    m_users.append(user);
}

void IRCChannel::removeUser(const QString& nick) {
    for (int i = 0; i < m_users.size(); ++i) {
        if (m_users[i].nick() == nick) {
            m_users.removeAt(i);
            m_userSet.remove(nick);
            return;
        }
    }
}

void IRCChannel::addMessage(const IRCMessage& msg) {
    m_messages.append(msg);
    if (m_messages.size() > IRCChannel::MAX_MESSAGES) {
        m_messages.removeFirst();
    }
}

void IRCChannel::clear() {
    m_topic.clear();
    m_users.clear();
    m_messages.clear();
    m_userSet.clear();
}

void IRCChannel::applyMode(const QString& modeStr) {
    QRegularExpression modeRegex(R"([+\-][a-zA-Z]+)");
    QRegularExpressionMatchIterator it = modeRegex.globalMatch(modeStr);
    while (it.hasNext()) {
        QRegularExpressionMatch match = it.next();
        QString mode = match.captured();
        bool isAdd = mode.startsWith('+');
        QString modeType = mode.mid(1);

        if (isAdd && (modeType == "o" || modeType == "v")) {
            m_prefix = modeType;
        } else if (!isAdd && m_prefix.contains(modeType)) {
            m_prefix.remove(modeType);
        }
    }
}
