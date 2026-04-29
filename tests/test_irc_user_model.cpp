#include <QtTest/QtTest>
#include <QAbstractSocket>

#include "../backend/NetworkManager.h"
#include "../backend/IRCChannel.h"
#include "../backend/IRCUser.h"
#include "../backend/IRCMessage.h"
#include "../backend/IRCMessageModel.h"

class TestIRCUserModel : public QObject {
    Q_OBJECT
public:
    explicit TestIRCUserModel(QObject* parent = nullptr) : QObject(parent) {}

private slots:
    // Test: addUser with duplicate nick (case-insensitive) does not insert duplicate
    void testAddUserDuplicateNick() {
        IRCUserModel model;

        IRCUser user1("alice", "alice", "host1");
        model.addUser(user1);
        QCOMPARE(model.users().size(), 1);

        // Try to add a duplicate with different case
        IRCUser user2("Alice", "alice2", "host2");
        model.addUser(user2);

        // Should not insert duplicate (case-insensitive comparison)
        QCOMPARE(model.users().size(), 1);

        // Original user should still be there with original nick
        QCOMPARE(model.users()[0].nick(), QString("alice"));
    }

    // Test: removeUser("alice") removes @alice from display list
    void testRemoveUserRemovesFromDisplay() {
        IRCUserModel model;

        IRCUser user1("alice", "alice", "host1");
        user1.setUserPrefix("@");
        IRCUser user2("bob", "bob", "host2");
        user2.setUserPrefix("+");

        QList<IRCUser> users = { user1, user2 };
        model.setUsers(users);
        QCOMPARE(model.users().size(), 2);

        // Verify @alice appears in display (Qt::DisplayRole)
        QVariant disp = model.data(model.index(0, 0), Qt::DisplayRole);
        QCOMPARE(disp.toString(), QString("@alice"));

        // Remove alice
        model.removeUser("alice");

        // Should have 1 user left
        QCOMPARE(model.users().size(), 1);

        // Verify alice is gone from display
        disp = model.data(model.index(0, 0), Qt::DisplayRole);
        QCOMPARE(disp.toString(), QString("+bob"));

        // Verify only nick role returns "bob"
        QVariant nickVar = model.data(model.index(0, 0), IRCUserModel::NickRole);
        QCOMPARE(nickVar.toString(), QString("bob"));
    }

    // Test: setUsers populates model and emits modelReset
    void testSetUsersPopulatesModel() {
        IRCUserModel model;

        IRCUser user1("alice", "alice", "host1");
        IRCUser user2("bob", "bob", "host2");
        IRCUser user3("charlie", "charlie", "host3");

        QList<IRCUser> users = { user1, user2, user3 };

        // Track modelReset signal
        bool modelResetSignal = false;
        QObject::connect(&model, &IRCUserModel::modelReset,
                         [&modelResetSignal]() {
                             modelResetSignal = true;
                         });

        model.setUsers(users);

        // Verify modelReset was emitted
        QVERIFY(modelResetSignal);

        // Verify all users are populated
        QCOMPARE(model.users().size(), 3);

        // Verify users are sorted alphabetically
        QCOMPARE(model.users()[0].nick(), QString("alice"));
        QCOMPARE(model.users()[1].nick(), QString("bob"));
        QCOMPARE(model.users()[2].nick(), QString("charlie"));

        // Verify data() returns correct display strings
        QCOMPARE(model.data(model.index(0, 0), Qt::DisplayRole).toString(), QString("alice"));
        QCOMPARE(model.data(model.index(1, 0), Qt::DisplayRole).toString(), QString("bob"));
        QCOMPARE(model.data(model.index(2, 0), Qt::DisplayRole).toString(), QString("charlie"));

        // Verify row count
        QCOMPARE(model.rowCount(), 3);
    }
};

QTEST_MAIN(TestIRCUserModel)

#include "test_irc_user_model.moc"
