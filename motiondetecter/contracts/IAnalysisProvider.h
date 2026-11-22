#pragma once
#include "core/types.h"
namespace ui {
struct AnalysisViewData {
    bool valid = false, simulated = true;
    double maximum = 0, rom = 0, peakSpeed = 0, symmetryPercent = 0, scorePercent = 0;
    QString grade, error;
};
class IAnalysisProvider {
  public:
    virtual ~IAnalysisProvider() = default;
    virtual AnalysisViewData analyze(const QVector<core::AngleSample> &samples) const = 0;
};
} // namespace ui
