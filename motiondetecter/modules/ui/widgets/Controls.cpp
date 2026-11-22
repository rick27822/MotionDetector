#include "Controls.h"
#include "modules/ui/theme/Theme.h"
#include <QHBoxLayout>
#include <QMouseEvent>
#include <QPainter>
#include <QStyle>
namespace ui {
QLabel *label(const QString &text, bool muted, QWidget *parent) {
    auto l = new QLabel(text, parent);
    l->setProperty("muted", muted);
    return l;
}
QLabel *title(const QString &text, QWidget *parent) {
    auto l = label(text, false, parent);
    l->setProperty("role", "title");
    return l;
}
QPushButton *button(const QString &text, const QString &name, const QString &variant) {
    auto b = new QPushButton(text);
    b->setAttribute(Qt::WA_LayoutUsesWidgetRect);
    if (!name.isEmpty())
        b->setObjectName(name);
    if (!variant.isEmpty())
        b->setProperty("variant", variant);
    b->setCursor(Qt::PointingHandCursor);
    return b;
}
QWidget *row(const QString &text, QWidget *control, QWidget *parent) {
    auto w = new QWidget(parent);
    auto l = new QHBoxLayout(w);
    l->setContentsMargins(0, 3, 0, 3);
    l->addWidget(label(text));
    l->addStretch();
    l->addWidget(control);
    return w;
}
void polish(QWidget *w) {
    w->style()->unpolish(w);
    w->style()->polish(w);
    w->update();
}
CardWidget::CardWidget(const QString &heading, QWidget *parent, int margin) : QFrame(parent) {
    setObjectName("card");
    body = new QVBoxLayout(this);
    body->setContentsMargins(margin, margin, margin, margin);
    body->setSpacing(12);
    if (!heading.isEmpty())
        body->addWidget(title(heading));
}
ToggleField::ToggleField(const QString &name, bool checked, QWidget *parent)
    : QAbstractButton(parent) {
    setAccessibleName(name);
    setCheckable(true);
    setChecked(checked);
    setFixedSize(30, 18);
    setCursor(Qt::PointingHandCursor);
    setFocusPolicy(Qt::StrongFocus);
}
void ToggleField::paintEvent(QPaintEvent *) {
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);
    p.setPen(Qt::NoPen);
    p.setBrush(!isEnabled() ? QColor("#242428") : isChecked() ? Orange : QColor("#414149"));
    p.drawRoundedRect(QRectF(0, 1, 30, 16), 8, 8);
    p.setBrush(!isEnabled() ? QColor("#626269") : isChecked() ? Qt::white : QColor("#A1A1AA"));
    p.drawEllipse(QPointF(isChecked() ? 23 : 7, 9), 5.5, 5.5);
    if (hasFocus()) {
        p.setPen(QPen(Muted, 1));
        p.setBrush(Qt::NoBrush);
        p.drawRoundedRect(QRectF(1, 0, 28, 18), 8, 8);
    }
}
SliderField::SliderField(const QString &text, const QString &name, int value, QWidget *parent)
    : QWidget(parent) {
    auto l = new QVBoxLayout(this);
    l->setContentsMargins(0, 0, 0, 0);
    l->setSpacing(5);
    auto top = new QHBoxLayout;
    top->addWidget(label(text, true));
    top->addStretch();
    valueLabel = label("");
    valueLabel->setStyleSheet("font-family: 'Menlo', monospace; font-size:12px; color:white;");
    top->addWidget(valueLabel);
    l->addLayout(top);
    slider = new FlatSlider;
    slider->setRange(0, 100);
    slider->setObjectName(name);
    slider->setAccessibleName(text);
    l->addWidget(slider);
    connect(slider, &QSlider::valueChanged, this,
            [this](int v) { valueLabel->setText(QString::number(v / 100., 'f', 2)); });
    setValue(value);
}
void SliderField::setValue(int v) {
    slider->setValue(v);
    valueLabel->setText(QString::number(slider->value() / 100., 'f', 2));
}
FlatSlider::FlatSlider(QWidget *p) : QSlider(Qt::Horizontal, p) {
    setFixedHeight(14);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    setFocusPolicy(Qt::StrongFocus);
}
int FlatSlider::valueAt(qreal x) const {
    return minimum() +
           qRound(qBound(0., (x - 5) / qMax(1., width() - 10.), 1.) * (maximum() - minimum()));
}
void FlatSlider::paintEvent(QPaintEvent *) {
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);
    qreal left = 5, right = width() - 5, y = height() / 2.;
    qreal x =
        left + (value() - minimum()) / double(qMax(1, maximum() - minimum())) * (right - left);
    p.setPen(QPen(QColor("#1C1C1E"), 4, Qt::SolidLine, Qt::RoundCap));
    p.drawLine(QPointF(left, y), QPointF(right, y));
    p.setPen(QPen(isEnabled() ? Orange : QColor("#626269"), 4, Qt::SolidLine, Qt::RoundCap));
    p.drawLine(QPointF(left, y), QPointF(x, y));
    if (isEnabled() && (underMouse() || hasFocus())) {
        p.setBrush(Orange);
        p.setPen(Qt::NoPen);
        p.drawEllipse(QPointF(x, y), 4, 4);
    }
}
void FlatSlider::mousePressEvent(QMouseEvent *e) {
    if (e->button() == Qt::LeftButton) {
        setFocus(Qt::MouseFocusReason);
        setSliderDown(true);
        setSliderPosition(valueAt(e->position().x()));
        e->accept();
    }
}
void FlatSlider::mouseMoveEvent(QMouseEvent *e) {
    if (e->buttons() & Qt::LeftButton) {
        setSliderPosition(valueAt(e->position().x()));
        e->accept();
    }
}
void FlatSlider::mouseReleaseEvent(QMouseEvent *e) {
    if (e->button() == Qt::LeftButton) {
        setSliderPosition(valueAt(e->position().x()));
        setSliderDown(false);
        e->accept();
    }
}
ScoreRing::ScoreRing(QWidget *parent) : QWidget(parent) {
    setMinimumHeight(150);
}
void ScoreRing::setScore(double s, const QString &g) {
    m_score = s;
    m_grade = g;
    update();
}
void ScoreRing::paintEvent(QPaintEvent *) {
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);
    qreal size = qMin(110, qMin(width() - 12, height() - 12));
    QRectF r((width() - size) / 2., (height() - size) / 2., size, size);
    p.setPen(QPen(QColor("#252529"), 10, Qt::SolidLine, Qt::RoundCap));
    p.drawArc(r, 210 * 16, -360 * 16);
    if (m_score >= 0) {
        p.setPen(QPen(Orange, 10, Qt::SolidLine, Qt::RoundCap));
        p.drawArc(r, 180 * 16, -qRound(qBound(0., m_score, 100.) / 100. * 360 * 16));
    }
    QFont f = font();
    f.setPixelSize(30);
    f.setWeight(QFont::DemiBold);
    p.setFont(f);
    p.setPen(Qt::white);
    p.drawText(r.adjusted(0, 17, 0, -25), Qt::AlignCenter,
               m_score < 0 ? "—" : QString::number(m_score, 'f', 0));
    f.setPixelSize(12);
    f.setWeight(QFont::Normal);
    p.setFont(f);
    p.setPen(Muted);
    p.drawText(r.adjusted(0, 54, 0, -8), Qt::AlignCenter, m_grade);
}
PairingPlaceholder::PairingPlaceholder(QWidget *p) : QWidget(p) {
    setFixedSize(200, 200);
    setAccessibleName("配对二维码占位，尚未接入手机连接服务");
    setToolTip("UI 样例：没有有效配对地址，不能扫描连接");
}
void PairingPlaceholder::paintEvent(QPaintEvent *) {
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);
    p.setBrush(Qt::white);
    p.setPen(Qt::NoPen);
    p.drawRoundedRect(rect(), 8, 8);
    // Deliberately not a QR: no valid pairing service exists in the handoff.
    p.setPen(QPen(QColor("#C8C8CC"), 2));
    p.drawRoundedRect(QRectF(30, 30, 140, 140), 5, 5);
    p.setPen(QColor("#626269"));
    QFont f = font();
    f.setPixelSize(14);
    f.setWeight(QFont::DemiBold);
    p.setFont(f);
    p.drawText(QRect(20, 57, 160, 50), Qt::AlignCenter, "配对二维码");
    f.setPixelSize(11);
    f.setWeight(QFont::Normal);
    p.setFont(f);
    p.drawText(QRect(15, 108, 170, 40), Qt::AlignCenter, "手机连接服务待接入\n当前为界面验收模式");
}
} // namespace ui
