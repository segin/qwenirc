#include "IRCChannel.h"
#include <QRegularExpression>

IRCChannel::IRCChannel(const QString& name) : m_name(name) {
}

IRCUser* IRCChannel::findUser(const QString& nick) {
    for (int i = 0; i < m_users.size(); ++i) {
        if (m_users[i].nick() == nick) {
            return &m_users[i];
        }
    }
    return nullptr;
}

int IRCChannel::findUserIndex(const QString& nick) const {
    for (int i = 0; i < m_users.size(); ++i) {
        if (m_users[i].nick() == nick)
            return i;
    }
    return -1;
}

IRCUser* IRCChannel::userAt(int index) {
    if (index < 0 || index >= m_users.size())
        return nullptr;
    return &m_users[index];
}

const IRCUser* IRCChannel::userAt(int index) const {
    if (index < 0 || index >= m_users.size())
        return nullptr;
    return &m_users[index];
}

void IRCChannel::setTopic(const QString& topic) {
    m_topic = topic;
}

void IRCChannel::setPrefix(const QString& prefix) {
    m_prefix = prefix;
}

void IRCChannel::addUser(const IRCUser& user) {
    if (m_userSet.contains(user.nick()))
        return;
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
    if (m_messages.size() > IRCMessageModel::MAX_MESSAGES) {
        m_messages.removeFirst();
    }
}

void IRCChannel::clear() {
    m_topic.clear();
    m_users.clear();
    m_messages.clear();
    m_userSet.clear();
}

void IRCChannel::applyMode(const QString& modeStr, const QStringList& modeParams, const QString& setter,
                           const QString& prefixSpec) {
    Q_UNUSED(setter);
    int paramIdx = 0;

    // Parse PREFIX spec to build mode-letter -> symbol map
    // Format: (letters)symbols, e.g. (ohv)@%+
    QMap<QChar, QChar> modeMap;
    int parenIdx = prefixSpec.indexOf('(');
    int parenClose = prefixSpec.indexOf(')');
    if (parenIdx >= 0 && parenClose > parenIdx) {
        QString letters = prefixSpec.mid(parenIdx + 1, parenClose - parenIdx - 1);
        QString symbols = prefixSpec.mid(parenClose + 1);
        for (int i = 0; i < letters.size() && i < symbols.size(); ++i) {
            modeMap[letters[i]] = symbols[i];
        }
    }

    // Default mapping: o->@, h->%, v->+
    if (modeMap.isEmpty()) {
        modeMap['o'] = '@';
        modeMap['h'] = '%';
        modeMap['v'] = '+';
        modeMap['q'] = '~';
    }

    bool adding = true;
    for (int i = 0; i < modeStr.size(); ++i) {
        QChar c = modeStr[i];
        if (c == '+') {
            adding = true;
            continue;
        }
        if (c == '-') {
            adding = false;
            continue;
        }

        // c is a mode letter
        if (modeMap.contains(c)) {
            // User mode — consumes a param
            if (paramIdx < modeParams.size()) {
                int idx = findUserIndex(modeParams[paramIdx]);
                IRCUser* user = userAt(idx);
                if (user) {
                    user->setUserPrefix(adding ? QString(modeMap[c]) : QString());
                }
                paramIdx++;
            }
        } else {
            // Channel-level mode — may consume a param
            QSet<QString> modesWithParam = {"k", "l", "b", "e", "d", "I"};
            if (modesWithParam.contains(QString(c))) {
                if (paramIdx < modeParams.size())
                    paramIdx++;
            }
        }
    }
}
