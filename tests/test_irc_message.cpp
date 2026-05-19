#include "../backend/IRCMessage.h"
#include <QtTest/QtTest>

class TestIRCMessage : public QObject {
    Q_OBJECT
public:
    explicit TestIRCMessage(QObject* parent = nullptr) : QObject(parent) {}

private slots:
    void initTestCase();
    void cleanupTestCase();

    // Helper to build strings with IRC control characters
    static QString ircControl(int code, const QString& text = QString()) {
        QString s;
        s += QChar(code);
        s += text;
        return s;
    }

    // Test: coloredText() for MessageType::Message with nick <b>nick</b>
    // -- verify <b> is HTML-escaped in output
    void testColoredTextMessageEscapesNick() {
        IRCMessage msg;
        msg.setType(MessageType::Message);
        msg.setSender("<b>nick</b>");
        msg.setText("hello world");

        QString result = msg.coloredText();
        QVERIFY(!result.contains("<b>nick</b>"));
        QVERIFY(result.contains("&lt;b&gt;nick&lt;/b&gt;"));
    }

    // Test: formattedText() for MessageType::Quit with non-empty reason includes the reason
    void testFormattedTextQuitIncludesReason() {
        IRCMessage msg;
        msg.setType(MessageType::Quit);
        msg.setSender("alice");
        msg.setText("Gone to lunch");

        QString result = msg.formattedText();
        QCOMPARE(result, QString("alice quit (Gone to lunch)"));
    }

    // Test: formattedText() for MessageType::TopicSet uses %1 placeholder correctly
    void testFormattedTextTopicSetPlaceholder() {
        IRCMessage msg;
        msg.setType(MessageType::TopicSet);
        msg.setSender("bot");
        msg.setText("welcome to #general");

        QString result = msg.formattedText();
        QCOMPARE(result, QString("Topic: welcome to #general"));
    }

    // stripIrcFormatting: colour code \x034red\x03 should be fully stripped
    void testStripIrcFormattingColor() {
        QString input = ircControl(3) + "4red" + ircControl(3);
        QString result = IRCMessage::stripIrcFormatting(input);
        QCOMPARE(result, QString("red"));
    }

    // stripIrcFormatting: colour with digits and comma
    void testStripIrcFormattingColorComma() {
        QString input = ircControl(3) + "12,4colored" + ircControl(3);
        QString result = IRCMessage::stripIrcFormatting(input);
        QCOMPARE(result, QString("colored"));
    }

    // stripIrcFormatting: bold toggle \x02 should be stripped
    void testStripIrcFormattingBold() {
        QString input = "bold" + ircControl(2) + "word" + ircControl(2);
        QString result = IRCMessage::stripIrcFormatting(input);
        QCOMPARE(result, QString("boldword"));
    }

    // stripIrcFormatting: reset \x0F should be stripped
    void testStripIrcFormattingReset() {
        QString input = "before" + ircControl(15) + "after";
        QString result = IRCMessage::stripIrcFormatting(input);
        QCOMPARE(result, QString("beforeafter"));
    }

    // stripIrcFormatting: monospace \x11 should be stripped
    void testStripIrcFormattingMonospace() {
        QString input = "code" + ircControl(0x11) + "here" + ircControl(0x11);
        QString result = IRCMessage::stripIrcFormatting(input);
        QCOMPARE(result, QString("codehere"));
    }

    // stripIrcFormatting: italic \x1D should be stripped
    void testStripIrcFormattingItalic() {
        QString input = "italic" + ircControl(0x1D) + "text" + ircControl(0x1D);
        QString result = IRCMessage::stripIrcFormatting(input);
        QCOMPARE(result, QString("italictext"));
    }

    // stripIrcFormatting: underline \x1F should be stripped
    void testStripIrcFormattingUnderline() {
        QString input = "underline" + ircControl(0x1F) + "word" + ircControl(0x1F);
        QString result = IRCMessage::stripIrcFormatting(input);
        QCOMPARE(result, QString("underlineword"));
    }

    // stripIrcFormatting: strikethrough \x1E should be stripped
    void testStripIrcFormattingStrike() {
        QString input = "strike" + ircControl(0x1E) + "text" + ircControl(0x1E);
        QString result = IRCMessage::stripIrcFormatting(input);
        QCOMPARE(result, QString("striketext"));
    }

