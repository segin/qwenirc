#include "IRCMessage.h"
#include <QRegularExpression>

IRCMessage::IRCMessage(MessageType type, const QString& text, const QString& sender)
    : m_type(type), m_text(text), m_sender(sender)
{
}

QString IRCMessage::formattedText() const {
    QString result;

    if (m_type == MessageType::NickChange) {
        const auto parts = m_text.split(' ', Qt::SkipEmptyParts);
        if (parts.size() >= 2) {
            result = QString("%1 changed nickname to %2").arg(parts[0]).arg(parts[1]);
        } else {
            result = m_text;
        }
    } else if (m_type == MessageType::Join) {
        result = QString("%1 joined %2").arg(m_sender).arg(m_text);
    } else if (m_type == MessageType::Part) {
        const auto parts = m_text.split(' ', Qt::SkipEmptyParts);
        if (parts.size() >= 1) {
            result = QString("%1 left %2").arg(m_sender).arg(m_text);
        } else {
            result = QString("%1 left").arg(m_sender);
        }
    } else if (m_type == MessageType::Quit) {
        result = QString("%1 quit").arg(m_sender);
    } else if (m_type == MessageType::Kick) {
        result = QString("%1").arg(m_text);
    } else if (m_type == MessageType::Mode) {
        result = QString("%1 set mode: %2").arg(m_sender).arg(m_text);
    } else if (m_type == MessageType::Topic) {
        result = QString("%1 set topic: %2").arg(m_sender).arg(m_text);
    } else if (m_type == MessageType::TopicSet) {
        result = QString("Topic: %2").arg(m_text);
    } else if (m_type == MessageType::Error) {
        result = QString("ERROR: %1").arg(m_text);
    } else if (m_type == MessageType::Notice) {
        result = QString("[Notice from %1] %2").arg(m_sender).arg(m_text);
    } else if (!m_sender.isEmpty()) {
        result = QString("%1: %2").arg(m_sender).arg(m_text);
    } else {
        result = m_text;
    }

    return result;
}

QString IRCMessage::coloredText() const {
    QString formatted;
    formatted += QString("[%1] ").arg(m_timestamp.toString("HH:mm:ss"));

    switch (m_type) {
    case MessageType::Message:
        formatted += QString("<span style=\"color: %1;\"><b>%2</b></span>: %3")
            .arg("#888888")
            .arg(m_sender)
            .arg(escapeHTML(m_text));
        break;
    case MessageType::NickChange:
        formatted += QString("<span style=\"color: #FF8888;\">%1</span>").arg(formattedText());
        break;
    case MessageType::Join:
        formatted += QString("<span style=\"color: #88FF88;\">%1</span>").arg(formattedText());
        break;
    case MessageType::Part:
        formatted += QString("<span style=\"color: #FF8888;\">%1</span>").arg(formattedText());
        break;
     case MessageType::Quit:
        formatted += QString("<span style=\"color: #FF8888;\">%1</span>").arg(formattedText());
        break;
    case MessageType::Kick:
        formatted += QString("<span style=\"color: #FF8888;\">%1</span>").arg(formattedText());
        break;
    case MessageType::Mode:
        formatted += QString("<span style=\"color: #8888FF;\">%1</span>").arg(formattedText());
        break;
    case MessageType::Topic:
        formatted += QString("<span style=\"color: #8888FF;\">%1</span>").arg(formattedText());
        break;
    case MessageType::TopicSet:
        formatted += QString("<span style=\"color: #8888FF;\">%1</span>").arg(formattedText());
        break;
    case MessageType::Error:
        formatted += QString("<span style=\"color: #FF0000;\">%1</span>").arg(formattedText());
        break;
    case MessageType::Notice:
        formatted += QString("<span style=\"color: #AAAAAA;\">%1</span>").arg(formattedText());
        break;
    }

    return formatted;
}

QString IRCMessage::escapeHTML(const QString& input) {
    QString result = input;
    result.replace('&', "&amp;");
    result.replace('<', "&lt;");
    result.replace('>', "&gt;");
    result.replace('\"', "&quot;");
    result.replace('\'', "&#39;");
    return result;
}
