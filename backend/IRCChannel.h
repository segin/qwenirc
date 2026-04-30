#ifndef IRCCHANNEL_H
#define IRCCHANNEL_H

#include "IRCMessage.h"
#include "IRCMessageModel.h"
#include "IRCUser.h"
#include <QList>
#include <QSet>
#include <QString>

class IRCChannel {
public:
    explicit IRCChannel(const QString& name);

    QString name() const { return m_name; }
    QString topic() const { return m_topic; }
    QList<IRCUser> users() const { return m_users; }
    IRCUser* findUser(const QString& nick);
    int findUserIndex(const QString& nick) const;
    IRCUser* userAt(int index);
    const IRCUser* userAt(int index) const;
    QList<IRCMessage> messages() const { return m_messages; }
    QString prefix() const { return m_prefix; }

    void setTopic(const QString& topic);
    void setPrefix(const QString& prefix);
    void addUser(const IRCUser& user);
    void removeUser(const QString& nick);
    void addMessage(const IRCMessage& msg);
    void clear();
    void applyMode(const QString& modeStr, const QStringList& modeParams, const QString& setter,
                   const QString& prefixSpec = "");

private:
    QString m_name;
    QString m_topic;
    QString m_prefix;
    QList<IRCUser> m_users;
    QList<IRCMessage> m_messages;
    QSet<QString> m_userSet;
};

#endif // IRCCHANNEL_H
