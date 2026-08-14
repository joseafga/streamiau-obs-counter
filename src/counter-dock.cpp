#include <obs-module.h>
#include <obs-frontend-api.h>
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
#include <QMetaObject>
#include <QTimer>
#include <ixwebsocket/IXWebSocket.h>
#include <ixwebsocket/IXWebSocketMessageType.h>
#include "counter-dock.hpp"
#include "counter-settings-dialog.hpp"

namespace {
constexpr int kMaxLogEntries = 50;
constexpr int kDebounceDelay = 500;
} // namespace

CounterDock::CounterDock(QWidget *parent) : QWidget(parent)
{
	buildUi();
	registerHotkeys();
	obs_frontend_add_save_callback(frontendSaveCallback, this);

	loadSettings();
	updateDisplay();

	writeToFile();
	setupWebSocket();
}

CounterDock::~CounterDock()
{
	obs_frontend_remove_save_callback(frontendSaveCallback, this);
	unregisterHotkeys();

	if (m_webSocket) {
		m_webSocket->setOnMessageCallback(nullptr);
		m_webSocket->stop();
		m_webSocket.reset();
	}
}

void CounterDock::buildUi()
{
	// fix background to OBS theme
	this->setAttribute(Qt::WA_StyledBackground, true);

	auto *mainLayout = new QVBoxLayout(this);
	mainLayout->setContentsMargins(2, 2, 2, 2);
	mainLayout->setSpacing(4);

	m_counterLabel = new QLabel(QStringLiteral("0"), this);
	m_counterLabel->setStyleSheet("font-family: sans-serif; \
		font-size: 48px; \
		font-weight: bold; \
		background-color: rgba(0, 0, 0, 0.25); \
		border-radius: 8px;");
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

	m_resetBtn = new QPushButton(obs_module_text("StreamiauCounter.Reset"), this);

	m_settingsBtn = new QPushButton(QStringLiteral("\u2699"), this);
	m_settingsBtn->setToolTip(obs_module_text("StreamiauCounter.Settings"));
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
	m_logList->setStyleSheet("background-color: rgba(0, 0, 0, 0.25); border-radius: 8px; \
		QListWidget::item { padding: 0px; margin: 0px; }");
	mainLayout->addWidget(m_logList, 1);

	// Status bar
	m_statusLabel = new QLabel(this);
	m_statusLabel->setStyleSheet("color: gray; font-style: italic;");

	m_wsIndicatorLabel = new QLabel(this);
	m_wsIndicatorLabel->setFixedSize(12, 12);
	m_wsIndicatorLabel->setToolTip(obs_module_text("StreamiauCounter.WebSocketStatus"));
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

	// Prevents multiple requests, send new counter value after `kDebounceDelay` only
	m_sendTimer = new QTimer(this);
	m_sendTimer->setSingleShot(true);
	connect(m_sendTimer, &QTimer::timeout, this, &CounterDock::sendCounterUpdate);
}

void CounterDock::showEvent(QShowEvent *event)
{
	QWidget::showEvent(event);
	m_webSocket->start();
}

void CounterDock::hideEvent(QHideEvent *event)
{
	QWidget::hideEvent(event);
	m_webSocket->stop();
}

void CounterDock::onIncrement()
{
	m_count += 1;
	updateDisplay();
	writeToFile();
	addLogEntry(QStringLiteral("OBS"), QStringLiteral(""), m_count);
	m_sendTimer->start(kDebounceDelay);
	saveSettings();
}

