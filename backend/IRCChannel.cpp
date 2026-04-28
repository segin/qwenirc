#include "IRCChannel.h"
#include <QRegularExpression>

IRCChannel::IRCChannel(const QString& name)
    : m_name(name)
{
}

IRCUser* IRCChannel::findUser(const QString& nick) {
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
    if (m_userSet.contains(user.nick())) return;
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

 void IRCChannel::applyMode(const QString& modeStr, const QStringList& modeParams, const QString& setter) {
    Q_UNUSED(setter);
    int paramIdx = 0;

    for (int i = 0; i < modeStr.size(); ++i) {
        QChar c = modeStr[i];
        bool isAdd = (c == '+');
        QChar modeChar = (isAdd || c == '-') ? c : c;
        if (isAdd || c == '-') {
            if (modeChar == 'o' || modeChar == 'v' || modeChar == 'h' || modeChar == 'q') {
                if (paramIdx < modeParams.size()) {
                    IRCUser* user = findUser(modeParams[paramIdx]);
                    if (user) {
                        user->setUserPrefix(isAdd ? QString(modeChar) : QString());
                    }
                    paramIdx++;
                }
            } else {
                // Channel-level mode (k, l, b, e, d, etc.): skip associated parameter
                if (modeChar == 'k' || modeChar == 'l' || modeChar == 'b' || modeChar == 'e' || modeChar == 'd') {
                    if (paramIdx < modeParams.size()) paramIdx++;
                }
            }
        }
    }
}
