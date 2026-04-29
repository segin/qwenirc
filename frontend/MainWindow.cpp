#include "MainWindow.h"
#include "ServerDialog.h"
#include <QStyledItemDelegate>
#include <QPainter>
#include <QApplication>
#include <QAction>
#include <QMessageBox>
#include <QSettings>
#include <QRandomGenerator>
#include <QLabel>

class SidebarItemDelegate : public QStyledItemDelegate {
public:
    explicit SidebarItemDelegate(QObject* parent = nullptr) : QStyledItemDelegate(parent) {}

    void paint(QPainter* painter, const QStyleOptionViewItem& option, const QModelIndex& index) const override {
        QString text = index.data(Qt::DisplayRole).toString();
        QRect rect = option.rect;
        
        painter->save();
        painter->setBrush(option.palette.brush(QPalette::Base));
        painter->setPen(option.palette.color(QPalette::WindowText));
        
        painter->drawRect(rect);
        
        painter->drawText(rect.adjusted(5, 0, 0, 0), Qt::TextFlag::TextWordWrap, text);
        
        painter->restore();
    }

    QSize sizeHint(const QStyleOptionViewItem& option, const QModelIndex& index) const override {
        Q_UNUSED(index);
        QFontMetrics fm(option.font);
        int height = fm.height() + 4;
        return QSize(option.decorationSize.width(), height);
    }
};

MainWindow::MainWindow(QWidget* parent)
    : QMainWindow(parent)
    , m_network(nullptr)
    , m_channelTabs(nullptr)
    , m_channelList(nullptr)
    , m_userList(nullptr)
    , m_menuBar(nullptr)
    , m_menu(nullptr)
    , m_viewMenu(nullptr)
    , m_helpMenu(nullptr)
    , m_channelMenu(nullptr)
    , m_userMenu(nullptr)
    , m_toolBar(nullptr)
    , m_statusBar(nullptr)
    , m_mainSplitter(nullptr)
    , m_currentChannel("")
    , m_serverInfo("")
    , m_cliHost("")
    , m_cliPort(6667)
    , m_cliNick("")
    , m_cliPass("")
    , m_cliChannel("")
    , m_cliTls(false)
    , m_hasCliArgs(false)
    , m_disconnectAction(nullptr)
    , m_serverTab(nullptr)
{
    initializeUI();
    createMenus();
    createToolBars();

    setWindowTitle("QwenIRC - Not Connected");
    setMinimumSize(800, 600);

    // Show connection dialog on startup
    QTimer::singleShot(0, this, [this]() {
        showConnectionDialog();
    });
}

MainWindow::~MainWindow() {
}

