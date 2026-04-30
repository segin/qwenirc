#include "IRCUser.h"

IRCUser::IRCUser(const QString& nick, const QString& ident, const QString& host)
    : m_nick(nick), m_ident(ident), m_host(host) {
}
