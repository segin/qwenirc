#include "IRCMessage.h"
#include <QRegularExpression>

IRCMessage::IRCMessage(MessageType type, const QString& text, const QString& sender)
    : m_type(type), m_text(text), m_sender(sender) {
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
        result = (!m_text.isEmpty()) ? QString("%1 left %2 (%3)").arg(m_sender).arg(m_channel).arg(m_text)
                                     : QString("%1 left %2").arg(m_sender).arg(m_channel);
        break;
    case MessageType::Quit:
        result =
            (!m_text.isEmpty()) ? QString("%1 quit (%2)").arg(m_sender).arg(m_text) : QString("%1 quit").arg(m_sender);
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
                                         .arg(IRCMessage::stripIrcFormatting(m_sender).toHtmlEscaped())
                                         .arg(IRCMessage::stripIrcFormatting(m_text).toHtmlEscaped());
        break;
    case MessageType::NickChange:
        formatted +=
            timestampStr + QString("<span style=\"color: #FF8888;\">%1</span>").arg(IRCMessage::stripIrcFormatting(formattedText()).toHtmlEscaped());
        break;
    case MessageType::Join:
        formatted +=
            timestampStr + QString("<span style=\"color: #88FF88;\">%1</span>").arg(IRCMessage::stripIrcFormatting(formattedText()).toHtmlEscaped());
        break;
    case MessageType::Part: {
        QString pt = formattedText();
        QString cleanPt = IRCMessage::stripIrcFormatting(pt);
        formatted += timestampStr + QString("<span style=\"color: #FF8888;\">%1</span>").arg(cleanPt.toHtmlEscaped());
        break;
    }
    case MessageType::Quit: {
        QString qt = formattedText();
        QString cleanQt = IRCMessage::stripIrcFormatting(qt);
        formatted += timestampStr + QString("<span style=\"color: #FF8888;\">%1</span>").arg(cleanQt.toHtmlEscaped());
        break;
    }
    case MessageType::Kick:
        formatted +=
            timestampStr + QString("<span style=\"color: #FF8888;\">%1</span>").arg(IRCMessage::stripIrcFormatting(formattedText()).toHtmlEscaped());
        break;
    case MessageType::Mode: {
        QString mt = formattedText();
        QString cleanMt = IRCMessage::stripIrcFormatting(mt);
        formatted += timestampStr + QString("<span style=\"color: #8888FF;\">%1</span>").arg(cleanMt.toHtmlEscaped());
        break;
    }
    case MessageType::Topic: {
        QString tt = formattedText();
        QString cleanTt = IRCMessage::stripIrcFormatting(tt);
        formatted += timestampStr + QString("<span style=\"color: #8888FF;\">%1</span>").arg(cleanTt.toHtmlEscaped());
        break;
    }
    case MessageType::TopicSet:
        formatted +=
            timestampStr + QString("<span style=\"color: #8888FF;\">%1</span>").arg(IRCMessage::stripIrcFormatting(formattedText()).toHtmlEscaped());
        break;
    case MessageType::Error:
        formatted +=
            timestampStr + QString("<span style=\"color: #FF0000;\">%1</span>").arg(IRCMessage::stripIrcFormatting(formattedText()).toHtmlEscaped());
        break;
    case MessageType::Notice: {
        QString nt = formattedText();
        QString cleanNt = IRCMessage::stripIrcFormatting(nt);
        formatted += timestampStr + QString("<span style=\"color: #AAAAAA;\">%1</span>").arg(cleanNt.toHtmlEscaped());
        break;
    }
    case MessageType::System:
        formatted +=
            timestampStr + QString("<span style=\"color: #888888;\">%1</span>").arg(IRCMessage::stripIrcFormatting(formattedText()).toHtmlEscaped());
        break;
    }

   return formatted;
}

QString IRCMessage::stripIrcFormatting(const QString& text) {
    QString result;
    result.reserve(text.size());
    
      for (int i = 0; i < text.size(); ++i) {
        QChar c = text[i];
        ushort u = c.unicode();
        if (u == 3) {
            ++i;
            int digits = 0;
            while (i < text.size() && text[i].isDigit() && digits < 2) {
                ++i;
                ++digits;
            }
            if (i < text.size() && text[i] == ',') {
                ++i;
                digits = 0;
                while (i < text.size() && text[i].isDigit() && digits < 2) {
                    ++i;
                    ++digits;
                }
            }
            --i;
        } else if (u == 2 || u == 15 || u == 16 || u == 17 || u == 22 || u == 29 || u == 30 || u == 31) {
            continue;
        }
        result.append(c);
    }
    return result;
}
