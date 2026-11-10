#pragma once
#include "../ScopeFrame.h"
#include <QString>
// Text-only formatter, no widget ownership or UI dependencies.
QString formatScopeMetrics(const ScopeFrame& frame);
