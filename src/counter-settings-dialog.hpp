#pragma once

#include <QDialog>
#include <QLineEdit>
#include <QPushButton>
#include <QString>

/*
 * CounterSettingsDialog
 * ----------------------
 * Dialog window for the counter settings.
 */
class CounterSettingsDialog : public QDialog {
	Q_OBJECT

public:
	explicit CounterSettingsDialog(const QString &currentOutputPath, QWidget *parent = nullptr);

	QString outputPath() const { return m_outputPath; }

private slots:
	void onBrowse();
	void onAccept();

private:
	void buildUi();

	QString m_outputPath;

	QLineEdit *m_pathEdit = nullptr;
	QPushButton *m_browseBtn = nullptr;
};
