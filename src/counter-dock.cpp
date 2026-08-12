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
#include <QJsonParseError>
#include <QDateTime>
#include <QFont>
#include <QUrl>
#include <QTimer>
#include <QWebSocket>
#include "counter-dock.hpp"
#include "counter-settings-dialog.hpp"

namespace {
constexpr int kMaxLogEntries = 50;
constexpr int kWsReconnectDelayMs = 5000;
} // namespace

CounterDock::CounterDock(QWidget *parent) : QWidget(parent)
{
	buildUi();
	loadSettings();
	updateDisplay();

	writeToFile();
	connectWebSocket();
}

CounterDock::~CounterDock()
{
	if (m_webSocket) {
		m_webSocket->disconnect(this);
		m_webSocket->close();
		m_webSocket->deleteLater();
		m_webSocket = nullptr;
	}
}

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

	m_resetBtn = new QPushButton(obs_module_text("Reset"), this);

	m_settingsBtn = new QPushButton(QStringLiteral("\u2699"), this);
	m_settingsBtn->setToolTip(obs_module_text("Settings"));
	m_settingsBtn->setFixedWidth(36);
	m_settingsBtn->setProperty("themeID", "propertiesIconSmall");
	m_settingsBtn->setProperty("class", "icon-gear");
	m_settingsBtn->setText("");
	m_settingsBtn->setEnabled(true);

	auto *row = new QHBoxLayout();
	row->addWidget(m_resetBtn);
	row->addStretch(1);
	row->addWidget(m_settingsBtn);
	mainLayout->addLayout(row);

	m_logList = new QListWidget(this);
	m_logList->setAlternatingRowColors(true);
	m_logList->setMinimumHeight(36); // single line view
	m_logList->setStyleSheet("QListWidget::item { padding: 0px; margin: 0px; }");
	mainLayout->addWidget(m_logList, 1);

	// Status bar
	m_statusLabel = new QLabel(this);
	m_statusLabel->setStyleSheet("color: gray; font-style: italic;");

	m_wsIndicatorLabel = new QLabel(this);
	m_wsIndicatorLabel->setFixedSize(12, 12);
	m_wsIndicatorLabel->setToolTip(obs_module_text("WebSocketStatus"));
	updateWsStatusLabel(false);

	auto *statusBar = new QHBoxLayout();
	statusBar->addWidget(m_statusLabel);
	statusBar->addStretch(1);
	statusBar->addWidget(m_wsIndicatorLabel);
	mainLayout->addLayout(statusBar);

	connect(m_incBtn, &QPushButton::clicked, this, &CounterDock::onIncrement);
	connect(m_decBtn, &QPushButton::clicked, this, &CounterDock::onDecrement);
	connect(m_resetBtn, &QPushButton::clicked, this, &CounterDock::onReset);
	connect(m_settingsBtn, &QPushButton::clicked, this, &CounterDock::onOpenSettings);

	m_wsReconnectTimer = new QTimer(this);
	m_wsReconnectTimer->setSingleShot(true);
	connect(m_wsReconnectTimer, &QTimer::timeout, this, &CounterDock::connectWebSocket);
}

void CounterDock::onIncrement()
{
	m_count += 1;
	updateDisplay();
	writeToFile();
	addLogEntry(QStringLiteral("OBS"), QStringLiteral(""), m_count);
	sendCounterUpdate();
	saveSettings();
}

void CounterDock::onDecrement()
{
	m_count -= 1;
	updateDisplay();
	writeToFile();
	addLogEntry(QStringLiteral("OBS"), QStringLiteral(""), m_count);
	sendCounterUpdate();
	saveSettings();
}

void CounterDock::onReset()
{
	if (m_count == 0)
		return;

	m_count = 0;
	updateDisplay();
	writeToFile();
	addLogEntry(QStringLiteral("OBS"), QStringLiteral(""), m_count);
	sendCounterUpdate();
	saveSettings();
}

void CounterDock::onOpenSettings()
{
	CounterSettingsDialog dialog(m_outputPath, m_wsUrl, m_token, this);

	if (dialog.exec() == QDialog::Accepted) {
		bool wsUrlChanged = dialog.wsUrl() != m_wsUrl;

		m_outputPath = dialog.outputPath();
		m_wsUrl = dialog.wsUrl();
		m_token = dialog.token();

		saveSettings();
		writeToFile();
		m_statusLabel->setText(obs_module_text("SettingsSaved"));

		if (wsUrlChanged)
			connectWebSocket();
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
			m_statusLabel->setText(obs_module_text("SettingsNoFile"));
		return;
	}

	QFileInfo info(m_outputPath);
	QDir().mkpath(info.absolutePath());

	QFile file(m_outputPath);
	if (!file.open(QIODevice::WriteOnly | QIODevice::Text | QIODevice::Truncate)) {
		if (m_statusLabel)
			m_statusLabel->setText(obs_module_text("FileWriteError"));
		return;
	}

	QTextStream stream(&file);
	stream << m_count;
	file.close();
}

void CounterDock::addLogEntry(const QString &sender, const QString &message, qint64 newValue)
{
	m_timestamp = QDateTime::currentDateTime();
	QString text = QString("[%1] %2: %3").arg(m_timestamp.toString("HH:mm:ss")).arg(sender).arg(newValue);

	if (!message.isEmpty()) {
		text += QString(" - %1").arg(message);
	}

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
	m_count = obj.value("count").toVariant().toLongLong();
	m_outputPath = obj.value("outputPath").toString();
	m_wsUrl = obj.value("wsUrl").toString();
	m_token = obj.value("token").toString();
}

