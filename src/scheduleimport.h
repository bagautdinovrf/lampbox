#pragma once
#include "projectrepository.h"

// Read-only, one-time legacy adapter. No legacy writer or runtime dependency.
namespace ScheduleImport {
bool read(const ProjectRepository::Paths &paths, ProjectRepository::Project *project, QString *error);
}