void MainWindow::initializeUI() {
    m_network = new NetworkManager(this);

     // Central widget with vertical layout
    QWidget* centralWidget = new QWidget(this);
    QVBoxLayout* centralLayout = new QVBoxLayout(centralWidget);
    centralLayout->setContentsMargins(0, 0, 0, 0);

    // Three-pane splitter: channel list | channel tabs | user list
    QSplitter* mainSplitter = new QSplitter(Qt::Horizontal);
    mainSplitter->setHandleWidth(4);

    // Channel list sidebar
    m_channelList = new QListWidget();
    m_channelList->setMinimumWidth(100);
    m_channelList->setItemDelegate(new SidebarItemDelegate(this));
      mainSplitter->addWidget(m_channelList);

    // Channel tabs (QTabWidget)
    m_channelTabs = new QTabWidget();
    mainSplitter->addWidget(m_channelTabs);
    mainSplitter->setStretchFactor(1, 1);

    // User list sidebar
    m_userList = new QListWidget();
    m_userList->setMinimumWidth(100);
    m_userList->setItemDelegate(new SidebarItemDelegate(this));
    mainSplitter->addWidget(m_userList);

    centralLayout->addWidget(mainSplitter);
    mainSplitter->setSizes({150, 800, 200});

    // Server tab (for connection messages)
    m_serverTab = new ChannelTab("Server", m_network);
    m_serverTab->setTopicVisible(false);
    IRCMessageModel* serverMsgModel = new IRCMessageModel(this);
    m_serverTab->setChatModel(serverMsgModel);
    m_channelTabs->addTab(m_serverTab, "Server");
    connect(m_serverTab, &ChannelTab::messageSent, this, [this](const QString& message) {
        m_network->sendUserInput("Server", message);
    });

    setCentralWidget(centralWidget);

    // Status bar
    m_statusBar = new QStatusBar(this);
    setStatusBar(m_statusBar);

    // Network connections
    connect(m_network, &NetworkManager::stateChanged, this, [this](NetworkManager::State state) {
        switch (state) {
        case NetworkManager::Disconnected:
            setStatus("Disconnected");
            m_disconnectAction->setEnabled(false);
            break;
        case NetworkManager::Connecting:
        {
            setStatus("Connecting...");
            m_disconnectAction->setEnabled(true);
            IRCMessage msg(MessageType::Message, "Connecting to " + m_network->serverHost(), "Server");
            m_serverTab->addMessage(msg);
            break;
        }
        case NetworkManager::Connected:
            setStatus("Connected");
            m_disconnectAction->setEnabled(true);
            break;
        case NetworkManager::Error:
            setStatus("Error");
            m_disconnectAction->setEnabled(false);
            break;
        }
    });

  connect(m_network, &NetworkManager::connected, this, [this]() {
        setStatus("Connected to " + m_network->serverHost());
        IRCMessage msg(MessageType::Message, "Connected to " + m_network->serverHost(), "Server");
        m_serverTab->addMessage(msg);
    });

    connect(m_network, &NetworkManager::disconnected, this, &MainWindow::onDisconnected);
    connect(m_network, &NetworkManager::serverError, this, &MainWindow::onServerError);
    connect(m_network, &NetworkManager::serverMessage, this, &MainWindow::onServerMessage);
    connect(m_network, &NetworkManager::serverChannelMessage, this, &MainWindow::onServerChannelMessage);
    connect(m_network, &NetworkManager::channelMessage, this, &MainWindow::onChannelMessage);
    connect(m_network, &NetworkManager::channelTopic, this, &MainWindow::onChannelTopic);
    connect(m_network, &NetworkManager::channelMode, this, &MainWindow::onChannelMode);
    connect(m_network, &NetworkManager::userJoined, this, &MainWindow::onUserJoined);
    connect(m_network, &NetworkManager::userLeft, this, &MainWindow::onUserLeft);
   connect(m_network, &NetworkManager::userChangedNick, this, &MainWindow::onUserChangedNick);
    connect(m_network, &NetworkManager::channelRegistered, this, &MainWindow::addChannelTab);
    connect(m_network, &NetworkManager::channelUnregistered, this, &MainWindow::removeChannelTab);
    connect(m_network, &NetworkManager::namesReceived, this, &MainWindow::onNamesReceived);
    connect(m_network, &NetworkManager::namesComplete, this, &MainWindow::onNamesComplete);
    connect(m_network, &NetworkManager::queryTabNeeded, this, &MainWindow::addQueryTab);

    // Channel list double-click to join channel
    connect(m_channelList, &QListWidget::itemDoubleClicked, this, [this](QListWidgetItem* item) {
        QString name = item->text();
        m_network->joinChannel(name);
    });

    // Handle names received (user list)
    connect(m_network, &NetworkManager::userReceived, this, [this](const QString& channel, const QString& nick) {
        if (channel == m_currentChannel && m_userList->isVisible()) {
            bool found = false;
            for (int i = 0; i < m_userList->count(); ++i) {
                if (m_userList->item(i)->text() == nick) {
                    found = true;
                    break;
                }
            }
            if (!found) {
                m_userList->addItem(nick);
            }
        }
    });

    // Tab change handling - update current channel and user list
    connect(m_channelTabs, &QTabWidget::currentChanged, this, [this](int index) {
        QString tabName = m_channelTabs->tabText(index);
        if (tabName == "Server") {
            m_currentChannel = "";
            m_userList->clear();
            m_userList->setVisible(false);
        } else {
            m_currentChannel = tabName;
            m_userList->setVisible(true);
        }
    });
}

void MainWindow::createMenus() {
    QMenuBar *mb = menuBar();

    m_menu = new QMenu("&File", mb);
    QAction* connectAction = new QAction("&Connect...", this);
    connectAction->setShortcut(QKeySequence("Ctrl+M"));
    connectAction->setStatusTip("Connect to an IRC server");
    m_menu->addAction(connectAction);
    connect(connectAction, &QAction::triggered, this, &MainWindow::showConnectionDialog);

    m_menu->addSeparator();

    QAction* exitAction = new QAction("E&xit", this);
    exitAction->setShortcut(QKeySequence("Ctrl+Q"));
    m_menu->addAction(exitAction);
    connect(exitAction, &QAction::triggered, qApp, &QApplication::closeAllWindows);
    mb->addMenu(m_menu);

    m_viewMenu = new QMenu("&View", mb);

    QAction* channelAction = new QAction("&Channel List", this);
    channelAction->setCheckable(true);
    channelAction->setChecked(true);
    m_viewMenu->addAction(channelAction);
    connect(channelAction, &QAction::toggled, this, [this](bool checked) {
        m_channelList->setVisible(checked);
    });

    QAction* userAction = new QAction("&User List", this);
    userAction->setCheckable(true);
    userAction->setChecked(true);
    m_viewMenu->addAction(userAction);
    connect(userAction, &QAction::toggled, this, [this](bool checked) {
        m_userList->setVisible(checked);
    });
    mb->addMenu(m_viewMenu);

    m_helpMenu = new QMenu("&Help", mb);

    QAction* helpAction = new QAction("&About QwenIRC", this);
    m_helpMenu->addAction(helpAction);
    connect(helpAction, &QAction::triggered, this, [this]() {
        QMessageBox::about(this, "About QwenIRC",
            "QwenIRC - A Qt6 IRC Client\n"
            "Version 0.1.0\n\n"
            "Supporting IRCv3 capabilities including:\n"
            "- Message Tags\n"
            "- Account Notification\n"
            "- Away Notification\n"
            "- Echo Message\n"
            "- Multi-prefix");
    });
    mb->addMenu(m_helpMenu);
}

