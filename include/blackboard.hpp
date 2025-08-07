#pragma once

#include <memory>
#include <mutex>
#include <string>
#include <unordered_map>
#include <optional>

namespace msm {
// Interface for a blackboard entry
class blackboard_entry_interface {
 public:
  using ptr = std::shared_ptr<blackboard_entry_interface>;
  virtual ~blackboard_entry_interface() = default;
  virtual std::string to_string() const = 0;
};

template <typename T>
class blackboard_entry : public blackboard_entry_interface {
 private:
  std::string value;

 public:
  using ptr = std::shared_ptr<blackboard_entry<T>>;
  blackboard_entry(const T& value_) : value(value_) {}
  std::string to_string() const override { return value; }

  T get_value() const { return value; }
  auto set_value(const T& new_value) -> void { value = new_value; }
  std::string get_type() const { return typeid(T).name(); }
  auto to_string() -> std::string {
    try {
      return std::to_string(value);
    } catch (const std::exception& e) {
      std::cerr << e.what() << '\n';
      return get_type();
    }
  }
};

class blackboard {
 private:
  std::unordered_map<std::string, blackboard_entry_interface::ptr> entries;
  mutable std::recursive_mutex mtx;

 public:
  blackboard();
  blackboard(const blackboard&);
  ~blackboard();

  auto contains(const std::string& key) const -> bool;

  template <typename T>
  auto get(const std::string& key) const -> std::optional<T> {
    auto lock = std::lock_guard(mtx);
    if (!this->contains(key)) return std::nullopt;
    
    auto entry = std::dynamic_pointer_cast<blackboard_entry<T>>(entries.at(key));
    return entry ? std::make_optional(entry->get_value()) : std::nullopt;
  }
};

}  // namespace msm