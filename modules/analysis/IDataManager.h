#pragma once
#include "core/types.h"

namespace motion {
class IDataManager {
public:
    virtual ~IDataManager() = default;
    virtual Result saveSession(const SessionRecord& record) = 0;
    // Lists metadata only; loadSession retrieves the full sequence.
    virtual std::vector<SessionRecord> listSessions() const = 0;
    virtual Result loadSession(const std::string& id, SessionRecord& out) const = 0;
    virtual Result saveConfig(const AppConfig& config) = 0;
    virtual AppConfig loadConfig() const = 0;
    virtual Result resetConfig() = 0;
    virtual Result exportCsv(const std::string& id, const std::string& path) const = 0;
    virtual Result exportReport(const std::string& id, const std::string& path,
                                ReportFormat format) const = 0;
};
}
