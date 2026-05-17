#include "IRCMessageModel.h"
#include <QAbstractItemDelegate>
#include <QColor>

// ---- IRCMessageModel ----

IRCMessageModel::IRCMessageModel(QObject* parent) : QAbstractListModel(parent) {
}

void IRCMessageModel::addMessage(const IRCMessage& msg) {
    if (m_messages.isEmpty()) {
        m_messages.resize(MAX_MESSAGES);
        m_head = 0;
        m_messageCount = 0;
    }

    if (m_messageCount >= MAX_MESSAGES) {
        int physIdx = m_head;
        m_messages[physIdx] = msg;
        m_head = (m_head + 1) % MAX_MESSAGES;
        emit dataChanged(index(0, 0), index(MAX_MESSAGES - 1, 0), {Qt::DisplayRole});
    } else {
        beginInsertRows(QModelIndex(), m_messageCount, m_messageCount);
        m_messages[m_messageCount] = msg;
        m_messageCount++;
        endInsertRows();
    }
}

void IRCMessageModel::clear() {
    beginResetModel();
    m_messages.clear();
    m_head = 0;
    m_messageCount = 0;
    endResetModel();
}

void IRCMessageModel::insertSystemMessage(const QString& text) {
    IRCMessage msg(MessageType::System, text);
    addMessage(msg);
}

int IRCMessageModel::rowCount(const QModelIndex& parent) const {
    Q_UNUSED(parent);
    return m_messageCount;
}

QVariant IRCMessageModel::data(const QModelIndex& index, int role) const {
    if (!index.isValid() || index.row() < 0 || index.row() >= m_messageCount) {
        return {};
    }

    int row = index.row();
    int physIdx = (m_head + row) % MAX_MESSAGES;
    const auto& msg = m_messages[physIdx];

    switch (role) {
    case Qt::DisplayRole:
        return msg.coloredText();
    case TypeRole:
        return static_cast<int>(msg.type());
    case ColorRole:
        switch (msg.type()) {
        case MessageType::NickChange:       return QVariant::fromValue(QColor("#FF8888"));
        case MessageType::Join:             return QVariant::fromValue(QColor("#88FF88"));
        case MessageType::Part:
        case MessageType::Quit:
        case MessageType::Kick:             return QVariant::fromValue(QColor("#FF8888"));
        case MessageType::Mode:
        case MessageType::Topic:
        case MessageType::TopicSet:         return QVariant::fromValue(QColor("#8888FF"));
        case MessageType::Error:            return QVariant::fromValue(QColor("#FF0000"));
        case MessageType::Notice:           return QVariant::fromValue(QColor("#AAAAAA"));
        case MessageType::System:           return QVariant::fromValue(QColor("#888888"));
        default:                            return {};
        }
    default:
        return {};
    }
}

// ---- IRCUserModel ----

IRCUserModel::IRCUserModel(QObject* parent) : QAbstractListModel(parent) {
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
