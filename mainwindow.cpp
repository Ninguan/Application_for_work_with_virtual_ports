#include "mainwindow.h"
#include "ui_mainwindow.h"

#include <QSerialPort>
#include <QSerialPortInfo>

#include <QDateTime>
#include <QMessageBox>

#include <QComboBox>
#include <QPlainTextEdit>
#include <QLineEdit>
#include <QPushButton>
#include <QCheckBox>
#include <QDoubleSpinBox>
#include <QSpinBox>

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QGroupBox>
#include <QLabel>

#include <QtMath>

#include <QtCharts/QChartView>
#include <QtCharts/QChart>
#include <QtCharts/QLineSeries>
#include <QtCharts/QValueAxis>

using namespace QtCharts;

static QString nowStamp()
{
    return QDateTime::currentDateTime().toString("HH:mm:ss.zzz");
}

static double waveValue(const QString& type, double phaseRad)
{
    const double twoPi = 2.0 * M_PI;
    double x = std::fmod(phaseRad, twoPi);
    if (x < 0) x += twoPi;

    if (type == "sin") return qSin(phaseRad);
    if (type == "square") return (qSin(phaseRad) >= 0.0) ? 1.0 : -1.0;

    if (type == "triangle") {
        double t = x / twoPi; // 0..1
        if (t < 0.25) return 4*t;
        if (t < 0.75) return 2 - 4*t;
        return -4 + 4*t;
    }

    double t = x / twoPi;
    return 2.0*t - 1.0;
}

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
{
    ui->setupUi(this);

    buildUi();
    setupChart();
    wireUi();

    m_serial = new QSerialPort(this);
    connect(m_serial, &QSerialPort::readyRead, this, &MainWindow::onReadyRead);

    connect(&m_genTimer, &QTimer::timeout, this, &MainWindow::onGenTick);

    refreshPorts();
    logLine("Gotowe. Bridge COM1<->COM2.");
}

MainWindow::~MainWindow()
{
    stopGenerator();
    if (m_serial && m_serial->isOpen())
        m_serial->close();
    delete ui;
}

