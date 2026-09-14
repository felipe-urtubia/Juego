#include "career/app_services.h"

#include "career/career_reports.h"
#include "validators/validators.h"

std::string buildCompetitionSummaryService(const Career& career) {
    return formatCareerReport(buildCompetitionReport(career));
}

std::string buildBoardSummaryService(const Career& career) {
    return formatCareerReport(buildBoardReport(career));
}

std::string buildClubSummaryService(const Career& career) {
    return formatCareerReport(buildClubReport(career));
}

std::string buildScoutingSummaryService(const Career& career) {
    return formatCareerReport(buildScoutingReport(career));
}

ValidationSuiteSummary runValidationService() {
    return buildValidationSuiteSummary();
}