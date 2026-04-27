#ifndef CHANNELTAB_H
#define CHANNELTAB_H

#include "backend/IRCMessage.h"
#include "backend/IRCUser.h"
#include "backend/NetworkManager.h"
#include "ChatWidget.h"
#include <QWidget>
#include <QVBoxLayout>
#include <QLineEdit>
#include <QAbstractItemModel>

class ChannelTab : public QWidget {
    Q_OBJECT

public:
    explicit ChannelTab(const QString& name, NetworkManager* nm, QWidget* parent = nullptr);

     void setTopic(const QString& topic);
    void setTopicVisible(bool visible);
    void addMessage(const IRCMessage& msg);
    void clearMessages();

    void setChatModel(QAbstractItemModel* model);
    const QString& channelName() const { return m_channelName; }

signals:
    void messageSent(const QString& message);

private:
    void initializeUI();

    QString m_channelName;
    ChatWidget* m_chatWidget;
    QLineEdit* m_inputEdit;
    QVBoxLayout* m_mainLayout;
};

#endif // CHANNELTAB_H
