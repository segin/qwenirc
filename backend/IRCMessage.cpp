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

static const char* s_mircColors[16] = {
    "#FFFFFF", "#000000", "#00007F", "#009300",
    "#FF0000", "#7F0000", "#9C009C", "#FC7F00",
    "#FFFF00", "#00FC00", "#009393", "#00FFFF",
    "#0000FC", "#FF00FF", "#7F7F7F", "#D2D2D2",
};

QString IRCMessage::renderIrcFormatting(const QString& rawText) {
    struct FmtState {
        bool bold = false, italic = false, underline = false;
        bool strike = false, mono = false, reverse = false;
        int fg = -1, bg = -1;

        bool operator==(const FmtState& o) const {
            return bold == o.bold && italic == o.italic && underline == o.underline
                && strike == o.strike && mono == o.mono && reverse == o.reverse
                && fg == o.fg && bg == o.bg;
        }
        bool isPlain() const {
            return !bold && !italic && !underline && !strike && !mono && !reverse
                && fg == -1 && bg == -1;
        }
    };

    auto buildStyle = [](const FmtState& s) -> QString {
        QString style;
        if (s.bold)      style += "font-weight:bold;";
        if (s.italic)    style += "font-style:italic;";
        if (s.mono)      style += "font-family:monospace;";
        QStringList deco;
        if (s.underline) deco << "underline";
        if (s.strike)    deco << "line-through";
        if (!deco.isEmpty()) style += "text-decoration:" + deco.join(' ') + ";";
        int fg = s.reverse ? s.bg  : s.fg;
        int bg = s.reverse ? s.fg  : s.bg;
        if (fg >= 0 && fg < 16) style += QString("color:%1;").arg(s_mircColors[fg]);
        if (bg >= 0 && bg < 16) style += QString("background-color:%1;").arg(s_mircColors[bg]);
        return style;
    };

    QString result;
    FmtState cur, last;
    QString run;

    auto flush = [&]() {
        if (run.isEmpty()) return;
        if (cur.isPlain()) {
            result += run;
        } else {
            result += "<span style=\"" + buildStyle(cur) + "\">" + run + "</span>";
        }
        run.clear();
        last = cur;
    };

    for (int i = 0; i < rawText.size(); ++i) {
        ushort u = rawText[i].unicode();
        if (u == 2)  { flush(); cur.bold      ^= true; }
        else if (u == 3) {
            flush();
            int fg = -1, bg = -1;
            ++i;
            if (i < rawText.size() && rawText[i].isDigit()) {
                fg = rawText[i++].digitValue();
                if (i < rawText.size() && rawText[i].isDigit())
                    fg = fg * 10 + rawText[i++].digitValue();
                if (i < rawText.size() && rawText[i] == ',') {
                    ++i;
                    if (i < rawText.size() && rawText[i].isDigit()) {
                        bg = rawText[i++].digitValue();
                        if (i < rawText.size() && rawText[i].isDigit())
                            bg = bg * 10 + rawText[i++].digitValue();
                    }
                }
            }
            --i;
            if (fg == -1 && bg == -1) { cur.fg = cur.bg = -1; }
            else { if (fg != -1) cur.fg = fg; if (bg != -1) cur.bg = bg; }
        }
        else if (u == 0x0F) { flush(); cur = FmtState{}; }
        else if (u == 0x11) { flush(); cur.mono      ^= true; }
        else if (u == 0x16) { flush(); cur.reverse   ^= true; }
        else if (u == 0x1D) { flush(); cur.italic    ^= true; }
        else if (u == 0x1E) { flush(); cur.strike    ^= true; }
        else if (u == 0x1F) { flush(); cur.underline ^= true; }
        else {
            run += QString(rawText[i]).toHtmlEscaped();
        }
    }
    flush();
return result;
}

QString IRCMessage::coloredText() const {
    QString formatted;
    QString timestampStr = QString("[%1] ").arg(m_timestamp.toString("HH:mm:ss"));

    switch (m_type) {
    case MessageType::Message:
        formatted += timestampStr + QString("<span style=\"color: %1;\"><b>%2</b></span>: %3")
                                          .arg("#888888")
                                          .arg(renderIrcFormatting(m_sender))
                                          .arg(renderIrcFormatting(m_text));
        break;
    case MessageType::NickChange:
        formatted +=
            timestampStr + QString("<span style=\"color: #FF8888;\">%1</span>").arg(renderIrcFormatting(formattedText()));
        break;
    case MessageType::Join:
        formatted +=
            timestampStr + QString("<span style=\"color: #88FF88;\">%1</span>").arg(renderIrcFormatting(formattedText()));
        break;
    case MessageType::Part:
        formatted +=
            timestampStr + QString("<span style=\"color: #FF8888;\">%1</span>").arg(renderIrcFormatting(formattedText()));
        break;
    case MessageType::Quit:
        formatted +=
            timestampStr + QString("<span style=\"color: #FF8888;\">%1</span>").arg(renderIrcFormatting(formattedText()));
        break;
    case MessageType::Kick:
        formatted +=
            timestampStr + QString("<span style=\"color: #FF8888;\">%1</span>").arg(renderIrcFormatting(formattedText()));
        break;
    case MessageType::Mode:
        formatted +=
            timestampStr + QString("<span style=\"color: #8888FF;\">%1</span>").arg(renderIrcFormatting(formattedText()));
        break;
    case MessageType::Topic:
        formatted +=
            timestampStr + QString("<span style=\"color: #8888FF;\">%1</span>").arg(renderIrcFormatting(formattedText()));
        break;
    case MessageType::TopicSet:
        formatted +=
            timestampStr + QString("<span style=\"color: #8888FF;\">%1</span>").arg(renderIrcFormatting(formattedText()));
        break;
    case MessageType::Error:
        formatted +=
            timestampStr + QString("<span style=\"color: #FF0000;\">%1</span>").arg(renderIrcFormatting(formattedText()));
        break;
    case MessageType::Notice:
        formatted +=
            timestampStr + QString("<span style=\"color: #AAAAAA;\">%1</span>").arg(renderIrcFormatting(formattedText()));
        break;
    case MessageType::System:
        formatted +=
            timestampStr + QString("<span style=\"color: #888888;\">%1</span>").arg(renderIrcFormatting(formattedText()));
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
            continue;
        } else if (u == 2 || u == 15 || u == 16 || u == 17 || u == 22 || u == 29 || u == 30 || u == 31) {
            continue;
        }
        result.append(c);
    }
    return result;
}