void MainWindow::createToolBars() {
    m_toolBar = addToolBar("&Connect");

    QAction* connectAction = new QAction("Connect", this);
    connect(connectAction, &QAction::triggered, this, &MainWindow::showConnectionDialog);
    m_toolBar->addAction(connectAction);

    QAction* disconnectAction = new QAction("Disconnect", this);
    disconnectAction->setEnabled(false);
    m_toolBar->addAction(disconnectAction);
    connect(disconnectAction, &QAction::triggered, this, [this]() {
        m_network->disconnect();
    });
    m_disconnectAction = disconnectAction;

    m_toolBar->addSeparator();

    m_toolBar->addActions(m_viewMenu->actions());
}

void MainWindow::setConnectionArgs(const QString& host, quint16 port,
                                   const QString& nick, const QString& pass,
                                   const QString& channel, bool useTLS) {
    m_cliHost = host;
    m_cliPort = port;
    m_cliNick = nick;
    m_cliPass = pass;
    m_cliChannel = channel;
    m_cliTls = useTLS;
    m_hasCliArgs = true;
}

void MainWindow::showConnectionDialog() {
    ServerDialog dialog(this);

    if (m_hasCliArgs) {
        dialog.setHost(m_cliHost);
        dialog.setPort(m_cliPort);
        dialog.setNick(m_cliNick);
        dialog.setPass(m_cliPass);
        dialog.setChannel(m_cliChannel);
        dialog.setUseTLS(m_cliTls);
    }

    connect(&dialog, &ServerDialog::connectRequested, this, &MainWindow::onConnect);

    dialog.exec();
}

void MainWindow::setStatus(const QString& status) {
    if (m_statusBar) {
        m_statusBar->showMessage(status);
    }
}

void MainWindow::onConnect(const QString& host, quint16 port,
                           const QString& nick, const QString& pass,
                           const QString& channel, bool useTLS) {
    m_network->connectToServer(host, port, nick, pass, channel, useTLS);
}

void MainWindow::onDisconnected() {
    setStatus("Disconnected");
    IRCMessage msg(MessageType::Message, "Disconnected from server", "Server");
    m_serverTab->addMessage(msg);
}

void MainWindow::onServerError(const QString& error) {
    IRCMessage msg(MessageType::Message, error, "Server");
    m_serverTab->addMessage(msg);
}

  void MainWindow::onServerMessage(const QString& message) {
    IRCMessage msg(MessageType::Message, message, "Server");
    m_serverTab->addMessage(msg);
    setStatus(message);
}

void MainWindow::onServerChannelMessage(const QString& message) {
    IRCMessage msg(MessageType::Message, message, "Server");
    m_serverTab->addMessage(msg);
}

void MainWindow::onChannelMessage(const QString& channel, const IRCMessage& msg) {
    auto* tab = findChannelTab(channel);
    if (!tab) {
        tab = findQueryTab(channel);
    }
    if (tab) {
        tab->addMessage(msg);
    }
}

void MainWindow::onChannelTopic(const QString& channel, const QString& topic) {
    auto* tab = findChannelTab(channel);
    if (tab) {
        tab->setTopic(topic);
    }
}

void MainWindow::onChannelMode(const QString& channel, const QString& mode) {
    auto* tab = findChannelTab(channel);
    if (tab) {
        tab->setMode(mode);
    }
}

void MainWindow::onUserJoined(const QString& channel, const IRCUser& user) {
    if (channel == m_currentChannel) {
        m_userList->addItem(user.nick());
    }
}

