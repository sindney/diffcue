// git/git_status.h implementation — short_code / label (task 5.1).
#include "git/git_status.h"

namespace diffcue::git {

const char* GitEntry::short_code() const {
    switch (status) {
        case FileStatus::Modified:  return "M";
        case FileStatus::Added:     return "A";
        case FileStatus::Deleted:   return "D";
        case FileStatus::Renamed:   return "R";
        case FileStatus::Untracked: return "?";
        case FileStatus::Clean:     return " ";
    }
    return " ";
}

const char* GitEntry::label() const {
    switch (status) {
        case FileStatus::Modified:  return "Modified";
        case FileStatus::Added:     return "Added";
        case FileStatus::Deleted:   return "Deleted";
        case FileStatus::Renamed:   return "Renamed";
        case FileStatus::Untracked: return "Untracked";
        case FileStatus::Clean:     return "Clean";
    }
    return "Clean";
}

}  // namespace diffcue::git
