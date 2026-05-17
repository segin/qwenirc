#ifndef IRCMODELS_H
#define IRCMODELS_H

#include "IRCChannel.h"
#include "IRCMessage.h"
#include "IRCUser.h"
#include <QAbstractListModel>
#include <QList>
#include <QSet>

class IRCMessageModel : public QAbstractListModel {
    Q_OBJECT

public:
    explicit IRCMessageModel(QObject* parent = nullptr);

    void addMessage(const IRCMessage& msg);
    void clear();
    void insertSystemMessage(const QString& text);

    int rowCount(const QModelIndex& parent = {}) const override;
    QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const override;

    enum IRCMessageRoles { TextRole = Qt::UserRole, TypeRole, ColorRole };

    Q_ENUM(IRCMessageRoles)

    const QList<IRCMessage>& messages() const { return m_messages; }

    static const int MAX_MESSAGES = 10000;

 protected:
    QList<IRCMessage> m_messages;
    int m_head = 0;
    int m_messageCount = 0;
};

class IRCUserModel : public QAbstractListModel {
    Q_OBJECT

public:
    explicit IRCUserModel(QObject* parent = nullptr);

    void setUsers(const QList<IRCUser>& users);
    void addUser(const IRCUser& user);
    void removeUser(const QString& nick);
    void clear();

    int rowCount(const QModelIndex& parent = {}) const override;
    QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const override;

    enum IRCUserRoles { NickRole = Qt::UserRole };

    Q_ENUM(IRCUserRoles)

    const QList<IRCUser>& users() const { return m_users; }

  protected:
    QList<IRCUser> m_users;
};

#endif // IRCMODELS_H
