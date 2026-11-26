#include "Sidebar.h"
#include "modules/ui/theme/Theme.h"
#include "modules/ui/widgets/Controls.h"
#include <QHBoxLayout>
namespace ui {
Sidebar::Sidebar(AppState *state, QWidget *parent) : QWidget(parent), m_state(state) {
    setObjectName("sidebar");
    setAttribute(Qt::WA_StyledBackground);
    setFixedWidth(225);
    auto l = new QVBoxLayout(this);
    l->setContentsMargins(16, 18, 16, 16);
    l->setSpacing(2);
    auto brand = new QHBoxLayout;
    brand->setSpacing(10);
    brand->addSpacing(4);
    auto logo = label("");
    logo->setPixmap(icon("wave", Orange).pixmap(28, 28));
    logo->setFixedSize(28, 28);
    logo->setStyleSheet("background:#2D1D07;border-radius:8px;");
    brand->addWidget(logo);
    auto words = new QVBoxLayout;
    words->setSpacing(1);
    auto name = title("PoseLab");
    name->setStyleSheet("font-size:16px;font-weight:600;color:white;");
    words->addWidget(name);
    auto subtitle = label("动作捕捉检测系统", true);
    subtitle->setStyleSheet("font-size:10px;color:#A1A1AA;");
    words->addWidget(subtitle);
    brand->addLayout(words);
    brand->addStretch();
    l->addLayout(brand);
    l->addSpacing(6);
    navigation = new QButtonGroup(this);
    navigation->setExclusive(true);
    QStringList names = {"实时检测", "设备连接", "检测配置", "录像回放", "数据分析", "系统设置"};
    QStringList icons = {"wave", "phone", "sliders", "video", "chart", "settings"};
    for (int i = 0; i < 6; i++) {
        auto b = button("  " + names[i], "nav");
        b->setProperty("pageIndex", i);
        b->setAccessibleName(names[i]);
        b->setCheckable(true);
        b->setFixedHeight(36);
        b->setIcon(icon(icons[i]));
        b->setIconSize({18, 18});
        navigation->addButton(b, i);
        l->addWidget(b);
    }
    connect(navigation, &QButtonGroup::idClicked, this,
            [this](int i) { emit pageRequested(static_cast<PageId>(i)); });
    l->addStretch();
    auto status = new CardWidget({}, this, 12);
    status->setObjectName("subcard");
    status->setFixedHeight(58);
    status->body->setSpacing(4);
    m_device = label("");
    m_detail = label("", true);
    m_detail->setStyleSheet("font-size:10px;color:#A1A1AA;");
    status->body->addWidget(m_device);
    status->body->addWidget(m_detail);
    l->addWidget(status);
    auto refresh = [this] {
        m_device->setText((m_state->connected ? "<font color='#00E676'>●</font> "
                                              : "<font color='#71717A'>●</font> ") +
                          (m_state->connected ? m_state->deviceName : "未连接设备"));
        m_detail->setText(m_state->connected ? "样例连接 · " + m_state->config.resolution + " · " +
                                                   QString::number(m_state->config.fps) + "fps"
                                             : "前往设备连接页开始");
        if (m_state->realtimeInput)
            m_detail->setText(m_state->connected ? "姿态输入 · 无对应视频" : "等待重新绑定姿态输入");
    };
    connect(state, &AppState::deviceChanged, this, refresh);
    connect(state, &AppState::configChanged, this, refresh);
    refresh();
    setCurrent(PageId::Realtime);
}
void Sidebar::setCurrent(PageId id) {
    if (auto b = navigation->button(int(id)))
        b->setChecked(true);
    const QStringList names = {"wave", "phone", "sliders", "video", "chart", "settings"};
    for (int i = 0; i < 6; i++)
        qobject_cast<QPushButton *>(navigation->button(i))
            ->setIcon(icon(names[i], i == int(id) ? Orange : Muted));
}
} // namespace ui
