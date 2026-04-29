#include "IRCMessage.h"
#include <QRegularExpression>

IRCMessage::IRCMessage(MessageType type, const QString& text, const QString& sender)
    : m_type(type), m_text(text), m_sender(sender)
{
}

QString IRCMessage::formattedText() const {
    QString result;

    switch (m_type) {
    case MessageType::Message:
        result = (!m_sender.isEmpty()) ? QString("%1: %2").arg(m_sender).arg(m_text) : m_text;
        break;
    case MessageType::NickChange:
        result = m_text;
        break;
    case MessageType::Join:
        result = QString("%1 joined %2").arg(m_sender).arg(m_text);
        break;
    case MessageType::Part:
        result = (!m_text.isEmpty()) ? QString("%1 left %2").arg(m_sender).arg(m_text) : QString("%1 left").arg(m_sender);
        break;
    case MessageType::Quit:
        result = (!m_text.isEmpty()) ? QString("%1 quit (%2)").arg(m_sender).arg(m_text) : QString("%1 quit").arg(m_sender);
        break;
    case MessageType::Kick:
        result = m_text;
        break;
    case MessageType::Mode:
        result = QString("%1 set mode: %2").arg(m_sender).arg(m_text);
        break;
    case MessageType::Topic:
        result = QString("%1 set topic: %2").arg(m_sender).arg(m_text);
        break;
    case MessageType::TopicSet:
        result = QString("Topic: %1").arg(m_text);
        break;
    case MessageType::Error:
        result = QString("ERROR: %1").arg(m_text);
        break;
case MessageType::Notice:
         result = QString("[Notice from %1] %2").arg(m_sender).arg(m_text);
         break;
    case MessageType::System:
         result = m_text;
         break;
    }

    return result;
}

QString IRCMessage::coloredText() const {
    QString formatted;
    QString timestampStr = QString("[%1] ").arg(m_timestamp.toString("HH:mm:ss"));

    switch (m_type) {
    case MessageType::Message:
        formatted += timestampStr + QString("<span style=\"color: %1;\"><b>%2</b></span>: %3")
            .arg("#888888")
            .arg(m_sender.toHtmlEscaped())
            .arg(m_text.toHtmlEscaped());
        break;
    case MessageType::NickChange:
        formatted += timestampStr + QString("<span style=\"color: #FF8888;\">%1</span>").arg(formattedText().toHtmlEscaped());
        break;
    case MessageType::Join:
        formatted += timestampStr + QString("<span style=\"color: #88FF88;\">%1</span>").arg(formattedText().toHtmlEscaped());
        break;
    case MessageType::Part:
        formatted += timestampStr + QString("<span style=\"color: #FF8888;\">%1</span>").arg(formattedText().toHtmlEscaped());
        break;
    case MessageType::Quit:
        formatted += timestampStr + QString("<span style=\"color: #FF8888;\">%1</span>").arg(formattedText().toHtmlEscaped());
        break;
    case MessageType::Kick:
        formatted += timestampStr + QString("<span style=\"color: #FF8888;\">%1</span>").arg(formattedText().toHtmlEscaped());
        break;
    case MessageType::Mode:
        formatted += timestampStr + QString("<span style=\"color: #8888FF;\">%1</span>").arg(formattedText().toHtmlEscaped());
        break;
    case MessageType::Topic:
        formatted += timestampStr + QString("<span style=\"color: #8888FF;\">%1</span>").arg(formattedText().toHtmlEscaped());
        break;
    case MessageType::TopicSet:
        formatted += timestampStr + QString("<span style=\"color: #8888FF;\">%1</span>").arg(formattedText().toHtmlEscaped());
        break;
    case MessageType::Error:
        formatted += timestampStr + QString("<span style=\"color: #FF0000;\">%1</span>").arg(formattedText().toHtmlEscaped());
        break;
case MessageType::Notice:
         formatted += timestampStr + QString("<span style=\"color: #AAAAAA;\">%1</span>").arg(formattedText().toHtmlEscaped());
         break;
     case MessageType::System:
          formatted += timestampStr + QString("<span style=\"color: #888888;\">%1</span>").arg(formattedText().toHtmlEscaped());
          break;
    }

    return formatted;
}