    // stripIrcFormatting: reverse video \x16 should be stripped
    void testStripIrcFormattingReverse() {
        QString input = "reverse" + ircControl(0x16) + "text" + ircControl(0x16);
        QString result = IRCMessage::stripIrcFormatting(input);
        QCOMPARE(result, QString("reversetext"));
    }

    // stripIrcFormatting: bare \x03 (no digits) should be stripped
    void testStripIrcFormattingBareColor() {
        QString input = "before" + ircControl(3) + "after";
        QString result = IRCMessage::stripIrcFormatting(input);
        QCOMPARE(result, QString("beforeafter"));
    }

    // stripIrcFormatting: multi-digit colour \x0315\x03 should be stripped
    void testStripIrcFormattingMultiDigitColor() {
        QString input = ircControl(3) + "15" + ircControl(3) + "grey";
        QString result = IRCMessage::stripIrcFormatting(input);
        QCOMPARE(result, QString("grey"));
    }

    // stripIrcFormatting: mixed formatting codes
    void testStripIrcFormattingMixed() {
        QString input = ircControl(3) + "4red" + ircControl(3) + " " + ircControl(2) + "bold" + ircControl(2) + " text";
        QString result = IRCMessage::stripIrcFormatting(input);
        QCOMPARE(result, QString("red bold text"));
    }

    // stripIrcFormatting: empty input returns empty
    void testStripIrcFormattingEmpty() {
        QString result = IRCMessage::stripIrcFormatting(QString());
        QCOMPARE(result, QString());
    }

    // stripIrcFormatting: input with no formatting codes passes through
    void testStripIrcFormattingPlain() {
        QString result = IRCMessage::stripIrcFormatting("plain text here");
        QCOMPARE(result, QString("plain text here"));
    }

    // stripIrcFormatting: complex mixed colours and toggles
    void testStripIrcFormattingComplex() {
        QString input = ircControl(2) + "bold " + ircControl(0x1D) + "bold+italic " + ircControl(2) + "italic" + ircControl(0x1D) + " plain";
        QString result = IRCMessage::stripIrcFormatting(input);
        QCOMPARE(result, QString("bold bold+italic italic plain"));
    }

    // renderIrcFormatting: bold toggle \x02 should render as styled
    void testRenderIrcFormattingBold() {
        QString input = "before" + ircControl(2) + "bold" + ircControl(2) + " after";
        QString result = IRCMessage::renderIrcFormatting(input);
        QVERIFY(result.contains("<span"));
        QVERIFY(result.contains("font-weight:bold"));
        QVERIFY(result.contains("bold</span>"));
    }

    // renderIrcFormatting: colour \x034red\x03 renders with red colour
    void testRenderIrcFormattingColor() {
        QString input = ircControl(3) + "4red" + ircControl(3);
        QString result = IRCMessage::renderIrcFormatting(input);
        QVERIFY(result.contains("<span"));
        QVERIFY(result.contains("color:#FF0000"));
        QVERIFY(result.contains("red</span>"));
    }

    // renderIrcFormatting: colour with fg and bg \x034,2red on navy\x03
    void testRenderIrcFormattingColorBg() {
        QString input = ircControl(3) + "4,2red on navy" + ircControl(3);
        QString result = IRCMessage::renderIrcFormatting(input);
        QVERIFY(result.contains("red on navy"));
        QVERIFY(result.contains("color:#FF0000"));
        QVERIFY(result.contains("background-color:#00007F"));
    }

    // renderIrcFormatting: italic toggle \x1D renders italic style
    void testRenderIrcFormattingItalic() {
        QString input = ircControl(0x1D) + "italic" + ircControl(0x1D);
        QString result = IRCMessage::renderIrcFormatting(input);
        QVERIFY(result.contains("<span"));
        QVERIFY(result.contains("font-style:italic"));
        QVERIFY(result.contains("italic</span>"));
    }

    // renderIrcFormatting: monospace toggle \x11 renders monospace
    void testRenderIrcFormattingMonospace() {
        QString input = ircControl(0x11) + "code" + ircControl(0x11);
        QString result = IRCMessage::renderIrcFormatting(input);
        QVERIFY(result.contains("<span"));
        QVERIFY(result.contains("font-family:monospace"));
        QVERIFY(result.contains("code</span>"));
    }

    // renderIrcFormatting: reset \x0F clears all formatting
    void testRenderIrcFormattingReset() {
        QString input = ircControl(2) + "bold" + ircControl(2) + ircControl(0x0F) + " plain";
        QString result = IRCMessage::renderIrcFormatting(input);
        QVERIFY(result.contains("font-weight:bold"));
        QVERIFY(result.contains("bold</span>"));
        QVERIFY(result.contains(" plain"));
    }

