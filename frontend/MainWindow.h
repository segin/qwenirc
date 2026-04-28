#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include "backend/NetworkManager.h"
#include "backend/IRCUser.h"
#include "backend/IRCMessageModel.h"
#include "ChannelTab.h"
#include <QMainWindow>
#include <QTabWidget>
#include <QListWidget>
#include <QListView>
#include <QMenuBar>
#include <QMenu>
#include <QToolBar>
#include <QStatusBar>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QSplitter>
#include <QStackedWidget>
#include <QList>
#include <QAbstractItemModel>

class MainWindow : public QMainWindow {
    Q_OBJECT

public:
    explicit MainWindow(QWidget* parent = nullptr);
    ~MainWindow() override;

private slots:
    void onConnect(const QString& host, quint16 port,
                   const QString& nick, const QString& pass,
                   const QString& channel);
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
    IRCChannelModel* m_channelModel;
    QString m_currentChannel;
    QString m_serverInfo;
    QAction* m_disconnectAction;
    ChannelTab* m_serverTab;
};

#endif // MAINWINDOW_H
