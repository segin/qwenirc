#ifndef SERVERDIALOG_H
#define SERVERDIALOG_H

#include <QDialog>
#include <QLineEdit>
#include <QGridLayout>
#include <QPushButton>
#include <QIntValidator>
#include <QCheckBox>

class ServerDialog : public QDialog {
    Q_OBJECT

public:
    explicit ServerDialog(QWidget* parent = nullptr);

    QString host() const { return m_hostEdit->text(); }
    quint16 port() const {
    bool ok;
    uint val = m_portEdit->text().toUInt(&ok);
    return ok ? static_cast<quint16>(val) : 6667;
}
    QString nick() const { return m_nickEdit->text(); }
    QString password() const { return m_passEdit->text(); }
    QString channel() const { return m_channelEdit->text(); }

signals:
    void connectRequested(const QString& host, quint16 port,
                          const QString& nick, const QString& pass,
                          const QString& channel, bool useTLS);

private:
    void applyConnection();

    QLineEdit* m_hostEdit;
    QLineEdit* m_portEdit;
    QLineEdit* m_nickEdit;
    QLineEdit* m_passEdit;
    QLineEdit* m_channelEdit;
    QPushButton* m_connectBtn;
    QPushButton* m_cancelBtn;
    QCheckBox* m_tlsCheckBox;
    QString m_channel;
};

#endif // SERVERDIALOG_H