void MainWindow::onUserLeft(const QString& channel, const QString& nick, const QString& reason) {
    Q_UNUSED(reason);
    if (channel == m_currentChannel) {
        QRegularExpression re("[~&@%+]");
        for (int i = 0; i < m_userList->count(); ++i) {
            QString text = m_userList->item(i)->text();
            text.replace(re, "");
            if (text == nick) {
                m_userList->takeItem(i);
                break;
            }
        }
    }
}

void MainWindow::onUserChangedNick(const QString& oldNick, const QString& newNick) {
    IRCMessage msg(MessageType::NickChange,
        QString("%1 -> %2").arg(oldNick).arg(newNick),
        oldNick);

    // Post to all channel tabs where old nick was present
    for (auto it = m_network->channels().begin(); it != m_network->channels().end(); ++it) {
        IRCChannel* ch = it.value();
        if (ch->findUser(oldNick) != nullptr) {
            ChannelTab* tab = findChannelTab(it.key());
            if (tab) {
                tab->addMessage(msg);
            }
        }
    }

    // Update the user list for current channel
    if (!m_currentChannel.isEmpty()) {
        QRegularExpression re("[~&@%+]");
        IRCChannel* ch = m_network->channel(m_currentChannel);
        IRCUser* user = ch ? ch->findUser(oldNick) : nullptr;
        for (int i = 0; i < m_userList->count(); ++i) {
            QString text = m_userList->item(i)->text();
            QString bareNick = text;
            bareNick.replace(re, "");
            if (bareNick == oldNick) {
                if (user) {
                    m_userList->item(i)->setText(user->userPrefix() + newNick);
                } else {
                    m_userList->item(i)->setText(newNick);
                }
                break;
            }
        }
    }
}

void MainWindow::addChannelTab(const QString& name) {
    m_currentChannel = name;
    m_network->setCurrentChannel(name);

    // Create per-channel message model
    IRCMessageModel* msgModel = new IRCMessageModel(this);
    m_channelModels[name] = msgModel;

    // Add tab
     ChannelTab* tab = new ChannelTab(name, m_network);
    tab->setChatModel(msgModel);
    connect(tab, &ChannelTab::messageSent, this, [this, name](const QString& message) {
        m_network->sendUserInput(name, message);
    });

    m_channelTabs->addTab(tab, name);
    m_channelList->addItem(name);
}

void MainWindow::removeChannelTab(const QString& name) {
    int idx = -1;
    for (int i = 0; i < m_channelTabs->count(); ++i) {
        if (m_channelTabs->tabText(i) == name) {
            idx = i;
            break;
        }
    }
    if (idx >= 0) {
        m_channelTabs->removeTab(idx);
    }

    if (m_channelModels.contains(name)) {
        delete m_channelModels.take(name);
    }
}

ChannelTab* MainWindow::findChannelTab(const QString& name) {
    for (int i = 0; i < m_channelTabs->count(); ++i) {
        if (m_channelTabs->tabText(i) == name) {
            return qobject_cast<ChannelTab*>(m_channelTabs->widget(i));
        }
    }
    return nullptr;
}

void MainWindow::onNamesReceived(const QString& channel, const QList<IRCUser>& users) {
    Q_UNUSED(channel);
    if (channel == m_currentChannel) {
        m_userList->clear();
        for (const auto& user : users) {
            QString nick = user.nick();
            QString mode = user.userPrefix();
            if (!mode.isEmpty()) {
                m_userList->addItem(mode + nick);
            } else {
                m_userList->addItem(nick);
            }
        }
    }
}

void MainWindow::onNamesComplete(const QString& channel) {
    Q_UNUSED(channel);
}

void MainWindow::addQueryTab(const QString& name) {
    // Check if query tab already exists
    for (int i = 0; i < m_channelTabs->count(); ++i) {
        if (m_channelTabs->tabText(i) == name) {
            return;
        }
    }

    IRCMessageModel* msgModel = new IRCMessageModel(this);
    m_queryModels[name] = msgModel;

    ChannelTab* tab = new ChannelTab(name, m_network);
    tab->setChatModel(msgModel);
    tab->setTopicVisible(false);
    connect(tab, &ChannelTab::messageSent, this, [this, name](const QString& message) {
        m_network->sendUserInput(name, message);
    });

    m_channelTabs->insertTab(0, tab, name);
    m_channelTabs->setCurrentWidget(tab);
}

ChannelTab* MainWindow::findQueryTab(const QString& name) {
    for (int i = 0; i < m_channelTabs->count(); ++i) {
        if (m_channelTabs->tabText(i) == name) {
            return qobject_cast<ChannelTab*>(m_channelTabs->widget(i));
        }
    }
    return nullptr;
}