#ifndef IRCUSER_H
#define IRCUSER_H

#include <QString>

class IRCUser {
public:
    IRCUser() = default;
    IRCUser(const QString& nick, const QString& ident = {}, const QString& host = {});
    ~IRCUser() = default;

    IRCUser(const IRCUser& other) = default;
    IRCUser& operator=(const IRCUser& other) = default;
    IRCUser(IRCUser&&) = default;
    IRCUser& operator=(IRCUser&&) = default;

    bool operator==(const IRCUser& other) const {
        return m_nick == other.m_nick && m_ident == other.m_ident && m_host == other.m_host &&
               m_userPrefix == other.m_userPrefix;
    }

    QString nick() const { return m_nick; }
    QString ident() const { return m_ident; }
    QString host() const { return m_host; }
    QString userPrefix() const { return m_userPrefix; }
    bool operator<(const IRCUser& other) const { return m_nick < other.m_nick; }

    void setNick(const QString& nick) { m_nick = nick; }
    void setIdent(const QString& ident) { m_ident = ident; }
    void setHost(const QString& host) { m_host = host; }
    void setUserPrefix(const QString& prefix) { m_userPrefix = prefix; }

private:
    QString m_nick;
    QString m_ident;
    QString m_host;
    QString m_userPrefix;
};

#endif // IRCUSER_H