    // renderIrcFormatting: combined bold+italic toggles
    void testRenderIrcFormattingCombined() {
        QString input = ircControl(2) + "bold " + ircControl(0x1D) + "both" + ircControl(2) + " italic" + ircControl(0x1D) + " plain";
        QString result = IRCMessage::renderIrcFormatting(input);
        QVERIFY(result.contains("font-weight:bold"));
        QVERIFY(result.contains("font-style:italic"));
        QVERIFY(result.contains("both</span>"));
        QVERIFY(result.contains(" italic</span>"));
    }

    // renderIrcFormatting: bare \x03 resets colour without affecting bold
    void testRenderIrcFormattingBareColor() {
        QString input = ircControl(2) + ircControl(3) + "bold" + ircControl(2);
        QString result = IRCMessage::renderIrcFormatting(input);
        QVERIFY(result.contains("font-weight:bold"));
        QVERIFY(result.contains("bold</span>"));
    }

    // renderIrcFormatting: reverse video \x16 swaps fg/bg
    void testRenderIrcFormattingReverse() {
        QString input = ircControl(3) + "12,4red on navy" + ircControl(0x16) + "reverse" + ircControl(0x16);
        QString result = IRCMessage::renderIrcFormatting(input);
        QVERIFY(result.contains("<span"));
        QVERIFY(result.contains("red on navy"));
    }

    // renderIrcFormatting: empty input returns empty
    void testRenderIrcFormattingEmpty() {
        QString result = IRCMessage::renderIrcFormatting(QString());
        QCOMPARE(result, QString());
    }

    // renderIrcFormatting: plain text passes through unstyled
    void testRenderIrcFormattingPlain() {
        QString result = IRCMessage::renderIrcFormatting("plain text here");
        QCOMPARE(result, QString("plain text here"));
    }

    // renderIrcFormatting: HTML special characters are escaped
    void testRenderIrcFormattingHtmlEscape() {
        QString input = ircControl(2) + "<bold>" + ircControl(2);
        QString result = IRCMessage::renderIrcFormatting(input);
        QVERIFY(result.contains("&lt;bold&gt;"));
    }

// renderIrcFormatting: multi-digit colour \x0315 renders light grey
    void testRenderIrcFormattingMultiDigitColor() {
        QString input = ircControl(3) + "15light grey" + ircControl(3);
        QString result = IRCMessage::renderIrcFormatting(input);
        QVERIFY(result.contains("light grey"));
        QVERIFY(result.contains("color:#D2D2D2"));
    }

    // renderIrcFormatting: strikethrough toggle \x1E
    void testRenderIrcFormattingStrike() {
        QString input = ircControl(0x1E) + "strike" + ircControl(0x1E) + " through";
        QString result = IRCMessage::renderIrcFormatting(input);
        QVERIFY(result.contains("text-decoration:line-through"));
    }

    // renderIrcFormatting: underline toggle \x1F
    void testRenderIrcFormattingUnderline() {
        QString input = ircControl(0x1F) + "underline" + ircControl(0x1F) + " text";
        QString result = IRCMessage::renderIrcFormatting(input);
        QVERIFY(result.contains("text-decoration:underline"));
    }

    // renderIrcFormatting: \x16 reverse video swaps colours
    void testRenderIrcFormattingReverseColor() {
        QString input = ircControl(3) + "4,2red on navy" + ircControl(0x16) + "reverse" + ircControl(0x16);
        QString result = IRCMessage::renderIrcFormatting(input);
        QVERIFY(result.contains("red on navy"));
    }

    // renderIrcFormatting: complex mixed colours and toggles
    void testRenderIrcFormattingComplex() {
        QString input = ircControl(2) + "bold " + ircControl(0x1D) + "both" + ircControl(2) + " italic" + ircControl(0x1D) + " plain";
        QString result = IRCMessage::renderIrcFormatting(input);
        QVERIFY(result.contains("font-weight:bold"));
        QVERIFY(result.contains("font-style:italic"));
        QVERIFY(result.contains("both</span>"));
        QVERIFY(result.contains(" italic</span>"));
        QVERIFY(result.contains(" plain"));
    }
};

void TestIRCMessage::initTestCase() {}
void TestIRCMessage::cleanupTestCase() {}

QTEST_MAIN(TestIRCMessage)

#include "test_irc_message.moc"
