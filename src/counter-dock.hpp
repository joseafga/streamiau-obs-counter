#pragma once

#include <QWidget>
#include <QLabel>
#include <QPushButton>
#include <QListWidget>
#include <QString>
#include <QDateTime>
#include <QAbstractSocket>

class QWebSocket;
class QTimer;

/*
 * CounterDock
 * -----------
 * An OBS Studio dock that displays a counter with control buttons.
 */
class CounterDock : public QWidget {
	Q_OBJECT

public:
	explicit CounterDock(QWidget *parent = nullptr);
	~CounterDock() override;

private slots:
	void onIncrement();
	void onDecrement();
	void onReset();
	void onOpenSettings();

	void onWebSocketConnected();
	void onWebSocketDisconnected();
	void onWebSocketErrorOccurred(QAbstractSocket::SocketError error);
	void onWebSocketTextMessageReceived(const QString &message);

private:
	void buildUi();
	void updateDisplay();
	void writeToFile();
	void addLogEntry(const QString &sender, const QString &message, qint64 newValue);
	void loadSettings();
	void saveSettings();
	QString configFilePath() const;

	void connectWebSocket();
	void updateWsStatusLabel(bool connected);
	void sendCounterUpdate();

	qint64 m_count = 0;
	QDateTime m_timestamp;
	QString m_outputPath;
	QString m_wsUrl;
	QString m_token;

	QLabel *m_counterLabel = nullptr;
	QPushButton *m_incBtn = nullptr;
	QPushButton *m_decBtn = nullptr;
	QPushButton *m_resetBtn = nullptr;
	QListWidget *m_logList = nullptr;
	QPushButton *m_settingsBtn = nullptr;
	QLabel *m_statusLabel = nullptr;
	QLabel *m_wsIndicatorLabel = nullptr;

	QWebSocket *m_webSocket = nullptr;
	QTimer *m_wsReconnectTimer = nullptr;
};
