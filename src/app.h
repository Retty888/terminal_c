#pragma once

#include "services/data_service.h"
#include "services/journal_service.h"

// The App class owns the services and drives the main event loop.
class App {
public:
  // Runs the application. Returns the exit code.
  int run();

private:
  DataService data_service_;
  JournalService journal_service_;
};

