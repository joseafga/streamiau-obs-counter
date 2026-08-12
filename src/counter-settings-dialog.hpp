#pragma once

#include <QDialog>
#include <QLineEdit>
#include <QPushButton>
#include <QString>
#include <QLabel>

/*
 * CounterSettingsDialog
 * ----------------------
 * Dialog window for the counter settings.
 */
class CounterSettingsDialog : public QDialog {
	Q_OBJECT

public:
	explicit CounterSettingsDialog(const QString &currentOutputPath, const QString &currentWsUrl,
				       const QString &currentToken, QWidget *parent = nullptr);

	QString outputPath() const { return m_outputPath; }
	QString wsUrl() const { return m_wsUrl; }
	QString token() const { return m_token; }

private slots:
	void onBrowse();
	void onAccept();

private:
	void buildUi();

	QString m_outputPath;
	QString m_wsUrl;
	QString m_token;

	QLineEdit *m_pathEdit = nullptr;
	QPushButton *m_browseBtn = nullptr;
	QLineEdit *m_wsUrlEdit = nullptr;
	QLineEdit *m_tokenEdit = nullptr;
	QLabel *m_tokenDesc = nullptr;
};
