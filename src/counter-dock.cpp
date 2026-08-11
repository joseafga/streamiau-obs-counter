#include "counter-dock.hpp"
#include "counter-settings-dialog.hpp"

#include <obs-module.h>
#include <util/bmem.h>

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFile>
#include <QFileInfo>
#include <QDir>
#include <QTextStream>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTime>
#include <QFont>

namespace {
constexpr int kMaxLogEntries = 50;
}

CounterDock::CounterDock(QWidget *parent) : QWidget(parent)
{
	buildUi();
	loadSettings();
	updateDisplay();

	writeToFile();
}

CounterDock::~CounterDock() {}

void CounterDock::buildUi()
{
	auto *mainLayout = new QVBoxLayout(this);
	mainLayout->setContentsMargins(0, 0, 0, 0);
	mainLayout->setSpacing(8);

	m_counterLabel = new QLabel(QStringLiteral("0"), this);
	m_counterLabel->setStyleSheet("font-family: sans-serif; font-size: 48px; font-weight: bold;");
	m_counterLabel->setAlignment(Qt::AlignCenter);
	m_counterLabel->setFrameShape(QFrame::StyledPanel);
	m_counterLabel->setMinimumHeight(56);

	m_incBtn = new QPushButton(QStringLiteral("\u25B2"), this);
	m_incBtn->setProperty("themeID", "upArrowIconSmall");
	m_incBtn->setProperty("class", "icon-up");
	m_incBtn->setText("");
	m_incBtn->setEnabled(true);

	m_decBtn = new QPushButton(QStringLiteral("\u25BC"), this);
	m_decBtn->setProperty("themeID", "downArrowIconSmall");
	m_decBtn->setProperty("class", "icon-down");
	m_decBtn->setText("");
	m_decBtn->setEnabled(true);

	mainLayout->addWidget(m_incBtn);
	mainLayout->addWidget(m_counterLabel);
	mainLayout->addWidget(m_decBtn);

	auto *row = new QHBoxLayout();
	m_resetBtn = new QPushButton(tr("Zerar"), this);

	m_settingsBtn = new QPushButton(QStringLiteral("\u2699"), this);
	m_settingsBtn->setToolTip(tr("Configurações"));
	m_settingsBtn->setFixedWidth(36);
	m_settingsBtn->setProperty("themeID", "propertiesIconSmall");
	m_settingsBtn->setProperty("class", "icon-gear");
	m_settingsBtn->setText("");
	m_settingsBtn->setEnabled(true);

	row->addWidget(m_resetBtn);
	row->addStretch(1);
	row->addWidget(m_settingsBtn);
	mainLayout->addLayout(row);

	m_logList = new QListWidget(this);
	m_logList->setAlternatingRowColors(true);
	mainLayout->addWidget(m_logList, 1);

	m_statusLabel = new QLabel(this);
	m_statusLabel->setStyleSheet("color: gray; font-style: italic;");
	mainLayout->addWidget(m_statusLabel);

	connect(m_incBtn, &QPushButton::clicked, this, &CounterDock::onIncrement);
	connect(m_decBtn, &QPushButton::clicked, this, &CounterDock::onDecrement);
	connect(m_resetBtn, &QPushButton::clicked, this, &CounterDock::onReset);
	connect(m_settingsBtn, &QPushButton::clicked, this, &CounterDock::onOpenSettings);
}

void CounterDock::onIncrement()
{
	m_count += 1;
	updateDisplay();
	writeToFile();
	addLogEntry(tr("Incremento"), m_count);
	saveSettings();
}

void CounterDock::onDecrement()
{
	m_count -= 1;
	updateDisplay();
	writeToFile();
	addLogEntry(tr("Decremento"), m_count);
	saveSettings();
}

void CounterDock::onReset()
{
	if (m_count == 0)
		return;

	m_count = 0;
	updateDisplay();
	writeToFile();
	addLogEntry(tr("Resetado"), m_count);
	saveSettings();
}

void CounterDock::onOpenSettings()
{
	CounterSettingsDialog dialog(m_outputPath, this);

	if (dialog.exec() == QDialog::Accepted) {
		m_outputPath = dialog.outputPath();
		saveSettings();
		writeToFile();
		m_statusLabel->setText(tr("Configurações salvas."));
	}
}

void CounterDock::updateDisplay()
{
	m_counterLabel->setText(QString::number(m_count));
}

void CounterDock::writeToFile()
{
	if (m_outputPath.isEmpty()) {
		if (m_statusLabel)
			m_statusLabel->setText(tr("Nenhum arquivo configurado. Clique em \"Configurações...\"."));
		return;
	}

	QFileInfo info(m_outputPath);
	QDir().mkpath(info.absolutePath());

	QFile file(m_outputPath);
	if (!file.open(QIODevice::WriteOnly | QIODevice::Text | QIODevice::Truncate)) {
		if (m_statusLabel)
			m_statusLabel->setText(tr("Erro ao gravar o arquivo!"));
		return;
	}

	QTextStream stream(&file);
	stream << m_count;
	file.close();

	if (m_statusLabel)
		m_statusLabel->setText(tr("Arquivo atualizado."));
}

void CounterDock::addLogEntry(const QString &action, int newValue)
{
	QString timestamp = QTime::currentTime().toString("HH:mm:ss");
	QString text = QString("[%1] fulano: %2 - %3").arg(timestamp).arg(newValue).arg(action);

	m_logList->insertItem(0, text);

	while (m_logList->count() > kMaxLogEntries) {
		QListWidgetItem *item = m_logList->takeItem(m_logList->count() - 1);
		delete item;
	}
}

QString CounterDock::configFilePath() const
{
	char *cpath = obs_module_config_path("settings.json");
	QString path = QString::fromUtf8(cpath ? cpath : "");
	bfree(cpath);

	if (path.isEmpty())
		return path;

	QDir().mkpath(QFileInfo(path).absolutePath());
	return path;
}

void CounterDock::loadSettings()
{
	QString path = configFilePath();
	if (path.isEmpty())
		return;

	QFile file(path);
	if (!file.open(QIODevice::ReadOnly))
		return;

	QJsonDocument doc = QJsonDocument::fromJson(file.readAll());
	file.close();

	if (!doc.isObject())
		return;

	QJsonObject obj = doc.object();
	m_count = obj.value("count").toInt(0);
	m_outputPath = obj.value("outputPath").toString();
}

void CounterDock::saveSettings()
{
	QString path = configFilePath();
	if (path.isEmpty())
		return;

	QJsonObject obj;
	obj["count"] = m_count;
	obj["outputPath"] = m_outputPath;

	QFile file(path);
	if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate))
		return;

	file.write(QJsonDocument(obj).toJson(QJsonDocument::Compact));
	file.close();
}
