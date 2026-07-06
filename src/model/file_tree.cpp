// model/file_tree.cpp — build a directory tree from git entries (task 6.2).
#include "model/file_tree.h"

#include <algorithm>
#include <functional>
#include <map>

namespace diffcue::model {

namespace {

using diffcue::git::FileStatus;

diffcue::git::FileStatus aggregate_status(const std::vector<std::unique_ptr<FileTreeNode>>& children) {
    // A folder is "Clean" only if every child is Clean; otherwise show the
    // first non-Clean status we encounter (Modified wins over Untracked for
    // visibility — but for simplicity, Modified takes priority).
    bool any_modified = false, any_added = false, any_deleted = false;
    bool any_untracked = false, any_renamed = false;
    for (const auto& c : children) {
        if (c->is_dir) {
            // Recurse into subdirs.
            FileStatus s = aggregate_status(c->children);
            switch (s) {
                case diffcue::git::FileStatus::Modified:  any_modified = true; break;
                case diffcue::git::FileStatus::Added:     any_added = true; break;
                case diffcue::git::FileStatus::Deleted:   any_deleted = true; break;
                case diffcue::git::FileStatus::Renamed:   any_renamed = true; break;
                case diffcue::git::FileStatus::Untracked: any_untracked = true; break;
                default: break;
            }
        } else {
            switch (c->status) {
                case diffcue::git::FileStatus::Modified:  any_modified = true; break;
                case diffcue::git::FileStatus::Added:     any_added = true; break;
                case diffcue::git::FileStatus::Deleted:   any_deleted = true; break;
                case diffcue::git::FileStatus::Renamed:   any_renamed = true; break;
                case diffcue::git::FileStatus::Untracked: any_untracked = true; break;
                default: break;
            }
        }
    }
    if (any_modified)  return diffcue::git::FileStatus::Modified;
    if (any_added)     return diffcue::git::FileStatus::Added;
    if (any_deleted)   return diffcue::git::FileStatus::Deleted;
    if (any_renamed)   return diffcue::git::FileStatus::Renamed;
    if (any_untracked) return diffcue::git::FileStatus::Untracked;
    return diffcue::git::FileStatus::Clean;
}

int count_changed_files(const FileTreeNode& node) {
    if (!node.is_dir) {
        return node.status != diffcue::git::FileStatus::Clean ? 1 : 0;
    }
    int n = 0;
    for (const auto& c : node.children) {
        n += count_changed_files(*c);
    }
    return n;
}

}  // namespace

std::unique_ptr<FileTreeNode> build_file_tree(const std::vector<diffcue::git::GitEntry>& entries) {
    auto root = std::make_unique<FileTreeNode>();
    root->name = "";
    root->relpath = "";
    root->is_dir = true;

    // Use a map keyed by path segments to build the tree. Each entry's
    // relpath is split into components; intermediate dirs become dir nodes.
    for (const auto& e : entries) {
        FileTreeNode* cur = root.get();
        const std::string p = e.relpath.generic_string();
        if (p.empty()) continue;

        // Split on '/'.
        size_t start = 0;
        size_t sep = p.find('/');
        std::vector<std::string> segs;
        while (sep != std::string::npos) {
            segs.push_back(p.substr(start, sep - start));
            start = sep + 1;
            sep = p.find('/', start);
        }
        segs.push_back(p.substr(start));

        // All segments except the last are directories.
        for (size_t i = 0; i + 1 < segs.size(); ++i) {
            const std::string& seg = segs[i];
            // Find or create the child dir.
            FileTreeNode* child = nullptr;
            for (auto& c : cur->children) {
                if (c->is_dir && c->name == seg) { child = c.get(); break; }
            }
            if (!child) {
                auto node = std::make_unique<FileTreeNode>();
                node->name = seg;
                node->is_dir = true;
                node->relpath = std::filesystem::path(cur->relpath) / seg;
                cur->children.push_back(std::move(node));
                child = cur->children.back().get();
            }
            cur = child;
        }

        // Last segment is the file.
        const std::string& fname = segs.back();
        auto file_node = std::make_unique<FileTreeNode>();
        file_node->name = fname;
        file_node->is_dir = false;
        file_node->relpath = e.relpath;
        file_node->status = e.status;
        cur->children.push_back(std::move(file_node));
    }

    // Sort each level: dirs first (alphabetical), then files (alphabetical).
    std::function<void(FileTreeNode*)> sort_rec = [&](FileTreeNode* node) {
        std::sort(node->children.begin(), node->children.end(),
                  [](const std::unique_ptr<FileTreeNode>& a,
                     const std::unique_ptr<FileTreeNode>& b) {
                      if (a->is_dir != b->is_dir) return a->is_dir;  // dirs first
                      return a->name < b->name;
                  });
        for (auto& c : node->children) sort_rec(c.get());
    };
    sort_rec(root.get());

    // Aggregate statuses and counts.
    std::function<void(FileTreeNode*)> agg = [&](FileTreeNode* node) {
        for (auto& c : node->children) agg(c.get());
        if (node->is_dir) {
            node->status = aggregate_status(node->children);
            node->changed_file_count = count_changed_files(*node);
        } else {
            node->changed_file_count = (node->status != diffcue::git::FileStatus::Clean) ? 1 : 0;
        }
    };
    agg(root.get());

    return root;
}

}  // namespace diffcue::model
