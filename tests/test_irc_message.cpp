#include "../backend/IRCMessage.h"
#include <QtTest/QtTest>

class TestIRCMessage : public QObject {
    Q_OBJECT
public:
    explicit TestIRCMessage(QObject* parent = nullptr) : QObject(parent) {}

private slots:
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
};

QTEST_MAIN(TestIRCMessage)

#include "test_irc_message.moc"
