#include "services/journal_service.h"
#include <filesystem>
#include "core/logger.h"

JournalService::JournalService(const std::filesystem::path &base_dir)
    : m_base_dir(base_dir) {
  std::filesystem::create_directories(m_base_dir);
}

bool JournalService::load(const std::string &filename) {
  auto path = m_base_dir / filename;
  if (!std::filesystem::exists(path)) {
    if (!save(filename)) {
      Core::Logger::instance().error("Failed to create journal file: " +
                                     path.string());
      return false;
    }
    return true;
  }
  if (!m_journal.load_json(path.string())) {
    Core::Logger::instance().error("Failed to load journal from " +
                                   path.string());
    return false;
  }
  return true;
}

bool JournalService::save(const std::string &filename) const {
  auto path = m_base_dir / filename;
  if (!m_journal.save_json(path.string())) {
    Core::Logger::instance().error("Failed to save journal to " +
                                   path.string());
    return false;
  }
  return true;
}

void JournalService::set_base_dir(const std::filesystem::path &dir) {
  m_base_dir = dir;
  std::filesystem::create_directories(m_base_dir);
}
