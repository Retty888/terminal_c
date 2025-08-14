#include "services/journal_service.h"

bool JournalService::load(const std::string &filename) {
  return m_journal.load_json(filename);
}

bool JournalService::save(const std::string &filename) const {
  return m_journal.save_json(filename);
}

