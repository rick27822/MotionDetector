#pragma once
#include "core/types.h"
namespace ui {
class PoseSampleReader {
  public:
    static bool readPoses(const QString &path, QVector<core::PoseResult> &out, QString &error);
    static bool readSeries(const QString &path, QVector<core::AngleSample> &out, QString &error);
    static bool parseAngles(const QJsonObject &values, quint32 mask,
                            std::array<float, core::kAngleCount> &out, QString &error);
};
} // namespace ui
