#pragma once

#include <obs.h>
#include <memory>
#include <QWidget>
#include <QLabel>
#include <QPushButton>
#include <QListWidget>
#include <QString>
#include <QDateTime>
#include <QTimer>

namespace ix {
class WebSocket;
}

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

protected:
	void showEvent(QShowEvent *event) override;
	void hideEvent(QHideEvent *event) override;

private:
	void buildUi();
	void updateDisplay();
	void writeToFile();
	void addLogEntry(const QString &sender, const QString &message, qint64 newValue);
	void loadSettings();
	void saveSettings();
	QString configFilePath() const;

	void setupSender();
	void setupWebSocket();
	void updateWsStatusLabel(bool connected);
	void sendCounterUpdate();

	void onWebSocketConnected();
	void onWebSocketDisconnected();
	void onWebSocketErrorOccurred(const QString &reason);
	void onWebSocketTextMessageReceived(const QString &message);

	void registerHotkeys();
	void unregisterHotkeys();
	void onFrontendSave(obs_data_t *saveData, bool saving);

	static void hotkeyIncrementCallback(void *data, obs_hotkey_id id, obs_hotkey_t *hotkey, bool pressed);
	static void hotkeyDecrementCallback(void *data, obs_hotkey_id id, obs_hotkey_t *hotkey, bool pressed);
	static void hotkeyResetCallback(void *data, obs_hotkey_id id, obs_hotkey_t *hotkey, bool pressed);
	static void frontendSaveCallback(obs_data_t *saveData, bool saving, void *data);

	qint64 m_count = 0;
	QDateTime m_timestamp;
	QString m_outputPath;
	QString m_wsUrl;
	QString m_token;
	QString m_sender;
	std::unique_ptr<ix::WebSocket> m_webSocket;

	obs_hotkey_id m_hotkeyIncrement = OBS_INVALID_HOTKEY_ID;
	obs_hotkey_id m_hotkeyDecrement = OBS_INVALID_HOTKEY_ID;
	obs_hotkey_id m_hotkeyReset = OBS_INVALID_HOTKEY_ID;

	QLabel *m_counterLabel = nullptr;
	QPushButton *m_incBtn = nullptr;
	QPushButton *m_decBtn = nullptr;
	QPushButton *m_resetBtn = nullptr;
	QListWidget *m_logList = nullptr;
	QPushButton *m_settingsBtn = nullptr;
	QLabel *m_statusLabel = nullptr;
	QLabel *m_wsIndicatorLabel = nullptr;
	QTimer *m_sendTimer = nullptr;
};
