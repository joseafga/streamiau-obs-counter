#include <QVBoxLayout>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QDialogButtonBox>
#include <QFileDialog>
#include <QDir>
#include "counter-settings-dialog.hpp"

CounterSettingsDialog::CounterSettingsDialog(const QString &currentOutputPath, const QString &currentWsUrl,
					     const QString &currentToken, QWidget *parent)
	: QDialog(parent),
	  m_outputPath(currentOutputPath),
	  m_wsUrl(currentWsUrl),
	  m_token(currentToken)
{
	buildUi();
}

void CounterSettingsDialog::buildUi()
{
	setWindowTitle(tr("Configurações do Contador"));
	setMinimumWidth(420);

	auto *mainLayout = new QVBoxLayout(this);
	mainLayout->setSpacing(8);

	auto *form = new QFormLayout();

	auto *pathRow = new QHBoxLayout();
	m_pathEdit = new QLineEdit(this);
	m_pathEdit->setReadOnly(true);
	m_pathEdit->setText(m_outputPath);
	m_pathEdit->setPlaceholderText(tr("Selecione o arquivo .txt para a fonte de Texto (GDI+)..."));

	m_browseBtn = new QPushButton(tr("Procurar..."), this);

	pathRow->addWidget(m_pathEdit, 1);
	pathRow->addWidget(m_browseBtn);
	form->addRow(tr("Arquivo de saída:"), pathRow);

	m_wsUrlEdit = new QLineEdit(this);
	m_wsUrlEdit->setText(m_wsUrl);
	m_wsUrlEdit->setPlaceholderText("ws://host:port/path");
	form->addRow("WebSocket:", m_wsUrlEdit);

	m_tokenEdit = new QLineEdit(this);
	m_tokenEdit->setText(m_token);
	form->addRow("Token:", m_tokenEdit);

	m_tokenDesc = new QLabel(this);
	m_tokenDesc->setStyleSheet("color: gray; font-style: italic;");
	m_tokenDesc->setText(tr("Não compartilhe este token. Ele dá acesso para alterar o contador."));
	form->addRow(m_tokenDesc);

	mainLayout->addLayout(form);
	mainLayout->addStretch(1);

	auto *buttonBox = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
	mainLayout->addWidget(buttonBox);

	connect(m_browseBtn, &QPushButton::clicked, this, &CounterSettingsDialog::onBrowse);
	connect(buttonBox, &QDialogButtonBox::accepted, this, &CounterSettingsDialog::onAccept);
	connect(buttonBox, &QDialogButtonBox::rejected, this, &QDialog::reject);
}

void CounterSettingsDialog::onBrowse()
{
	QString start = m_pathEdit->text().isEmpty() ? QDir::homePath() : m_pathEdit->text();
	QString path = QFileDialog::getSaveFileName(this, tr("Selecionar arquivo de texto"), start,
						    tr("Arquivo de texto (*.txt)"));

	if (path.isEmpty())
		return;

	if (!path.endsWith(".txt", Qt::CaseInsensitive))
		path += ".txt";

	m_pathEdit->setText(path);
}

void CounterSettingsDialog::onAccept()
{
	m_outputPath = m_pathEdit->text();
	m_wsUrl = m_wsUrlEdit->text().trimmed();
	m_token = m_tokenEdit->text().trimmed();
	accept();
}
