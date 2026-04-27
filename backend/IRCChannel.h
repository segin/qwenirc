#ifndef IRCCHANNEL_H
#define IRCCHANNEL_H

#include "IRCUser.h"
#include "IRCMessage.h"
#include <QString>
#include <QList>
#include <QSet>

class IRCChannel {
public:
    explicit IRCChannel(const QString& name);

    QString name() const { return m_name; }
    QString topic() const { return m_topic; }
    QList<IRCUser> users() const { return m_users; }
    const IRCUser* findUser(const QString& nick) const;
    QList<IRCMessage> messages() const { return m_messages; }
    QString prefix() const { return m_prefix; }

    void setTopic(const QString& topic);
    void setPrefix(const QString& prefix);
    void addUser(const IRCUser& user);
    void removeUser(const QString& nick);
    void addMessage(const IRCMessage& msg);
    void clear();
    void applyMode(const QString& modeStr);

private:
    static const int MAX_MESSAGES = 10000;
    QString m_name;
    QString m_topic;
    QString m_prefix;
    QList<IRCUser> m_users;
    QList<IRCMessage> m_messages;
    QSet<QString> m_userSet;
};

#endif // IRCCHANNEL_H
