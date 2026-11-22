#pragma once
#include "contracts/IAnalysisProvider.h"
namespace ui {
class AnalysisM0Adapter final : public IAnalysisProvider {
  public:
    AnalysisViewData analyze(const QVector<core::AngleSample> &samples) const override;
};
} // namespace ui
