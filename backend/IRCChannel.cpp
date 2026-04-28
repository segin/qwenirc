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
    int paramIdx = 0;
    int i = 0;
    while (i < modeStr.size()) {
        QChar c = modeStr[i];
        if (c == '+' || c == '-') {
            bool isAdd = (c == '+');
            i++;
            while (i < modeStr.size()) {
                QChar modeChar = modeStr[i];
                i++;
                if (modeChar == 'o' || modeChar == 'v') {
                    // User mode: next param is the nick
                    if (paramIdx < modeParams.size()) {
                        QString nick = modeParams[paramIdx];
                        paramIdx++;
                        IRCUser* user = findUser(nick);
                        if (user) {
                            if (isAdd) {
                                user->setUserPrefix(QString(modeChar));
                            } else {
                                user->setUserPrefix("");
                            }
                        }
                    }
                } else if (modeChar == 'k' || modeChar == 'l') {
                    // Channel key/topic limit: next param is the value
                    if (paramIdx < modeParams.size()) {
                        paramIdx++;
                    }
                } else if (modeChar == 'b' || modeChar == 'e') {
                    // Ban/exempt: next param is the ban mask
                    if (paramIdx < modeParams.size()) {
                        paramIdx++;
                    }
                }
            }
        } else {
            i++;
        }
    }
    // Store setter info for logging/debugging
    Q_UNUSED(setter);
}
