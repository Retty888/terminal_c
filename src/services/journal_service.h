#pragma once

#include <string>
#include <filesystem>

#include "journal.h"
#include "core/data_dir.h"

// Simple wrapper around the Journal class.  Abstracting it into a
// service allows the application to interact with journal data without
// being tied to a concrete storage implementation.
class JournalService {
public:
  explicit JournalService(const std::filesystem::path &base_dir = Core::resolve_data_dir());

  bool load(const std::string &filename);
  bool save(const std::string &filename) const;

  void set_base_dir(const std::filesystem::path &dir);
  const std::filesystem::path &base_dir() const { return m_base_dir; }

  Journal::Journal &journal() { return m_journal; }

private:
  Journal::Journal m_journal;
  std::filesystem::path m_base_dir;
};

