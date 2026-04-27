#ifndef IRCUSER_H
#define IRCUSER_H

#include <QString>

class IRCUser {
public:
    IRCUser() = default;
    IRCUser(const QString& nick, const QString& ident = {}, const QString& host = {});

    QString nick() const { return m_nick; }
    QString ident() const { return m_ident; }
    QString host() const { return m_host; }
    QString mode() const { return m_mode; }
    bool operator<(const IRCUser& other) const { return m_nick < other.m_nick; }

    void setNick(const QString& nick) { m_nick = nick; }
    void setIdent(const QString& ident) { m_ident = ident; }
    void setHost(const QString& host) { m_host = host; }
    void setMode(const QString& mode) { m_mode = mode; }

private:
    QString m_nick;
    QString m_ident;
    QString m_host;
    QString m_mode;
};

#endif // IRCUSER_H
