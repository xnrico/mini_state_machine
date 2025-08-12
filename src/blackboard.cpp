#include "blackboard.hpp"

namespace msm {
blackboard::blackboard(const blackboard& other) {
  std::lock_guard lock(other.mtx);
  entries = other.entries;  // Shallow copy of shared_ptrs
}

auto blackboard::contains(const std::string& key) const -> bool {
  std::lock_guard lock(mtx);
  return entries.find(key) != entries.end();
}

auto blackboard::remove(const std::string& key) noexcept -> void {
  std::lock_guard lock(mtx);
  entries.erase(key);
}

auto blackboard::size() const noexcept -> size_t {
  std::lock_guard lock(mtx);
  return entries.size();
}

auto blackboard::clear() noexcept -> void {
  std::lock_guard lock(mtx);
  entries.clear();
}

auto blackboard::serialize() const -> std::string {
  std::lock_guard lock(mtx);
  std::string result = "{";
  for (const auto& [key, entry] : entries) {
    result += "\"" + key + "\": \"" + entry->to_string() + "\", ";
  }
  if (result.size() > 1) {
    result.pop_back();  // Remove last space
    result.pop_back();  // Remove last comma
  }
  result += "}";
  return result;
}

}  // namespace msm