#include "services/journal_service.h"
#include <filesystem>

JournalService::JournalService(const std::filesystem::path &base_dir)
    : m_base_dir(base_dir) {
  std::filesystem::create_directories(m_base_dir);
}

bool JournalService::load(const std::string &filename) {
  return m_journal.load_json((m_base_dir / filename).string());
}

bool JournalService::save(const std::string &filename) const {
  return m_journal.save_json((m_base_dir / filename).string());
}

void JournalService::set_base_dir(const std::filesystem::path &dir) {
  m_base_dir = dir;
  std::filesystem::create_directories(m_base_dir);
}

