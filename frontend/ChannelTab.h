#ifndef CHANNELTAB_H
#define CHANNELTAB_H

#include "ChatWidget.h"
#include "backend/IRCMessage.h"
#include "backend/IRCUser.h"
#include <QAbstractItemModel>
#include <QLineEdit>
#include <QVBoxLayout>
#include <QWidget>

class ChannelTab : public QWidget {
    Q_OBJECT

public:
    explicit ChannelTab(const QString& name, QWidget* parent = nullptr);

    void setTopic(const QString& topic);
    void setTopicVisible(bool visible);
    void setMode(const QString& mode);
    void addMessage(const IRCMessage& msg);
    void clearMessages();

    void setChatModel(QAbstractItemModel* model);
    const QString& channelName() const { return m_channelName; }

public slots:
    void closeTab();

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
