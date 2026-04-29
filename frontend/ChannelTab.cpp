#include "ChannelTab.h"
#include <QHBoxLayout>
#include <QLabel>
#include <QTabWidget>

 ChannelTab::ChannelTab(const QString& name, NetworkManager* nm, QWidget* parent)
    : QWidget(parent)
    , m_channelName(name)
    , m_chatWidget(nullptr)
    , m_inputEdit(nullptr)
{
    Q_UNUSED(nm);
    initializeUI();
}

void ChannelTab::initializeUI() {
    m_mainLayout = new QVBoxLayout(this);
     m_mainLayout->setContentsMargins(0, 0, 0, 0);
    m_mainLayout->setSpacing(0);

    m_chatWidget = new ChatWidget();
    m_chatWidget->setChannel(m_channelName);
    m_chatWidget->setChannelName(m_channelName);

    m_mainLayout->addWidget(m_chatWidget, 1);

    QHBoxLayout* inputLayout = new QHBoxLayout();
      inputLayout->setContentsMargins(0, 0, 0, 0);

  m_inputEdit = new QLineEdit();
    m_inputEdit->setPlaceholderText("Type a message...");
    inputLayout->addWidget(m_inputEdit, 1);

    m_mainLayout->addLayout(inputLayout);

    connect(m_inputEdit, &QLineEdit::returnPressed, this, [this]() {
        QString text = m_inputEdit->text();
        if (text.isEmpty()) return;
        emit messageSent(text);
        m_inputEdit->clear();
    });
}

void ChannelTab::setTopic(const QString& topic) {
    m_chatWidget->setTopic(topic);
}

void ChannelTab::setTopicVisible(bool visible) {
    m_chatWidget->setTopicVisible(visible);
}

void ChannelTab::addMessage(const IRCMessage& msg) {
    m_chatWidget->addMessage(msg);
}

void ChannelTab::clearMessages() {
    m_chatWidget->clearMessages();
}

void ChannelTab::setChatModel(QAbstractItemModel* model) {
    m_chatWidget->setChatModel(model);
}

void ChannelTab::setMode(const QString& mode) {
    QTabWidget* tw = qobject_cast<QTabWidget*>(parentWidget());
    if (tw && !mode.isEmpty()) {
        tw->setTabText(tw->indexOf(this), channelName() + " [" + mode + "]");
    }
}
