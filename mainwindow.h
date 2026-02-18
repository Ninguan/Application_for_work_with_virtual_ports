#pragma once

#include <QMainWindow>
#include <QByteArray>
#include <QTimer>

class QSerialPort;
class QComboBox;
class QPlainTextEdit;
class QLineEdit;
class QPushButton;
class QCheckBox;
class QDoubleSpinBox;
class QSpinBox;

namespace QtCharts {
class QChart;
class QChartView;
class QLineSeries;
class QValueAxis;
}

QT_BEGIN_NAMESPACE
namespace Ui { class MainWindow; }
QT_END_NAMESPACE

class MainWindow : public QMainWindow
{
    Q_OBJECT
public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

private slots:
    void refreshPorts();
    void connectPort();
    void disconnectPort();
    void sendManual();
    void onReadyRead();

    void startGenerator();
    void stopGenerator();
    void onGenTick();

private:
    void buildUi();
    void wireUi();
    void logLine(const QString& s);

    void parseLine(const QString& line, bool fromTxLocalEcho);
    bool tryParseOut(const QString& line, QStringList& outSamples);
    bool tryParseCsv(const QString& line, QStringList& outSamples);

    void setupChart();
    void pushSampleToChart(double y);

private:
    Ui::MainWindow* ui = nullptr;

    QSerialPort* m_serial = nullptr;
    QByteArray m_rxBuffer;

    QTimer m_genTimer;
    double m_timeSec = 0.0;

    QPushButton* m_btnRefreshPorts = nullptr;
    QComboBox*   m_cmbPorts = nullptr;
    QComboBox*   m_cmbBaud = nullptr;
    QPushButton* m_btnConnect = nullptr;
    QPushButton* m_btnDisconnect = nullptr;

    QPlainTextEdit* m_txtLog = nullptr;
    QLineEdit* m_edtSend = nullptr;
    QPushButton* m_btnSend = nullptr;
    QCheckBox* m_chkLocalEcho = nullptr;
    QDoubleSpinBox* m_spAmp = nullptr;
    QDoubleSpinBox* m_spFreq = nullptr;
    QDoubleSpinBox* m_spDc = nullptr;
    QComboBox* m_cmbWave = nullptr;
    QSpinBox* m_spSampleRate = nullptr;
    QSpinBox* m_spPacketSamples = nullptr;
    QCheckBox* m_chkSendSamples = nullptr;
    QPushButton* m_btnGenStart = nullptr;
    QPushButton* m_btnGenStop = nullptr;

    QPlainTextEdit* m_txtParsed = nullptr;

    QtCharts::QChart* m_chart = nullptr;
    QtCharts::QChartView* m_chartView = nullptr;
    QtCharts::QLineSeries* m_series = nullptr;
    QtCharts::QValueAxis* m_axisX = nullptr;
    QtCharts::QValueAxis* m_axisY = nullptr;

    qint64 m_sampleIndex = 0;
    int m_maxPoints = 600;
    double m_yMin = -10;
    double m_yMax = 10;
};
