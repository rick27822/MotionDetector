#pragma once
#include <QAbstractButton>
#include <QFrame>
#include <QLabel>
#include <QPushButton>
#include <QSlider>
#include <QVBoxLayout>
namespace ui {
QLabel *label(const QString &text, bool muted = false, QWidget *parent = nullptr);
QLabel *title(const QString &text, QWidget *parent = nullptr);
QPushButton *button(const QString &text, const QString &name = {}, const QString &variant = {});
QWidget *row(const QString &text, QWidget *control, QWidget *parent = nullptr);
void polish(QWidget *w);
class CardWidget : public QFrame {
  public:
    explicit CardWidget(const QString &heading = {}, QWidget *parent = nullptr, int margin = 20);
    QVBoxLayout *body;
};
class ToggleField : public QAbstractButton {
    Q_OBJECT
  public:
    explicit ToggleField(const QString &accessibleName, bool checked = false,
                         QWidget *parent = nullptr);
    QSize sizeHint() const override {
        return {30, 18};
    }

  protected:
    void paintEvent(QPaintEvent *) override;
};
class SliderField : public QWidget {
    Q_OBJECT
  public:
    SliderField(const QString &text, const QString &name, int value, QWidget *parent = nullptr);
    QSlider *slider;
    QLabel *valueLabel;
    void setValue(int v);
};
class FlatSlider : public QSlider {
  public:
    explicit FlatSlider(QWidget *parent = nullptr);

  protected:
    void paintEvent(QPaintEvent *) override;
    void mousePressEvent(QMouseEvent *) override;
    void mouseMoveEvent(QMouseEvent *) override;
    void mouseReleaseEvent(QMouseEvent *) override;

  private:
    int valueAt(qreal x) const;
};
class ScoreRing : public QWidget {
  public:
    explicit ScoreRing(QWidget *parent = nullptr);
    void setScore(double score, const QString &grade);

  protected:
    void paintEvent(QPaintEvent *) override;

  private:
    double m_score = -1;
    QString m_grade;
};
class PairingPlaceholder : public QWidget {
  public:
    explicit PairingPlaceholder(QWidget *parent = nullptr);

  protected:
    void paintEvent(QPaintEvent *) override;
};
} // namespace ui
