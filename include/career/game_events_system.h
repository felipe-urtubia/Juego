#pragma once

#include "engine/models.h"

#include <string>
#include <vector>
#include <ctime>

namespace career_events {

enum class EventType {
    CriticalInjury,
    PlayerOffered,
    TransferCompleted,
    ManagerAlert,
    FormAlert,
    AchievementUnlocked,
    CareerMilestone,
    FinancialWarning,
    MoraleAlert
};

struct GameEvent {
    EventType type;
    std::string title;
    std::string message;
    time_t timestamp;
    bool isRead = false;
};

class EventNotificationSystem {
public:
    static void recordEvent(EventType type, const std::string& title, const std::string& message);
    static std::vector<GameEvent> getUnreadEvents();
    static std::vector<GameEvent> getAllEvents();
    static void markEventAsRead(size_t index);
    static void clearOldEvents();
    static int getUnreadCount();
};

// Milestone tracking
bool checkCareerMilestone(const Career& career, int& outMilestoneWeek);
std::string GetMilestoneDescription(int weekNumber, const Career& career);

}  // namespace career_events
