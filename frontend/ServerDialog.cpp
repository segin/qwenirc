#include "ServerDialog.h"
#include <QLabel>
#include <QMessageBox>
#include <QSettings>
#include <QRandomGenerator>
#include <QCheckBox>

ServerDialog::ServerDialog(QWidget* parent)
    : QDialog(parent)
{
    setWindowTitle("Connect to IRC");
    setMinimumSize(450, 360);

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

    m_tlsCheckBox = new QCheckBox("Use TLS/SSL");
    layout->addWidget(m_tlsCheckBox, 5, 0, 1, 2);

    connect(m_tlsCheckBox, &QCheckBox::toggled, this, [this](bool checked) {
        if (checked && m_portEdit->text().toUInt() == 6667) {
            m_portEdit->setText("6697");
        }
    });

    m_connectBtn = new QPushButton("Connect");
    m_connectBtn->setDefault(true);
    layout->addWidget(m_connectBtn, 6, 0);

    m_cancelBtn = new QPushButton("Cancel");
    layout->addWidget(m_cancelBtn, 6, 1);

    connect(m_connectBtn, &QPushButton::clicked, this, &ServerDialog::applyConnection);
    connect(m_cancelBtn, &QPushButton::clicked, this, &QDialog::reject);

    m_portEdit->setValidator(new QIntValidator(1, 65535, this));

    // Load saved connection settings
    QSettings settings("QwenIRC", "Connection");
    m_hostEdit->setText(settings.value("host", "irc.libera.chat").toString());
    m_portEdit->setText(settings.value("port", "6667").toString());
    m_nickEdit->setText(settings.value("nick", "guest" + QString::number(QRandomGenerator::global()->bounded(1000))).toString());
    m_channelEdit->setText(settings.value("channel", "#qwenirc").toString());
    m_tlsCheckBox->setChecked(settings.value("tls", false).toBool());
}

void ServerDialog::applyConnection() {
    QSettings settings("QwenIRC", "Connection");
    settings.setValue("host", m_hostEdit->text());
    settings.setValue("port", m_portEdit->text());
    settings.setValue("nick", m_nickEdit->text());
    settings.setValue("channel", m_channelEdit->text());
    settings.setValue("tls", m_tlsCheckBox->isChecked());

    QStringList channels;
    for (const auto& c : m_channelEdit->text().split(',', Qt::SkipEmptyParts)) {
        QString trimmed = c.trimmed();
        if (!trimmed.isEmpty()) {
            channels << trimmed;
        }
    }
    QString channelStr = channels.join(",");

    emit connectRequested(
        m_hostEdit->text(),
        static_cast<quint16>(m_portEdit->text().toUInt()),
        m_nickEdit->text(),
        m_passEdit->text(),
        channelStr,
        m_tlsCheckBox->isChecked()
    );

    accept();
}
