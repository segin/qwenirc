#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include "ChannelTab.h"
#include "backend/IRCMessageModel.h"
#include "backend/IRCUser.h"
#include "backend/NetworkManager.h"
#include <QAbstractItemModel>
#include <QHBoxLayout>
#include <QList>
#include <QListWidget>
#include <QMainWindow>
#include <QMenu>
#include <QMenuBar>
#include <QRegularExpression>
#include <QSplitter>
#include <QStackedWidget>
#include <QStatusBar>
#include <QTabWidget>
#include <QToolBar>
#include <QVBoxLayout>

class MainWindow : public QMainWindow {
    Q_OBJECT

public:
    explicit MainWindow(QWidget* parent = nullptr);
    ~MainWindow() override;
    void setConnectionArgs(const QString& host, quint16 port, const QString& nick, const QString& pass,
                           const QString& channel, bool useTLS);

private slots:
    void onConnect(const QString& host, quint16 port, const QString& nick, const QString& pass, const QString& channel,
                   bool useTLS);
    void onDisconnected();
    void onServerError(const QString& error);
    void onServerMessage(const QString& message);
    void onServerChannelMessage(const QString& message);
    void onChannelMessage(const QString& channel, const IRCMessage& msg);
    void onChannelTopic(const QString& channel, const QString& topic);
    void onChannelMode(const QString& channel, const QString& mode);
    void onUserJoined(const QString& channel, const IRCUser& user);
    void onUserLeft(const QString& channel, const QString& nick, const QString& reason);
    void onUserChangedNick(const QString& oldNick, const QString& newNick);
    void addChannelTab(const QString& name);
    void removeChannelTab(const QString& name);
    void setStatus(const QString& status);
    void onNamesReceived(const QString& channel, const QList<IRCUser>& users);
    void onNamesComplete(const QString& channel);

    void addQueryTab(const QString& name);

private:
    ChannelTab* findChannelTab(const QString& name);
    ChannelTab* findQueryTab(const QString& name);
    void initializeUI();
    void createMenus();
    void createToolBars();
    void showConnectionDialog();

    NetworkManager* m_network;
    QTabWidget* m_channelTabs;
    QListWidget* m_channelList;
    QListWidget* m_userList;
    QMenuBar* m_menuBar;
    QMenu* m_menu;
    QMenu* m_viewMenu;
    QMenu* m_helpMenu;
    QMenu* m_channelMenu;
    QMenu* m_userMenu;
    QToolBar* m_toolBar;
    QStatusBar* m_statusBar;
    QSplitter* m_mainSplitter;
    QMap<QString, IRCMessageModel*> m_channelModels;
    QMap<QString, IRCMessageModel*> m_queryModels;
    QString m_currentChannel;
    QString m_serverInfo;
    QString m_cliHost;
    quint16 m_cliPort;
    QString m_cliNick;
    QString m_cliPass;
    QString m_cliChannel;
    bool m_cliTls;
    bool m_hasCliArgs;

    QAction* m_disconnectAction;
    ChannelTab* m_serverTab;
};

#endif // MAINWINDOW_H
