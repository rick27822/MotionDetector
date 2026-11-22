#include "DevicePage.h"
#include "modules/ui/theme/Theme.h"
#include "modules/ui/widgets/Controls.h"
#include <QGridLayout>
namespace ui {
DevicePage::DevicePage(AppState *s, DeviceController *c, QWidget *p)
    : QWidget(p), m_state(s), m_controller(c) {
    setObjectName("page");
    auto root = new QHBoxLayout(this);
    root->setContentsMargins(24, 24, 24, 24);
    root->setSpacing(24);
    auto pair = new CardWidget({}, this, 24);
    pair->setFixedWidth(400);
    pair->body->setSpacing(16);
    auto heading = title("扫码连接手机摄像头");
    heading->setAlignment(Qt::AlignCenter);
    pair->body->addWidget(heading);
    pair->body->addWidget(new PairingPlaceholder, 0, Qt::AlignHCenter);
    auto intro = label(
        "手机连接服务尚未交付，当前展示配对入口。\n此占位区域不能用于扫描连接真实手机。", true);
    intro->setWordWrap(true);
    intro->setStyleSheet("color:#A1A1AA;font-size:12px;");
    pair->body->addWidget(intro);
    for (int i = 0; i < 3; i++) {
        QStringList steps = {"手机与电脑连接同一 Wi-Fi 网络", "扫描真实配对码并允许摄像头权限",
                             "推流成功后即可开始实时检测"};
        pair->body->addWidget(
            label(QString("<font color='#FF8800'>%1</font>　%2").arg(i + 1).arg(steps[i])));
    }
    pair->body->addStretch();
    root->addWidget(pair);
    auto right = new QVBoxLayout;
    right->setSpacing(20);
    auto devices = new CardWidget("已发现设备", this);
    devices->setFixedHeight(267);
    devices->body->setSpacing(10);
    auto device = [&](const QString &name, const QString &detail, QPushButton *&action,
                      const QString &id) {
        auto card = new CardWidget({}, devices, 12);
        card->setObjectName("subcard");
        card->setFixedHeight(58);
        auto h = new QHBoxLayout;
        auto v = new QVBoxLayout;
        v->setSpacing(2);
        v->addWidget(label(name));
        auto d = label(detail, true);
        d->setStyleSheet("font-size:11px;color:#A1A1AA;font-family:'Menlo';");
        v->addWidget(d);
        h->addLayout(v);
        h->addStretch();
        action = button("连接", id);
        action->setStyleSheet("padding:3px 10px;font-size:12px;color:#FF8800;");
        h->addWidget(action);
        card->body->addLayout(h);
        devices->body->addWidget(card);
    };
    device("iPhone 15 Pro", "192.168.1.24 · 样例设备", m_iphone, "connectIphone");
    device("Xiaomi 14 Ultra", "192.168.1.31 · 在线样例", m_xiaomi, "connectXiaomi");
    QPushButton *offline;
    device("iPad Pro", "离线 · 2 小时前（样例）", offline, "connectOffline");
    offline->setText("不可用");
    offline->setEnabled(false);
    right->addWidget(devices);
    auto info = new CardWidget({}, this);
    info->setFixedHeight(257);
    m_info = title("");
    info->body->addWidget(m_info);
    auto grid = new QGridLayout;
    grid->setSpacing(12);
    QStringList labels = {"分辨率", "帧率", "网络延迟（样例）", "电量（样例）"};
    QStringList values = {"1920×1080", "30 fps", "45 ms", "78%"};
    for (int i = 0; i < 4; i++) {
        auto box = new CardWidget({}, info, 12);
        box->setObjectName("subcard");
        box->setFixedHeight(61);
        box->body->setSpacing(4);
        box->body->addWidget(label(labels[i], true));
        auto val = title(values[i]);
        m_infoValues.append(val);
        if (i == 3)
            val->setStyleSheet("color:#00E676;font-size:14px;font-family:'Menlo';");
        box->body->addWidget(val);
        grid->addWidget(box, i / 2, i % 2);
    }
    info->body->addLayout(grid);
    auto actions = new QHBoxLayout;
    m_disconnect = button("断开连接", "disconnectDevice", "danger");
    m_refresh = button("刷新设备", "refreshDevices");
    actions->addWidget(m_disconnect);
    actions->addWidget(m_refresh);
    info->body->addLayout(actions);
    right->addWidget(info);
    m_status = label("设备与地址均为 UI 样例，未建立真实网络连接。", true);
    m_status->setWordWrap(true);
    right->addWidget(m_status);
    right->addStretch();
    root->addLayout(right, 1);
    connect(m_iphone, &QPushButton::clicked, c, [c] { c->connectDevice("iphone"); });
    connect(m_xiaomi, &QPushButton::clicked, c, [c] { c->connectDevice("xiaomi"); });
    connect(m_disconnect, &QPushButton::clicked, c, &DeviceController::disconnectDevice);
    connect(m_refresh, &QPushButton::clicked, c, &DeviceController::refresh);
    connect(s, &AppState::deviceChanged, this, &DevicePage::refresh);
    connect(s, &AppState::configChanged, this, &DevicePage::refresh);
    connect(c, &DeviceController::busyChanged, this, [this] { refresh(); });
    connect(c, &DeviceController::feedback, this, [this](const QString &m, bool ok) {
        m_status->setText(m);
        m_status->setStyleSheet(ok ? "color:#A1A1AA;" : "color:#FF6B80;");
    });
    refresh();
}
void DevicePage::refresh() {
    QStringList values = {m_state->config.resolution, QString::number(m_state->config.fps) + " fps",
                          "45 ms", "78%"};
    for (int i = 0; i < m_infoValues.size(); i++)
        m_infoValues[i]->setText(m_state->connected ? values[i] : "—");
    bool busy = m_controller->busy();
    m_info->setText("设备信息 · " + (m_state->connected ? m_state->deviceName : "未连接"));
    m_iphone->setText(m_state->connected && m_state->deviceId == "iphone" ? "使用中" : "连接");
    m_xiaomi->setText(m_state->connected && m_state->deviceId == "xiaomi" ? "使用中" : "连接");
    m_iphone->setEnabled(!busy && (!m_state->connected || m_state->deviceId != "iphone"));
    m_xiaomi->setEnabled(!busy && (!m_state->connected || m_state->deviceId != "xiaomi"));
    m_disconnect->setEnabled(m_state->connected && !busy);
    m_refresh->setEnabled(!busy);
    m_refresh->setText(busy ? "正在处理…" : "刷新设备");
}
} // namespace ui