void MainWindow::buildUi()
{
    auto *root = new QWidget(this);
    setCentralWidget(root);

    auto *mainLay = new QHBoxLayout(root);

    auto *gbSerial = new QGroupBox("Port szeregowy", root);
    auto *serialLay = new QVBoxLayout(gbSerial);

    auto *rowPorts = new QHBoxLayout();
    m_btnRefreshPorts = new QPushButton("Dostępne porty", gbSerial);
    m_cmbPorts = new QComboBox(gbSerial);
    rowPorts->addWidget(m_btnRefreshPorts);
    rowPorts->addWidget(m_cmbPorts, 1);
    serialLay->addLayout(rowPorts);

    auto *rowBaud = new QHBoxLayout();
    rowBaud->addWidget(new QLabel("Baud:", gbSerial));
    m_cmbBaud = new QComboBox(gbSerial);
    m_cmbBaud->addItems({"9600","19200","38400","57600","115200"});
    m_cmbBaud->setCurrentText("115200");
    rowBaud->addWidget(m_cmbBaud, 1);
    serialLay->addLayout(rowBaud);

    auto *rowConn = new QHBoxLayout();
    m_btnConnect = new QPushButton("Połącz", gbSerial);
    m_btnDisconnect = new QPushButton("Rozłącz", gbSerial);
    m_btnDisconnect->setEnabled(false);
    rowConn->addWidget(m_btnConnect);
    rowConn->addWidget(m_btnDisconnect);
    serialLay->addLayout(rowConn);

    m_txtLog = new QPlainTextEdit(gbSerial);
    m_txtLog->setReadOnly(true);
    serialLay->addWidget(m_txtLog, 1);

    auto *rowSend = new QHBoxLayout();
    m_edtSend = new QLineEdit(gbSerial);
    m_edtSend->setPlaceholderText("Np. OUT=1,100,100,100  albo  1,100,100,100");
    m_btnSend = new QPushButton("Wyślij", gbSerial);
    rowSend->addWidget(m_edtSend, 1);
    rowSend->addWidget(m_btnSend);
    serialLay->addLayout(rowSend);

    m_chkLocalEcho = new QCheckBox("Local echo", gbSerial);
    m_chkLocalEcho->setChecked(true);
    serialLay->addWidget(m_chkLocalEcho);

    auto *rightCol = new QWidget(root);
    auto *rightLay = new QVBoxLayout(rightCol);

    auto *gbGen = new QGroupBox("Zadajniki / Generator", rightCol);
    auto *grid = new QGridLayout(gbGen);

    grid->addWidget(new QLabel("Amp:"), 0, 0);
    m_spAmp = new QDoubleSpinBox(gbGen);
    m_spAmp->setRange(0, 1e9);
    m_spAmp->setDecimals(4);
    m_spAmp->setValue(1.0);
    grid->addWidget(m_spAmp, 0, 1);

    grid->addWidget(new QLabel("Freq [Hz]:"), 0, 2);
    m_spFreq = new QDoubleSpinBox(gbGen);
    m_spFreq->setRange(0, 1e6);
    m_spFreq->setDecimals(4);
    m_spFreq->setValue(1.0);
    grid->addWidget(m_spFreq, 0, 3);

    grid->addWidget(new QLabel("DC:"), 0, 4);
    m_spDc = new QDoubleSpinBox(gbGen);
    m_spDc->setRange(-1e9, 1e9);
    m_spDc->setDecimals(4);
    m_spDc->setValue(0.0);
    grid->addWidget(m_spDc, 0, 5);

    grid->addWidget(new QLabel("Wave:"), 1, 0);
    m_cmbWave = new QComboBox(gbGen);
    m_cmbWave->addItems({"sin","square","triangle","saw"});
    grid->addWidget(m_cmbWave, 1, 1);

    grid->addWidget(new QLabel("SampleRate [Hz]:"), 1, 2);
    m_spSampleRate = new QSpinBox(gbGen);
    m_spSampleRate->setRange(1, 5000);
    m_spSampleRate->setValue(50);
    grid->addWidget(m_spSampleRate, 1, 3);

    grid->addWidget(new QLabel("Packet samples:"), 1, 4);
    m_spPacketSamples = new QSpinBox(gbGen);
    m_spPacketSamples->setRange(1, 1000);
    m_spPacketSamples->setValue(10);
    grid->addWidget(m_spPacketSamples, 1, 5);

    m_chkSendSamples = new QCheckBox("Wysyłaj próbki (OUT=1,s1,s2,...)", gbGen);
    m_chkSendSamples->setChecked(true);
    grid->addWidget(m_chkSendSamples, 2, 0, 1, 6);

    m_btnGenStart = new QPushButton("Start", gbGen);
    m_btnGenStop  = new QPushButton("Stop", gbGen);
    grid->addWidget(m_btnGenStart, 3, 0, 1, 3);
    grid->addWidget(m_btnGenStop,  3, 3, 1, 3);

    rightLay->addWidget(gbGen);

    auto *gbChart = new QGroupBox("Wykres", rightCol);
    auto *chartLay = new QVBoxLayout(gbChart);
    rightLay->addWidget(gbChart, 2);

    auto *gbParsed = new QGroupBox("Odebrane liczby", rightCol);
    auto *parsedLay = new QVBoxLayout(gbParsed);
    m_txtParsed = new QPlainTextEdit(gbParsed);
    m_txtParsed->setReadOnly(true);
    parsedLay->addWidget(m_txtParsed);
    rightLay->addWidget(gbParsed, 1);

    mainLay->addWidget(gbSerial, 2);
    mainLay->addWidget(rightCol, 3);
    gbChart->setProperty("chartLayPtr", QVariant::fromValue<void*>(chartLay));
}

void MainWindow::setupChart()
{
    m_series = new QLineSeries();

    m_chart = new QChart();
    m_chart->addSeries(m_series);
    m_chart->legend()->hide();
    m_chart->setTitle("Próbki");

    m_axisX = new QValueAxis();
    m_axisY = new QValueAxis();
    m_axisX->setTitleText("Sample #");
    m_axisY->setTitleText("Value");
    m_axisX->setRange(0, m_maxPoints);
    m_axisY->setRange(m_yMin, m_yMax);

    m_chart->addAxis(m_axisX, Qt::AlignBottom);
    m_chart->addAxis(m_axisY, Qt::AlignLeft);

    m_series->attachAxis(m_axisX);
    m_series->attachAxis(m_axisY);

    m_chartView = new QChartView(m_chart);
    m_chartView->setRenderHint(QPainter::Antialiasing);

    auto gbChartList = this->findChildren<QGroupBox*>();
    for (auto* gb : gbChartList) {
        if (gb->title().startsWith("Wykres")) {
            auto ptrVar = gb->property("chartLayPtr");
            auto *chartLay = static_cast<QVBoxLayout*>(ptrVar.value<void*>());
            if (chartLay) chartLay->addWidget(m_chartView);
            break;
        }
    }
}

