#ifndef CHATWIDGET_H
#define CHATWIDGET_H

#include "backend/IRCMessage.h"
#include "backend/IRCMessageModel.h"
#include "backend/IRCUser.h"
#include <QAbstractItemModel>
#include <QListView>
#include <QTextEdit>
#include <QVBoxLayout>
#include <QWidget>

class ChatWidget : public QWidget {
    Q_OBJECT

public:
    explicit ChatWidget(QWidget* parent = nullptr);

    void setTopic(const QString& topic);
    void setTopicVisible(bool visible);
    void addMessage(const IRCMessage& msg);
    void clearMessages();

    void setChatModel(QAbstractItemModel* model);
    void setChannelName(const QString& name);
    QString channelName() const { return m_channelName; }
    void copySelectedText();

signals:
    void messageSent(const QString& message);

private:
    void scrollToBottom();

    QAbstractItemModel* m_chatModel;
    QString m_channelName;
    QTextEdit* m_topicEdit;
    QListView* m_chatList;
    QVBoxLayout* m_mainLayout;
};

#endif // CHATWIDGET_H
