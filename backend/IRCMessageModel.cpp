#include "IRCMessageModel.h"
#include <QAbstractItemDelegate>

// ---- IRCMessageModel ----

IRCMessageModel::IRCMessageModel(QObject* parent)
    : QAbstractListModel(parent)
{
}

void IRCMessageModel::addMessage(const IRCMessage& msg) {
    if (m_messages.size() >= IRCMessageModel::MAX_MESSAGES) {
        beginRemoveRows(QModelIndex(), 0, 0);
        m_messages.removeFirst();
        endRemoveRows();
    }

    beginInsertRows(QModelIndex(), m_messages.size(), m_messages.size());
    m_messages.append(msg);
    endInsertRows();
}

void IRCMessageModel::clear() {
    beginResetModel();
    m_messages.clear();
    endResetModel();
}

void IRCMessageModel::insertSystemMessage(const QString& text) {
    IRCMessage msg(MessageType::System, text);
    addMessage(msg);
}

int IRCMessageModel::rowCount(const QModelIndex& parent) const {
    Q_UNUSED(parent);
    return m_messages.size();
}

QVariant IRCMessageModel::data(const QModelIndex& index, int role) const {
    if (!index.isValid() || index.row() < 0 || index.row() >= m_messages.size()) {
        return {};
    }

    const auto& msg = m_messages[index.row()];

    switch (role) {
case Qt::DisplayRole:
         return msg.coloredText();
     case TypeRole:
         return static_cast<int>(msg.type());
     case ColorRole:
         return msg.coloredText();
     default:
         return {};
    }
}

// ---- IRCUserModel ----

IRCUserModel::IRCUserModel(QObject* parent)
    : QAbstractListModel(parent)
{
}

void IRCUserModel::setUsers(const QList<IRCUser>& users) {
    beginResetModel();
    m_users = users;
    endResetModel();
}

void IRCUserModel::addUser(const IRCUser& user) {
    for (const auto& u : m_users) {
        if (u.nick().toUpper() == user.nick().toUpper()) {
            return;
        }
    }

    int row = 0;
    for (; row < m_users.size(); ++row) {
        if (user.nick().toUpper() < m_users[row].nick().toUpper()) {
            break;
        }
    }

    beginInsertRows(QModelIndex(), row, row);
    m_users.insert(row, user);
    endInsertRows();
}

void IRCUserModel::removeUser(const QString& nick) {
    for (int i = 0; i < m_users.size(); ++i) {
        if (m_users[i].nick().toUpper() == nick.toUpper()) {
            beginRemoveRows(QModelIndex(), i, i);
            m_users.removeAt(i);
            endRemoveRows();
            return;
        }
    }
}

void IRCUserModel::clear() {
    beginResetModel();
    m_users.clear();
    endResetModel();
}

int IRCUserModel::rowCount(const QModelIndex& parent) const {
    Q_UNUSED(parent);
    return m_users.size();
}

QVariant IRCUserModel::data(const QModelIndex& index, int role) const {
    if (!index.isValid() || index.row() < 0 || index.row() >= m_users.size()) {
        return {};
    }

    const auto& user = m_users[index.row()];

    switch (role) {
case Qt::DisplayRole: {
         return QString("%1%2").arg(user.userPrefix()).arg(user.nick());
     }
    case NickRole:
        return user.nick();
    default:
        return {};
    }
}

// ---- IRCChannelModel ----

IRCChannelModel::IRCChannelModel(QObject* parent)
    : QAbstractListModel(parent)
{
}

void IRCChannelModel::addChannel(const QString& name) {
    for (const auto& ch : m_channels) {
        if (ch == name) {
            return;
        }
    }

    beginInsertRows(QModelIndex(), m_channels.size(), m_channels.size());
    m_channels.append(name);
    endInsertRows();
}

void IRCChannelModel::removeChannel(const QString& name) {
    int idx = m_channels.indexOf(name);
    if (idx >= 0) {
        beginRemoveRows(QModelIndex(), idx, idx);
        m_channels.removeOne(name);
        endRemoveRows();
    }
}

void IRCChannelModel::clear() {
    beginResetModel();
    m_channels.clear();
    m_currentChannel.clear();
    endResetModel();
}

void IRCChannelModel::setCurrentChannel(const QString& name) {
    m_currentChannel = name;
    if (m_channels.isEmpty()) return;
    emit dataChanged(index(0, 0), index(m_channels.size() - 1, 0));
}

int IRCChannelModel::rowCount(const QModelIndex& parent) const {
    Q_UNUSED(parent);
    return m_channels.size();
}

QVariant IRCChannelModel::data(const QModelIndex& index, int role) const {
    if (!index.isValid() || index.row() < 0 || index.row() >= m_channels.size()) {
        return {};
    }

    const auto& channel = m_channels[index.row()];

    switch (role) {
    case Qt::DisplayRole:
        return channel;
    case NameRole:
        return channel;
    case ActiveRole:
        return channel == m_currentChannel;
    default:
        return {};
    }
}
