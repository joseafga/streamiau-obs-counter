#include "counter-settings-dialog.hpp"

#include <QVBoxLayout>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QDialogButtonBox>
#include <QFileDialog>
#include <QDir>

CounterSettingsDialog::CounterSettingsDialog(const QString &currentOutputPath, QWidget *parent)
	: QDialog(parent),
	  m_outputPath(currentOutputPath)
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
	m_pathEdit->setPlaceholderText(
		tr("Selecione o arquivo .txt para a fonte de Texto (GDI+)..."));

	m_browseBtn = new QPushButton(tr("Procurar..."), this);

	pathRow->addWidget(m_pathEdit, 1);
	pathRow->addWidget(m_browseBtn);
	form->addRow(tr("Arquivo de saída:"), pathRow);

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
	QString path = QFileDialog::getSaveFileName(
		this, tr("Selecionar arquivo de texto"), start,
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
	accept();
}
