#ifndef IRCMESSAGE_H
#define IRCMESSAGE_H

#include <QString>
#include <QDateTime>

enum class MessageType {
    Message,
    NickChange,
    Join,
    Part,
    Quit,
    Kick,
    Mode,
    Topic,
    TopicSet,
    Error,
    Notice
};

class IRCMessage {
public:
    IRCMessage() = default;
    IRCMessage(MessageType type, const QString& text, const QString& sender = {});

    MessageType type() const { return m_type; }
    QString text() const { return m_text; }
    QString sender() const { return m_sender; }
    QDateTime timestamp() const { return m_timestamp; }
    QString channel() const { return m_channel; }

    void setType(MessageType type) { m_type = type; }
    void setText(const QString& text) { m_text = text; }
    void setSender(const QString& sender) { m_sender = sender; }
    void setChannel(const QString& channel) { m_channel = channel; }

    QString formattedText() const;
    QString coloredText() const;

    static QString escapeHTML(const QString& input);

private:
    MessageType m_type = MessageType::Message;
    QString m_text;
    QString m_sender;
    QString m_channel;
    QDateTime m_timestamp = QDateTime::currentDateTime();
};

#endif // IRCMESSAGE_H
