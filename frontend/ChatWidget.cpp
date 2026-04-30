#include "ChatWidget.h"
#include "backend/IRCMessageModel.h"
#include <QApplication>
#include <QClipboard>
#include <QDateTime>
#include <QFrame>
#include <QKeyEvent>
#include <QLabel>
#include <QPainter>
#include <QPalette>
#include <QScrollBar>
#include <QStyledItemDelegate>
#include <QTextBlock>
#include <QTextDocument>
#include <QTextLength>

class ChatItemDelegate : public QStyledItemDelegate {
public:
    explicit ChatItemDelegate(QObject* parent = nullptr) : QStyledItemDelegate(parent) {}

    void paint(QPainter* painter, const QStyleOptionViewItem& option, const QModelIndex& index) const override {
        QString text = index.data(Qt::DisplayRole).toString();
        QRect rect = option.rect;

        QTextDocument doc;
        doc.setHtml(text);
        doc.setTextWidth(option.rect.width());

        painter->save();
        painter->setFont(option.font);

        painter->translate(0, rect.top());

        QRect clipRect(QPoint(0, 0), option.rect.size());
        doc.drawContents(painter, clipRect);

        painter->restore();
    }

    QSize sizeHint(const QStyleOptionViewItem& option, const QModelIndex& index) const override {
        QString text = index.data(Qt::DisplayRole).toString();
        QTextDocument doc;
        doc.setHtml(text);
        doc.setTextWidth(option.rect.width() > 0 ? option.rect.width() : 600);
        return QSize(qRound(doc.idealWidth()), qRound(doc.size().height()) + 4);
    }
};

ChatWidget::ChatWidget(QWidget* parent)
    : QWidget(parent), m_chatModel(nullptr), m_channelName(""), m_topicEdit(nullptr), m_chatList(nullptr) {
    m_mainLayout = new QVBoxLayout(this);
    m_mainLayout->setContentsMargins(0, 0, 0, 0);
    m_mainLayout->setSpacing(0);

    m_topicEdit = new QTextEdit();
    m_topicEdit->setPlaceholderText("Channel topic will appear here...");
    m_topicEdit->setReadOnly(true);
    m_topicEdit->setMaximumHeight(40);
    m_topicEdit->setVisible(false);
    m_topicEdit->document()->setPlainText("Topic: No topic set");
    m_mainLayout->addWidget(m_topicEdit);

    m_chatList = new QListView();
    m_chatList->setItemDelegate(new ChatItemDelegate(this));
    m_chatList->setWordWrap(true);
    m_chatList->setFrameShape(QFrame::NoFrame);
    m_chatList->setBackgroundRole(QPalette::Window);
    m_chatList->setSelectionMode(QAbstractItemView::NoSelection);
    m_chatList->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    m_chatList->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_chatList->setFocusPolicy(Qt::NoFocus);
    m_mainLayout->addWidget(m_chatList, 1);
}

void ChatWidget::setTopic(const QString& topic) {
    m_topicEdit->setPlainText(topic);
    m_topicEdit->setMaximumHeight(40);
    m_topicEdit->setVisible(true);
}

void ChatWidget::setTopicVisible(bool visible) {
    if (visible) {
        m_topicEdit->setVisible(true);
    } else {
        m_topicEdit->setVisible(false);
        m_topicEdit->setMaximumHeight(0);
    }
}

void ChatWidget::addMessage(const IRCMessage& msg) {
    if (m_chatModel && m_chatModel->inherits("IRCMessageModel")) {
        IRCMessageModel* model = static_cast<IRCMessageModel*>(m_chatModel);
        model->addMessage(msg);
    }
    scrollToBottom();
}

void ChatWidget::clearMessages() {
    if (m_chatModel && m_chatModel->inherits("IRCMessageModel")) {
        IRCMessageModel* model = static_cast<IRCMessageModel*>(m_chatModel);
        model->clear();
    }
}

void ChatWidget::setChatModel(QAbstractItemModel* model) {
    m_chatModel = model;
    m_chatList->setModel(model);
}

void ChatWidget::setChannelName(const QString& name) {
    m_channelName = name;
}

void ChatWidget::scrollToBottom() {
    if (!m_chatList || m_chatList->model() == nullptr)
        return;

    QScrollBar* scroll = m_chatList->verticalScrollBar();
    if (scroll) {
        // Only scroll to bottom if user was already at bottom
        if (scroll->value() >= scroll->maximum() - 4) {
            scroll->setValue(scroll->maximum());
        }
    }
}

void ChatWidget::copySelectedText() {
    if (!m_chatList || !m_chatList->model())
        return;
    auto indexes = m_chatList->selectionModel()->selectedIndexes();
    if (indexes.isEmpty())
        return;
    QString text = indexes.first().data(Qt::DisplayRole).toString();
    QTextDocument doc;
    doc.setHtml(text);
    QApplication::clipboard()->setText(doc.toPlainText());
}