void CounterDock::onDecrement()
{
	m_count -= 1;
	updateDisplay();
	writeToFile();
	addLogEntry(QStringLiteral("OBS"), QStringLiteral(""), m_count);
	m_sendTimer->start(kDebounceDelay);
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
	QMetaObject::invokeMethod(m_sendTimer, "start", Qt::QueuedConnection);
	m_sendTimer->start(kDebounceDelay);
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
		m_statusLabel->setText(obs_module_text("StreamiauCounter.SettingsSaved"));

		if (wsUrlChanged) {
			setupWebSocket();
			m_webSocket->start();
		}
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
			m_statusLabel->setText(obs_module_text("StreamiauCounter.SettingsNoFile"));
		return;
	}

	QFileInfo info(m_outputPath);
	QDir().mkpath(info.absolutePath());

	QFile file(m_outputPath);
	if (!file.open(QIODevice::WriteOnly | QIODevice::Text | QIODevice::Truncate)) {
		if (m_statusLabel)
			m_statusLabel->setText(obs_module_text("StreamiauCounter.FileWriteError"));
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

void CounterDock::setupWebSocket()
{
	// Tear down any previous connection first.
	if (m_webSocket) {
		m_webSocket->setOnMessageCallback(nullptr);
		m_webSocket->stop();
		m_webSocket.reset();
	}

	if (m_wsUrl.trimmed().isEmpty()) {
		updateWsStatusLabel(false);
		return;
	}

	m_webSocket = std::make_unique<ix::WebSocket>();
	m_webSocket->setUrl(m_wsUrl.trimmed().toStdString());

	m_webSocket->setOnMessageCallback([this](const ix::WebSocketMessagePtr &msg) {
		switch (msg->type) {
		case ix::WebSocketMessageType::Open:
			QMetaObject::invokeMethod(this, [this]() { onWebSocketConnected(); }, Qt::QueuedConnection);
			break;
		case ix::WebSocketMessageType::Close:
			QMetaObject::invokeMethod(this, [this]() { onWebSocketDisconnected(); }, Qt::QueuedConnection);
			break;
		case ix::WebSocketMessageType::Error: {
			QString reason = QString::fromStdString(msg->errorInfo.reason);
			QMetaObject::invokeMethod(
				this, [this, reason]() { onWebSocketErrorOccurred(reason); }, Qt::QueuedConnection);
			break;
		}
		case ix::WebSocketMessageType::Message: {
			QString text = QString::fromStdString(msg->str);
			QMetaObject::invokeMethod(
				this, [this, text]() { onWebSocketTextMessageReceived(text); }, Qt::QueuedConnection);
			break;
		}
		default:
			break;
		}
	});
}

void CounterDock::onWebSocketConnected()
{
	updateWsStatusLabel(true);
}

void CounterDock::onWebSocketDisconnected()
{
	updateWsStatusLabel(false);
}

void CounterDock::onWebSocketErrorOccurred(const QString &reason)
{
	Q_UNUSED(reason)
	updateWsStatusLabel(false);
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
	if (!m_webSocket || m_webSocket->getReadyState() != ix::ReadyState::Open)
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

	QByteArray json = QJsonDocument(obj).toJson(QJsonDocument::Compact);
	m_webSocket->send(std::string(json.constData(), static_cast<size_t>(json.size())));
}

void CounterDock::updateWsStatusLabel(bool connected)
{
	if (!m_wsIndicatorLabel)
		return;

	if (m_wsUrl.trimmed().isEmpty()) {
		m_wsIndicatorLabel->setStyleSheet("background-color: #7F8C8D; border-radius: 6px;"); // no adress
		m_wsIndicatorLabel->setToolTip(obs_module_text("StreamiauCounter.WebSocketNoAddr"));
		return;
	}

	if (connected) {
		m_wsIndicatorLabel->setStyleSheet("background-color: #2ECC71; border-radius: 6px;"); // conected
		m_wsIndicatorLabel->setToolTip(obs_module_text("StreamiauCounter.WebSocketConnected"));
	} else {
		m_wsIndicatorLabel->setStyleSheet("background-color: #CC2D2D; border-radius: 6px;"); // disconnected
		m_wsIndicatorLabel->setToolTip(obs_module_text("StreamiauCounter.WebSocketDisconnected"));
	}
}

void CounterDock::registerHotkeys()
{
	m_hotkeyIncrement = obs_hotkey_register_frontend("streamiau_counter_hotkey_increment",
							 obs_module_text("StreamiauCounter.Hotkey.Increment"),
							 hotkeyIncrementCallback, this);

	m_hotkeyDecrement = obs_hotkey_register_frontend("streamiau_counter_hotkey_decrement",
							 obs_module_text("StreamiauCounter.Hotkey.Decrement"),
							 hotkeyDecrementCallback, this);

	m_hotkeyReset = obs_hotkey_register_frontend("streamiau_counter_hotkey_reset",
						     obs_module_text("StreamiauCounter.Hotkey.Reset"),
						     hotkeyResetCallback, this);
}

void CounterDock::unregisterHotkeys()
{
	obs_hotkey_unregister(m_hotkeyIncrement);
	obs_hotkey_unregister(m_hotkeyDecrement);
	obs_hotkey_unregister(m_hotkeyReset);

	m_hotkeyIncrement = OBS_INVALID_HOTKEY_ID;
	m_hotkeyDecrement = OBS_INVALID_HOTKEY_ID;
	m_hotkeyReset = OBS_INVALID_HOTKEY_ID;
}

void CounterDock::hotkeyIncrementCallback(void *data, obs_hotkey_id id, obs_hotkey_t *hotkey, bool pressed)
{
	UNUSED_PARAMETER(id);
	UNUSED_PARAMETER(hotkey);

	if (!pressed)
		return;

	auto *dock = static_cast<CounterDock *>(data);
	QMetaObject::invokeMethod(dock, "onIncrement", Qt::QueuedConnection);
}

void CounterDock::hotkeyDecrementCallback(void *data, obs_hotkey_id id, obs_hotkey_t *hotkey, bool pressed)
{
	UNUSED_PARAMETER(id);
	UNUSED_PARAMETER(hotkey);

	if (!pressed)
		return;

	auto *dock = static_cast<CounterDock *>(data);
	QMetaObject::invokeMethod(dock, "onDecrement", Qt::QueuedConnection);
}

void CounterDock::hotkeyResetCallback(void *data, obs_hotkey_id id, obs_hotkey_t *hotkey, bool pressed)
{
	UNUSED_PARAMETER(id);
	UNUSED_PARAMETER(hotkey);

	if (!pressed)
		return;

	auto *dock = static_cast<CounterDock *>(data);
	QMetaObject::invokeMethod(dock, "onReset", Qt::QueuedConnection);
}

void CounterDock::frontendSaveCallback(obs_data_t *saveData, bool saving, void *data)
{
	static_cast<CounterDock *>(data)->onFrontendSave(saveData, saving);
}

void CounterDock::onFrontendSave(obs_data_t *saveData, bool saving)
{
	static const char *kIncrementKey = "streamiau_counter_hotkey_increment";
	static const char *kDecrementKey = "streamiau_counter_hotkey_decrement";
	static const char *kResetKey = "streamiau_counter_hotkey_reset";

	if (saving) {
		obs_data_array_t *incArray = obs_hotkey_save(m_hotkeyIncrement);
		obs_data_array_t *decArray = obs_hotkey_save(m_hotkeyDecrement);
		obs_data_array_t *resetArray = obs_hotkey_save(m_hotkeyReset);

		obs_data_set_array(saveData, kIncrementKey, incArray);
		obs_data_set_array(saveData, kDecrementKey, decArray);
		obs_data_set_array(saveData, kResetKey, resetArray);

		obs_data_array_release(incArray);
		obs_data_array_release(decArray);
		obs_data_array_release(resetArray);
	} else {
		obs_data_array_t *incArray = obs_data_get_array(saveData, kIncrementKey);
		obs_data_array_t *decArray = obs_data_get_array(saveData, kDecrementKey);
		obs_data_array_t *resetArray = obs_data_get_array(saveData, kResetKey);

		obs_hotkey_load(m_hotkeyIncrement, incArray);
		obs_hotkey_load(m_hotkeyDecrement, decArray);
		obs_hotkey_load(m_hotkeyReset, resetArray);

		obs_data_array_release(incArray);
		obs_data_array_release(decArray);
		obs_data_array_release(resetArray);
	}
}