void CounterDock::saveSettings()
{
	QString path = configFilePath();
	if (path.isEmpty())
		return;

	QJsonObject obj;
	obj["count"] = m_count;
	obj["outputPath"] = m_outputPath;
	obj["wsUrl"] = m_wsUrl;
	obj["token"] = m_token;

	QFile file(path);
	if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate))
		return;

	file.write(QJsonDocument(obj).toJson());
	file.close();
}

void CounterDock::connectWebSocket()
{
	m_wsReconnectTimer->stop();

	if (m_webSocket) {
		m_webSocket->disconnect(this);
		m_webSocket->close();
		m_webSocket->deleteLater();
		m_webSocket = nullptr;
	}

	if (m_wsUrl.trimmed().isEmpty()) {
		updateWsStatusLabel(false);
		return;
	}

	m_webSocket = new QWebSocket(QString(), QWebSocketProtocol::VersionLatest, this);

	connect(m_webSocket, &QWebSocket::connected, this, &CounterDock::onWebSocketConnected);
	connect(m_webSocket, &QWebSocket::disconnected, this, &CounterDock::onWebSocketDisconnected);
	connect(m_webSocket, &QWebSocket::textMessageReceived, this, &CounterDock::onWebSocketTextMessageReceived);
	connect(m_webSocket, &QWebSocket::errorOccurred, this, &CounterDock::onWebSocketErrorOccurred);

	m_webSocket->open(QUrl(m_wsUrl.trimmed()));
}

void CounterDock::onWebSocketConnected()
{
	updateWsStatusLabel(true);
}

void CounterDock::onWebSocketDisconnected()
{
	updateWsStatusLabel(false);

	// Keep reconnecting
	if (!m_wsUrl.trimmed().isEmpty())
		m_wsReconnectTimer->start(kWsReconnectDelayMs);
}

void CounterDock::onWebSocketErrorOccurred(QAbstractSocket::SocketError error)
{
	Q_UNUSED(error)
	updateWsStatusLabel(false);

	// Keep reconnecting
	if (!m_wsUrl.trimmed().isEmpty())
		m_wsReconnectTimer->start(kWsReconnectDelayMs);
}

void CounterDock::onWebSocketTextMessageReceived(const QString &message)
{
	QJsonParseError parseError;
	QJsonDocument doc = QJsonDocument::fromJson(message.toUtf8(), &parseError);

	if (parseError.error != QJsonParseError::NoError || !doc.isObject())
		return;

	QJsonObject obj = doc.object();

	if (obj.value("type").toString() != "counter")
		return;

	if (!obj.contains("value"))
		return;

	// JSON value is a UInt32
	qint64 newValue = static_cast<qint64>(obj.value("value").toInteger());

	if (newValue == m_count)
		return;

	// Fallback
	QString sender = QStringLiteral("WebSocket");
	QString note;

	if (obj.contains("metadata") && obj.value("metadata").isObject()) {
		QJsonObject metadata = obj.value("metadata").toObject();
		sender = metadata.value("sender").toString();
		note = metadata.value("message").toString(); // message is optional

		// Ignore messages older than 60s after last update
		auto time = metadata.value("time").toString();
		QDateTime messageTime = QDateTime::fromString(time, Qt::ISODate);
		qint64 deltaTime = messageTime.secsTo(m_timestamp.toUTC());

		// printf("Message time is: %s\n", qUtf8Printable(messageTime.toLocalTime().toString(Qt::ISODate)));
		// printf("Timestamp time is: %s\n", qUtf8Printable(m_timestamp.toString(Qt::ISODate)));
		// printf("Difference between times: %lld\n", static_cast<long long>(deltaTime));

		if (deltaTime > 60)
			return;
	}

	m_count = newValue;
	updateDisplay();
	writeToFile();

	addLogEntry(sender, note, m_count);
	saveSettings();
}

void CounterDock::sendCounterUpdate()
{
	if (!m_webSocket || m_webSocket->state() != QAbstractSocket::ConnectedState)
		return;

	if (m_token.isEmpty())
		return;

	QJsonObject metadata;
	metadata["sender"] = QStringLiteral("OBS");

	QJsonObject obj;
	obj["type"] = QStringLiteral("counter");
	obj["token"] = m_token;
	obj["value"] = m_count;
	obj["metadata"] = metadata;

	m_webSocket->sendTextMessage(QString::fromUtf8(QJsonDocument(obj).toJson(QJsonDocument::Compact)));
}

void CounterDock::updateWsStatusLabel(bool connected)
{
	if (!m_wsIndicatorLabel)
		return;

	if (m_wsUrl.trimmed().isEmpty()) {
		m_wsIndicatorLabel->setStyleSheet("background-color: #7F8C8D; border-radius: 6px;"); // sem endereço
		m_wsIndicatorLabel->setToolTip(obs_module_text("WebSocketNoAddr"));
		return;
	}

	if (connected) {
		m_wsIndicatorLabel->setStyleSheet("background-color: #2ECC71; border-radius: 6px;"); // conectado
		m_wsIndicatorLabel->setToolTip(obs_module_text("WebSocketConnected"));
	} else {
		m_wsIndicatorLabel->setStyleSheet("background-color: #CC2D2D; border-radius: 6px;"); // desconectado
		m_wsIndicatorLabel->setToolTip(obs_module_text("WebSocketDisconnected"));
	}
}
