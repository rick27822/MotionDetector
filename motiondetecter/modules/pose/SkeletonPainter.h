// modules/pose/SkeletonPainter.h —— 骨架绘制工具（张在庆 提供，陈柯睿 消费）
//
// UI 只负责给出目标矩形，几何换算、连线拓扑、置信度着色都在这里完成，
// 保证 P1 实时页与 P4 回放页的骨架表现完全一致。
#pragma once

#include "core/types.h"

#include <QColor>
#include <QPainter>
#include <QPair>
#include <QRectF>
#include <QSize>
#include <QVector>

namespace pose {

struct SkeletonStyle {
    QColor leftBone{0x4d, 0xd4, 0xff};
    QColor rightBone{0xc0, 0x8b, 0xff};
    QColor centerBone{0x5c, 0xe3, 0xc0};
    QColor jointFill{0xff, 0xff, 0xff};
    QColor jointHalo{0x0d, 0x11, 0x17};
    qreal boneWidthScale = 0.0055;   // 相对目标矩形高度
    qreal jointRadiusScale = 0.0060;
    float minConfidence = 0.30f;
    bool drawJoints = true;
    bool drawLabels = false;
    bool glow = true;
    bool mirror = false;
};

class SkeletonPainter
{
public:
    /// 保持画面宽高比的目标矩形（letterbox）。
    static QRectF fittedRect(const QSize &frameSize, const QRectF &target);

    /// 归一化坐标 → 像素坐标。
    static QPointF toPixel(const core::Keypoint &keypoint, const QRectF &fitted, bool mirror);

    /// 骨架连线（BlazePose 33 拓扑；无效点自动跳过，因此 17/16 预设同样适用）。
    static QVector<QPair<int, int>> connections();

    /// 在 target 内绘制骨架（自动 letterbox，与 VideoView 的画面区域一致）。
    /// frameSize 是原始画面尺寸，默认按 1280×720（16:9）处理。
    static void draw(QPainter &painter, const QRectF &target, const core::PoseResult &pose,
                     const SkeletonStyle &style = SkeletonStyle(),
                     const QSize &frameSize = QSize(1280, 720));

    /// 仅绘制关节点（配置页预览用）。
    static void drawJoints(QPainter &painter, const QRectF &target, const core::PoseResult &pose,
                           const SkeletonStyle &style = SkeletonStyle(),
                           const QSize &frameSize = QSize(1280, 720));
};

} // namespace pose
