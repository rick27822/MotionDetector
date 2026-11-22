#pragma once
#include "app/AppState.h"
#include "contracts/IAnalysisProvider.h"
#include <QElapsedTimer>
#include <QTimer>
#include <memory>
namespace ui {
class SettingsController : public QObject {
    Q_OBJECT
  public:
    SettingsController(AppState *, QString directory, QObject *parent = nullptr);
    bool apply(const UiConfig &config);
    QString error() const {
        return m_error;
    }
    QString configFile() const;
    void setFailSave(bool fail) {
        m_fail = fail;
    }

  private:
    AppState *m_state;
    QString m_directory, m_error;
    bool m_fail = false;
  signals:
    void feedback(const QString &, bool success);
};
class DeviceController : public QObject {
    Q_OBJECT
  public:
    explicit DeviceController(AppState *, QObject *parent = nullptr);
    void connectDevice(const QString &id);
    void disconnectDevice();
    void refresh();
    void setFailConnection(bool b) {
        m_fail = b;
    }
    bool busy() const {
        return m_busy;
    }

  private:
    AppState *m_state;
    bool m_busy = false, m_fail = false;
    int m_generation = 0;
  signals:
    void busyChanged(bool);
    void feedback(const QString &, bool);
};
class DetectionController : public QObject {
    Q_OBJECT
  public:
    explicit DetectionController(AppState *, QObject *parent = nullptr);
    void toggleDetection();
    void toggleRecording();
    void stop();
    qint64 recordingMs() const;
    void setFailRecording(bool fail) {
        m_fail = fail;
    }

  private:
    AppState *m_state;
    QElapsedTimer m_clock;
    QTimer m_tick;
    bool m_fail = false;
  signals:
    void elapsedChanged(qint64);
    void sampleRecorded(qint64 duration);
    void feedback(const QString &, bool);
};
class SessionController : public QObject {
    Q_OBJECT
  public:
    SessionController(AppState *, QString directory, QObject *parent = nullptr);
    void select(const QString &id);
    void togglePlayback();
    void seek(qint64 t);
    void setRate(double rate);
    void addRecorded(qint64 duration);
    void setFailExport(bool fail) {
        m_failExport = fail;
    }
    bool exportSample(const QString &path, bool csv);
    const DemoSession *current() const;
    AnalysisViewData analysis() const;
    qint64 position() const {
        return m_position;
    }
    bool playing() const {
        return m_playing;
    }
    double rate() const {
        return m_rate;
    }
    QString loadError() const {
        return m_loadError;
    }
    void setEmpty();
    void restoreSamples();

  private:
    void saveDemoSessions();
    QVector<DemoSession> m_seed;
    AppState *m_state;
    QString m_directory, m_loadError;
    std::unique_ptr<IAnalysisProvider> m_analysis;
    QTimer m_timer;
    QElapsedTimer m_tick;
    qint64 m_position = 3300;
    bool m_playing = false, m_failExport = false;
    double m_rate = 1.;
  signals:
    void positionChanged(qint64, qint64);
    void playbackChanged(bool);
    void feedback(const QString &, bool);
};
} // namespace ui
