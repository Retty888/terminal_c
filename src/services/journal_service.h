#pragma once

#include <string>

#include "journal.h"

// Simple wrapper around the Journal class.  Abstracting it into a
// service allows the application to interact with journal data without
// being tied to a concrete storage implementation.
class JournalService {
public:
  bool load(const std::string &filename);
  bool save(const std::string &filename) const;

  Journal::Journal &journal() { return m_journal; }

private:
  Journal::Journal m_journal;
};

