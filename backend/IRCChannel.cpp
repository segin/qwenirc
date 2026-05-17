#include "IRCChannel.h"
#include <QMap>
#include <QRegularExpression>

const QSet<QString> IRCChannel::s_modesWithParam = {"k", "l", "b", "e", "d", "I"};

QSet<QChar> IRCChannel::parseChanModes(const QString& chanModes) {
    QSet<QChar> modesWithParam;
    if (chanModes.isEmpty()) {
        for (const QString& s : s_modesWithParam) {
            if (!s.isEmpty()) {
                modesWithParam.insert(s[0]);
            }
        }
        return modesWithParam;
    }
    QStringList groups = chanModes.split(',', Qt::SkipEmptyParts);
    for (const QString& group : groups) {
        for (QChar c : group) {
            modesWithParam.insert(c);
        }
    }
    return modesWithParam;
}

void IRCChannel::classifyChanModes(const QString& chanModes, QSet<QChar>& typeA, QSet<QChar>& typeB, QSet<QChar>& typeC) {
    QStringList groups = chanModes.split(',', Qt::SkipEmptyParts);
    for (int i = 0; i < groups.size() && i < 3; ++i) {
        for (QChar c : groups[i]) {
            if (i == 0) {
                typeA.insert(c);
            } else if (i == 1) {
                typeB.insert(c);
            } else {
                typeC.insert(c);
            }
        }
    }
}

IRCChannel::IRCChannel(const QString& name) : m_name(name) {
}

IRCUser* IRCChannel::findUser(const QString& nick) {
    for (int i = 0; i < m_users.size(); ++i) {
        if (m_users[i].nick().toLower() == nick.toLower()) {
            return &m_users[i];
        }
    }
    return nullptr;
}

int IRCChannel::findUserIndex(const QString& nick) const {
    for (int i = 0; i < m_users.size(); ++i) {
        if (m_users[i].nick().toLower() == nick.toLower())
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
    QString lowerNick = user.nick().toLower();
    for (const QString& existing : m_userSet) {
        if (existing.toLower() == lowerNick) {
            return;
        }
    }
    m_userSet.insert(user.nick());
    m_users.append(user);
}

void IRCChannel::removeUser(const QString& nick) {
    for (int i = 0; i < m_users.size(); ++i) {
        if (m_users[i].nick().toLower() == nick.toLower()) {
            QString removedNick = m_users[i].nick();
            m_users.removeAt(i);
            m_userSet.remove(removedNick);
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
                            const QString& prefixSpec, const QString& chanModes) {
    Q_UNUSED(setter);
    int paramIdx = 0;
    QMap<QChar, QChar> modeMap;

    // Parse PREFIX spec to build mode-letter -> symbol map
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

    QSet<QChar> typeA, typeB, typeC;
    classifyChanModes(chanModes, typeA, typeB, typeC);

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

        if (modeMap.contains(c)) {
            if (paramIdx < modeParams.size()) {
                int idx = findUserIndex(modeParams[paramIdx]);
                IRCUser* user = userAt(idx);
                if (user) {
                    user->setUserPrefix(adding ? QString(modeMap[c]) : QString());
                }
                paramIdx++;
            }
        } else {
            bool takesParam = typeA.contains(c) || typeB.contains(c)
                              || (typeC.contains(c) && adding);
            if (takesParam && paramIdx < modeParams.size()) {
                paramIdx++;
            }
        }
    }
}
