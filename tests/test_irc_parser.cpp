  #include <QtTest/QtTest>
#include <QSet>
#include <QDateTime>
#include <QAbstractSocket>

#include "../backend/NetworkManager.h"
#include "../backend/IRCChannel.h"
#include "../backend/IRCUser.h"
#include "../backend/IRCMessage.h"

// Test subclass that captures sent commands and signals
class TestNetworkManager : public NetworkManager {
    Q_OBJECT
public:
    explicit TestNetworkManager(QObject* parent = nullptr)
        : NetworkManager(parent)
    {}

    void sendRaw(const QString& data) override {
        m_sentCommands.append(data);
    }

    QStringList sentCommands() const { return m_sentCommands; }
    void clearCommands() { m_sentCommands.clear(); }

    // Capture signals
    QString lastServerChannelMessage;
    QString lastChannelMessageChannel;
    IRCMessage lastChannelMessage;
    QString lastChannelTopicChannel;
    QString lastChannelTopicText;
    QString lastNamesChannel;
    QList<IRCUser> lastNamesUsers;
    QString lastUserReceivedChannel;
    QString lastUserReceivedNick;

public Q_SLOTS:
    void captureServerChannelMessage(const QString& msg) {
        lastServerChannelMessage = msg;
    }
    void captureChannelMessage(const QString& channel, const IRCMessage& msg) {
        lastChannelMessageChannel = channel;
        lastChannelMessage = msg;
    }
    void captureChannelTopic(const QString& channel, const QString& topic) {
        lastChannelTopicChannel = channel;
        lastChannelTopicText = topic;
    }
    void captureNamesReceived(const QString& channel, const QList<IRCUser>& users) {
        lastNamesChannel = channel;
        lastNamesUsers = users;
    }
    void captureUserReceived(const QString& channel, const QString& nick) {
        lastUserReceivedChannel = channel;
        lastUserReceivedNick = nick;
    }

protected:
    QStringList m_sentCommands;
};

class TestIrcParser : public QObject {
    Q_OBJECT
public:
    explicit TestIrcParser(QObject* parent = nullptr) : QObject(parent) {}

private slots:
    // 353 RPL_NAMREPLY test
    void test353NamReply() {
        TestNetworkManager mgr;
        connect(&mgr, SIGNAL(serverChannelMessage(QString)),
                &mgr, SLOT(captureServerChannelMessage(QString)));
        connect(&mgr, SIGNAL(namesReceived(QString,QList<IRCUser>)),
                &mgr, SLOT(captureNamesReceived(QString,QList<IRCUser>)));
        connect(&mgr, SIGNAL(userReceived(QString,QString)),
                &mgr, SLOT(captureUserReceived(QString,QString)));

        // Simulated server line: :server 353 me = #chan :@op +voice user
        mgr.parseLine(":server 353 me = #chan :@op +voice user");

        // Verify channel from namesReceived
        QCOMPARE(mgr.lastNamesChannel, QString("#chan"));

        // Verify users extracted: @op -> op with prefix @, +voice -> voice with prefix +
        QCOMPARE(mgr.lastNamesUsers.size(), 3);

        // Check each user
        IRCUser* op = nullptr;
        IRCUser* voice = nullptr;
        IRCUser* user = nullptr;
        for (const IRCUser& u : mgr.lastNamesUsers) {
            if (u.nick() == "op" && u.userPrefix() == "@") op = const_cast<IRCUser*>(&u);
            else if (u.nick() == "voice" && u.userPrefix() == "+") voice = const_cast<IRCUser*>(&u);
            else if (u.nick() == "user" && u.userPrefix().isEmpty()) user = const_cast<IRCUser*>(&u);
        }
        QVERIFY(op != nullptr);
        QVERIFY(voice != nullptr);
        QVERIFY(user != nullptr);
    }