void MainWindow::wireUi()
{
    connect(m_btnRefreshPorts, &QPushButton::clicked, this, &MainWindow::refreshPorts);
    connect(m_btnConnect, &QPushButton::clicked, this, &MainWindow::connectPort);
    connect(m_btnDisconnect, &QPushButton::clicked, this, &MainWindow::disconnectPort);

    connect(m_btnSend, &QPushButton::clicked, this, &MainWindow::sendManual);
    connect(m_edtSend, &QLineEdit::returnPressed, this, &MainWindow::sendManual);

    connect(m_btnGenStart, &QPushButton::clicked, this, &MainWindow::startGenerator);
    connect(m_btnGenStop, &QPushButton::clicked, this, &MainWindow::stopGenerator);
}

void MainWindow::logLine(const QString &s)
{
    m_txtLog->appendPlainText(QString("[%1] %2").arg(nowStamp(), s));
}

void MainWindow::refreshPorts()
{
    m_cmbPorts->clear();
    const auto ports = QSerialPortInfo::availablePorts();
    for (const auto& p : ports)
        m_cmbPorts->addItem(QString("%1 (%2)").arg(p.portName(), p.description()), p.portName());

    logLine(QString("Znaleziono portów: %1").arg(ports.size()));
}

void MainWindow::connectPort()
{
    if (m_serial->isOpen()) {
        logLine("Port już otwarty.");
        return;
    }

    const QString portName = m_cmbPorts->currentData().toString();
    if (portName.isEmpty()) {
        QMessageBox::warning(this, "Błąd", "Nie wybrano portu.");
        return;
    }

    m_serial->setPortName(portName);
    m_serial->setBaudRate(m_cmbBaud->currentText().toInt());
    m_serial->setDataBits(QSerialPort::Data8);
    m_serial->setParity(QSerialPort::NoParity);
    m_serial->setStopBits(QSerialPort::OneStop);
    m_serial->setFlowControl(QSerialPort::NoFlowControl);

    if (!m_serial->open(QIODevice::ReadWrite)) {
        QMessageBox::critical(this, "Błąd", "Nie udało się otworzyć portu: " + m_serial->errorString());
        return;
    }

    m_btnConnect->setEnabled(false);
    m_btnDisconnect->setEnabled(true);
    logLine("Połączono z: " + portName);
}

void MainWindow::disconnectPort()
{
    if (!m_serial->isOpen())
        return;

    stopGenerator();

    m_serial->close();
    m_btnConnect->setEnabled(true);
    m_btnDisconnect->setEnabled(false);
    logLine("Rozłączono.");
}

void MainWindow::sendManual()
{
    if (!m_serial->isOpen()) {
        QMessageBox::warning(this, "Błąd", "Port nie jest otwarty.");
        return;
    }

    QString msg = m_edtSend->text().trimmed();
    if (msg.isEmpty()) return;

    QString toSend = msg;
    if (!toSend.endsWith('\n')) toSend += "\n";

    m_serial->write(toSend.toUtf8());
    logLine("TX: " + msg);

    if (m_chkLocalEcho->isChecked())
        parseLine(msg, true);
}

void MainWindow::onReadyRead()
{
    m_rxBuffer += m_serial->readAll();

    while (true) {
        int idx = m_rxBuffer.indexOf('\n');
        if (idx < 0) break;

        const QByteArray lineBytes = m_rxBuffer.left(idx);
        m_rxBuffer.remove(0, idx + 1);

        const QString line = QString::fromUtf8(lineBytes).trimmed();
        if (!line.isEmpty()) {
            logLine("RX: " + line);
            parseLine(line, false);
        }
    }
}
bool MainWindow::tryParseOut(const QString& line, QStringList& outSamples)
{
    if (!line.startsWith("OUT=", Qt::CaseInsensitive))
        return false;

    const QString payload = line.mid(4);
    const QStringList parts = payload.split(',', Qt::SkipEmptyParts);
    if (parts.isEmpty())
        return true;

    if (parts.size() >= 2) {
        for (int i = 1; i < parts.size(); ++i) {
            bool ok=false; double v = parts[i].toDouble(&ok);
            if (ok) outSamples << QString::number(v);
        }
    } else {
        bool ok=false; double v = parts[0].toDouble(&ok);
        if (ok) outSamples << QString::number(v);
    }
    return true;
}

