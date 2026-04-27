#include "ServerDialog.h"
#include <QLabel>
#include <QMessageBox>
#include <QSettings>
#include <QRandomGenerator>

ServerDialog::ServerDialog(QWidget* parent)
    : QDialog(parent)
{
    setWindowTitle("Connect to IRC");
    setMinimumSize(450, 320);

    QGridLayout* layout = new QGridLayout(this);

    QLabel* hostLabel = new QLabel("Server:");
    layout->addWidget(hostLabel, 0, 0);
    m_hostEdit = new QLineEdit("irc.libera.chat");
    layout->addWidget(m_hostEdit, 0, 1);

    QLabel* portLabel = new QLabel("Port:");
    layout->addWidget(portLabel, 1, 0);
    m_portEdit = new QLineEdit("6667");
    layout->addWidget(m_portEdit, 1, 1);

    QLabel* nickLabel = new QLabel("Nickname:");
    layout->addWidget(nickLabel, 2, 0);
    m_nickEdit = new QLineEdit();
    layout->addWidget(m_nickEdit, 2, 1);

    QLabel* passLabel = new QLabel("Password:");
    layout->addWidget(passLabel, 3, 0);
    m_passEdit = new QLineEdit();
    m_passEdit->setEchoMode(QLineEdit::Password);
    layout->addWidget(m_passEdit, 3, 1);

    QLabel* chanLabel = new QLabel("Channel:");
    layout->addWidget(chanLabel, 4, 0);
    m_channelEdit = new QLineEdit("#qwenirc");
    layout->addWidget(m_channelEdit, 4, 1);

    m_connectBtn = new QPushButton("Connect");
    m_connectBtn->setDefault(true);
    layout->addWidget(m_connectBtn, 5, 0);

    m_cancelBtn = new QPushButton("Cancel");
    layout->addWidget(m_cancelBtn, 5, 1);

    connect(m_connectBtn, &QPushButton::clicked, this, &ServerDialog::applyConnection);
    connect(m_cancelBtn, &QPushButton::clicked, this, &QWidget::close);

    // Load saved connection settings
    QSettings settings("QwenIRC", "Connection");
    m_hostEdit->setText(settings.value("host", "irc.libera.chat").toString());
    m_portEdit->setText(settings.value("port", "6667").toString());
    m_nickEdit->setText(settings.value("nick", "guest" + QString::number(QRandomGenerator::global()->bounded(1000))).toString());
}

void ServerDialog::applyConnection() {
    QSettings settings("QwenIRC", "Connection");
    settings.setValue("host", m_hostEdit->text());
    settings.setValue("port", m_portEdit->text());
    settings.setValue("nick", m_nickEdit->text());

    emit connectRequested(
        m_hostEdit->text(),
        static_cast<quint16>(m_portEdit->text().toUInt()),
        m_nickEdit->text(),
        m_passEdit->text(),
        m_channelEdit->text()
    );

    accept();
}
