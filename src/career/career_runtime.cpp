#include "career/career_runtime.h"

#include <iostream>
#include <utility>

using namespace std;

namespace {

thread_local CareerRuntimeContext g_defaultRuntimeContext;
thread_local CareerRuntimeContext* g_currentRuntimeContext = &g_defaultRuntimeContext;

CareerRuntimeContext& mutableCurrentContext() {
    return *g_currentRuntimeContext;
}

const CareerRuntimeContext& currentContext() {
    return *g_currentRuntimeContext;
}

}  // namespace

ScopedCareerRuntimeContext::ScopedCareerRuntimeContext(CareerRuntimeContext context)
    : context_(std::move(context)),
      previous_(g_currentRuntimeContext),
      active_(true) {
    if (context_.presentation != WeekSimulationPresentation::Compact &&
        context_.presentation != WeekSimulationPresentation::Detailed &&
        context_.presentation != WeekSimulationPresentation::MatchCenter) {
        context_.presentation = WeekSimulationPresentation::Detailed;
    }
    g_currentRuntimeContext = &context_;
}

ScopedCareerRuntimeContext::~ScopedCareerRuntimeContext() {
    if (!active_) return;
    g_currentRuntimeContext = previous_ ? previous_ : &g_defaultRuntimeContext;
}

void setManagerJobSelectionCallback(ManagerJobSelectionCallback callback) {
    mutableCurrentContext().managerJobSelection = callback;
}

void setUiMessageCallback(UiMessageCallback callback) {
    mutableCurrentContext().uiMessage = callback;
}

void setIdleCallback(IdleCallback callback) {
    mutableCurrentContext().idle = callback;
}

void setIncomingOfferDecisionCallback(IncomingOfferDecisionCallback callback) {
    mutableCurrentContext().incomingOfferDecision = callback;
}

void setContractRenewalDecisionCallback(ContractRenewalDecisionCallback callback) {
    mutableCurrentContext().contractRenewalDecision = callback;
}

void setWeekSimulationPresentation(WeekSimulationPresentation presentation) {
    mutableCurrentContext().presentation = presentation;
}

CareerRuntimeContext currentCareerRuntimeContext() {
    return currentContext();
}

ManagerJobSelectionCallback managerJobSelectionCallback() {
    return currentContext().managerJobSelection;
}

UiMessageCallback uiMessageCallback() {
    return currentContext().uiMessage;
}

IncomingOfferDecisionCallback incomingOfferDecisionCallback() {
    return currentContext().incomingOfferDecision;
}

ContractRenewalDecisionCallback contractRenewalDecisionCallback() {
    return currentContext().contractRenewalDecision;
}

IdleCallback idleCallback() {
    return currentContext().idle;
}

WeekSimulationPresentation weekSimulationPresentation() {
    return currentContext().presentation;
}

void emitUiMessage(const string& message) {
    if (UiMessageCallback callback = uiMessageCallback()) {
        callback(message);
    } else {
        cout << message << endl;
    }
}