bool MainWindow::tryParseCsv(const QString& line, QStringList& outSamples)
{
    if (!line.contains(',')) return false;
    if (line.startsWith("PARAM=", Qt::CaseInsensitive)) return false;

    const QStringList parts = line.split(',', Qt::SkipEmptyParts);
    if (parts.isEmpty()) return false;

    if (parts.size() >= 2) {
        for (int i = 1; i < parts.size(); ++i) {
            bool ok=false; double v = parts[i].toDouble(&ok);
            if (!ok) return false;
            outSamples << QString::number(v);
        }
        return true;
    }

    bool ok=false; double v = parts[0].toDouble(&ok);
    if (ok) outSamples << QString::number(v);
    return ok;
}

void MainWindow::parseLine(const QString &line, bool fromTxLocalEcho)
{
    QStringList samples;

    if (tryParseOut(line, samples)) {
        if (!samples.isEmpty()) {
            m_txtParsed->appendPlainText(
                QString("%1OUT samples: %2").arg(fromTxLocalEcho ? "[echo] " : "").arg(samples.join(", ")));
            for (const QString& s : samples) {
                bool ok=false; double v=s.toDouble(&ok);
                if (ok) pushSampleToChart(v);
            }
        } else {
            m_txtParsed->appendPlainText(QString("%1OUT (bez liczb): %2").arg(fromTxLocalEcho ? "[echo] " : "") .arg(line));
        }
        return;
    }

    if (tryParseCsv(line, samples)) {
        m_txtParsed->appendPlainText(
            QString("%1CSV samples: %2").arg(fromTxLocalEcho ? "[echo] " : "").arg(samples.join(", "))
            );
        for (const QString& s : samples) {
            bool ok=false; double v=s.toDouble(&ok);
            if (ok) pushSampleToChart(v);
        }
        return;
    }

    bool ok=false;
    double v = line.toDouble(&ok);
    if (ok) {
        m_txtParsed->appendPlainText(
            QString("%1Sample: %2").arg(fromTxLocalEcho ? "[echo] " : "").arg(QString::number(v)));
        pushSampleToChart(v);
    }
}

void MainWindow::pushSampleToChart(double y)
{
    if (y < m_yMin) m_yMin = y - 1;
    if (y > m_yMax) m_yMax = y + 1;

    m_series->append(m_sampleIndex, y);
    m_sampleIndex++;

    if (m_series->count() > m_maxPoints) {
        int removeCount = m_series->count() - m_maxPoints;
        m_series->removePoints(0, removeCount);
    }

    const qint64 xEnd = m_sampleIndex;
    const qint64 xStart = qMax<qint64>(0, xEnd - m_maxPoints);

    m_axisX->setRange(xStart, xEnd);
    m_axisY->setRange(m_yMin, m_yMax);
}

void MainWindow::startGenerator()
{
    if (!m_serial->isOpen()) {
        QMessageBox::warning(this, "Błąd", "Najpierw połącz z portem.");
        return;
    }

    const int fs = m_spSampleRate->value();
    if (fs <= 0) return;

    m_timeSec = 0.0;

    const int intervalMs = qMax(1, int(1000.0 / fs));
    m_genTimer.start(intervalMs);

    logLine(QString("Generator START (fs=%1 Hz)").arg(fs));
}

void MainWindow::stopGenerator()
{
    if (m_genTimer.isActive()) {
        m_genTimer.stop();
        logLine("Generator STOP");
    }
}

void MainWindow::onGenTick()
{
    if (!m_serial->isOpen()) return;

    const double amp = m_spAmp->value();
    const double freq = m_spFreq->value();
    const double dc  = m_spDc->value();
    const QString wave = m_cmbWave->currentText();

    const int fs = m_spSampleRate->value();
    const int packetN = m_spPacketSamples->value();
    const bool sendSamples = m_chkSendSamples->isChecked();

    QStringList samples;
    samples.reserve(packetN);

    for (int i = 0; i < packetN; ++i) {
        const double phase = 2.0 * M_PI * freq * m_timeSec;
        const double w = waveValue(wave, phase);
        const double y = dc + amp * w;

        samples << QString::number(y, 'f', 4);
        m_timeSec += 1.0 / double(fs);
    }

    QString msg;
    if (sendSamples) {
        msg = "OUT=1," + samples.join(',');
    } else {
        msg = QString("PARAM=%1,%2,%3,%4")
        .arg(amp, 0, 'f', 4)
            .arg(freq,0, 'f', 4)
            .arg(dc,  0, 'f', 4)
            .arg(wave);
    }

    m_serial->write((msg + "\n").toUtf8());
    logLine("TX: " + msg);

    if (m_chkLocalEcho->isChecked())
        parseLine(msg, true);
}
