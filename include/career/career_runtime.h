#pragma once

#include "engine/models.h"

#include <string>
#include <vector>

struct IncomingOfferDecision {
    int action = 3;
    long long counterOffer = 0;
};

enum class WeekSimulationPresentation {
    Compact,
    Detailed,
    MatchCenter
};

using ManagerJobSelectionCallback = int (*)(const Career& career, const std::vector<Team*>& jobs);
using UiMessageCallback = void (*)(const std::string& message);
using IdleCallback = void (*)();
using IncomingOfferDecisionCallback = IncomingOfferDecision (*)(const Career& career,
                                                                const Player& player,
                                                                long long offer,
                                                                long long maxOffer);
using ContractRenewalDecisionCallback = bool (*)(const Career& career,
                                                 const Team& team,
                                                 const Player& player,
                                                 long long demandedWage,
                                                 int demandedWeeks,
                                                 long long demandedClause);

struct CareerRuntimeContext {
    ManagerJobSelectionCallback managerJobSelection = nullptr;
    UiMessageCallback uiMessage = nullptr;
    IdleCallback idle = nullptr;
    IncomingOfferDecisionCallback incomingOfferDecision = nullptr;
    ContractRenewalDecisionCallback contractRenewalDecision = nullptr;
    WeekSimulationPresentation presentation = WeekSimulationPresentation::Detailed;
};

class ScopedCareerRuntimeContext {
public:
    explicit ScopedCareerRuntimeContext(CareerRuntimeContext context);
    ~ScopedCareerRuntimeContext();

    ScopedCareerRuntimeContext(const ScopedCareerRuntimeContext&) = delete;
    ScopedCareerRuntimeContext& operator=(const ScopedCareerRuntimeContext&) = delete;

private:
    CareerRuntimeContext context_{};
    CareerRuntimeContext* previous_ = nullptr;
    bool active_ = false;
};

void setManagerJobSelectionCallback(ManagerJobSelectionCallback callback);
void setUiMessageCallback(UiMessageCallback callback);
void setIdleCallback(IdleCallback callback);
void setIncomingOfferDecisionCallback(IncomingOfferDecisionCallback callback);
void setContractRenewalDecisionCallback(ContractRenewalDecisionCallback callback);
void setWeekSimulationPresentation(WeekSimulationPresentation presentation);

CareerRuntimeContext currentCareerRuntimeContext();
ManagerJobSelectionCallback managerJobSelectionCallback();
UiMessageCallback uiMessageCallback();
IdleCallback idleCallback();
IncomingOfferDecisionCallback incomingOfferDecisionCallback();
ContractRenewalDecisionCallback contractRenewalDecisionCallback();
WeekSimulationPresentation weekSimulationPresentation();

void emitUiMessage(const std::string& message);