     // Timestamp + 332 topic test
    void testTimestamp332Topic() {
        TestNetworkManager mgr;
        connect(&mgr, SIGNAL(channelTopic(QString,QString)),
                &mgr, SLOT(captureChannelTopic(QString,QString)));
        connect(&mgr, SIGNAL(channelMessage(QString,IRCMessage)),
                &mgr, SLOT(captureChannelMessage(QString,IRCMessage)));

        // Simulated: @time=2024-01-01T12:00:00Z :srv 332 me #chan :topic
        mgr.parseLine("@time=2024-01-01T12:00:00Z :srv 332 me #chan :topic");

        QCOMPARE(mgr.lastChannelTopicChannel, QString("#chan"));
        QCOMPARE(mgr.lastChannelTopicText, QString("topic"));

        // Verify timestamp is set on the message
        QVERIFY(!mgr.lastChannelMessage.timestamp().isNull());
        QString tsStr = mgr.lastChannelMessage.timestamp().toString(Qt::ISODate);
        QVERIFY2(tsStr == "2024-01-01T12:00:00", ("timestamp mismatch: " + tsStr).toUtf8().data());
        
        // Debug: verify the serverTime tag was extracted
        QVERIFY2(mgr.lastChannelMessage.timestamp().toString("yyyy") == "2024", QString("year mismatch").toUtf8().data());
        QVERIFY2(mgr.lastChannelMessage.timestamp().toString("MM") == "01", QString("month mismatch").toUtf8().data());
        QVERIFY2(mgr.lastChannelMessage.timestamp().toString("dd") == "01", QString("day mismatch").toUtf8().data());
        QVERIFY2(mgr.lastChannelMessage.timestamp().toString("HH") == "12", QString("hour mismatch").toUtf8().data());
    }

    // PING -> PONG test
    void testPingPong() {
        TestNetworkManager mgr;
        connect(&mgr, SIGNAL(serverChannelMessage(QString)),
                &mgr, SLOT(captureServerChannelMessage(QString)));

        mgr.clearCommands();

        // :srv PING :pingtoken
        mgr.parseLine(":srv PING :pingtoken");

        // Verify PONG is sent
        bool foundPong = false;
        for (const QString& cmd : mgr.sentCommands()) {
            if (cmd.startsWith("PONG")) {
                foundPong = true;
                break;
            }
        }
        QVERIFY(foundPong);

        // Verify PONG contains the token
        QString pongCmd;
        for (const QString& cmd : mgr.sentCommands()) {
            if (cmd.startsWith("PONG")) {
                pongCmd = cmd;
                break;
            }
        }
        QVERIFY(pongCmd.contains("pingtoken"));

        // Verify no serverChannelMessage was emitted for PING (REQ-PING-01)
        QCOMPARE(mgr.lastServerChannelMessage, QString());
    }

      // CAP LS intersection + REQ test
    void testCapLSIntersection() {
        TestNetworkManager mgr;
        connect(&mgr, SIGNAL(serverChannelMessage(QString)),
                &mgr, SLOT(captureServerChannelMessage(QString)));

        mgr.clearCommands();

        // CAP * LS :sasl server-time echo-message
        mgr.parseLine("CAP * LS :sasl server-time echo-message");
        QTest::qWait(100);

        // Verify CAP REQ was sent with the correct caps
        QStringList cmds = mgr.sentCommands();

        QString capReqCmd;
        for (const QString& cmd : cmds) {
            if (cmd.startsWith("CAP REQ")) {
                capReqCmd = cmd;
                break;
            }
        }

        // The CAP REQ should contain the accepted caps (sasl, server-time, echo-message)
        // Each cap should appear as a separate token
        QVERIFY(!capReqCmd.isEmpty());

        // Count the number of individual caps in the request (should be 3+)
        QStringList parts = capReqCmd.split(' ', Qt::SkipEmptyParts);
        // Format: "CAP REQ cap1 cap2 cap3\r\n"
        // The cap list should contain sasl, server-time, echo-message
        QVERIFY(parts.size() >= 4); // CAP REQ at minimum + 1 cap

        // Verify individual cap names are present
        QString combined = capReqCmd.toLower();
        QVERIFY(combined.contains("sasl"));
        QVERIFY(combined.contains("server-time"));
        QVERIFY(combined.contains("echo-message"));
    }

    // CAP LS with multi-line
    void testCapLSMultiline() {
        TestNetworkManager mgr;

        mgr.clearCommands();

        // Simulate multi-line CAP LS responses using proper IRC format
        // First LS line (start of multi-line)
        mgr.parseLine("CAP nick LS :sasl server-time");
        // Continuation
        mgr.parseLine("CAP nick LS * :echo-message");
        QTest::qWait(100);

        // Final LS should trigger CAP REQ
        QStringList cmds = mgr.sentCommands();
        bool foundCapReq = false;
        for (const QString& cmd : cmds) {
            if (cmd.startsWith("CAP REQ")) {
                foundCapReq = true;
                break;
            }
        }
        QVERIFY(foundCapReq);

        QString capReqCmd;
        for (const QString& cmd : cmds) {
            if (cmd.startsWith("CAP REQ")) {
                capReqCmd = cmd;
                break;
            }
        }
        QString combined = capReqCmd.toLower();
        QVERIFY(combined.contains("sasl"));
        QVERIFY(combined.contains("server-time"));
        QVERIFY(combined.contains("echo-message"));
    }
};

QTEST_MAIN(TestIrcParser)

#include "test_irc_parser.moc"
