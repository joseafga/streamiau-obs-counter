#pragma once

#include <QWidget>
#include <QLabel>
#include <QPushButton>
#include <QListWidget>
#include <QString>

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

private:
	void buildUi();
	void updateDisplay();
	void writeToFile();
	void addLogEntry(const QString &action, int newValue);
	void loadSettings();
	void saveSettings();
	QString configFilePath() const;

	int m_count = 0;
	QString m_outputPath;

	QLabel *m_counterLabel = nullptr;
	QPushButton *m_incBtn = nullptr;
	QPushButton *m_decBtn = nullptr;
	QPushButton *m_resetBtn = nullptr;
	QListWidget *m_logList = nullptr;
	QPushButton *m_settingsBtn = nullptr;
	QLabel *m_statusLabel = nullptr;
};
