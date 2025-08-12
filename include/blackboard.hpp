#pragma once

#ifdef __GNUG__     // If using GCC/G++
#include <cxxabi.h> // For abi::__cxa_demangle
#endif

#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <unordered_map>

namespace msm {
// Interface for a blackboard entry
class blackboard_entry_interface {
 public:
  using ptr = std::shared_ptr<blackboard_entry_interface>;
  virtual ~blackboard_entry_interface() = default;
  virtual auto to_string() const -> std::string = 0;
};

template <typename T>
class blackboard_entry : public blackboard_entry_interface {
 private:
  std::string value;

 public:
  blackboard_entry(const T& value_) : value(value_) {}

  auto get_value() const -> T { return value; }
  auto get_ref() -> T& { return value; }
  auto set_value(const T& new_value) -> void { value = new_value; }
  
  auto get_type() const -> std::string {
    auto type = typeid(T).name();
#ifdef __GNUG__  // If using GCC/G++
    int status;
    // Demangle the name using GCC's demangling function
    char* demangled = abi::__cxa_demangle(type.c_str(), nullptr, nullptr, &status);
    if (status == 0) {
      type = demangled;
    }
    free(demangled);
#endif
    return type;
  }
  
  auto to_string() const override -> std::string {
    try {
      return std::to_string(value);
    } catch (const std::exception& e) {
      std::cerr << e.what() << '\n';
      return "Object of Type [" + get_type() + "]";
    }
  }
};

// Blackboard class for storing key-value pairs
class blackboard final {
 private:
  std::unordered_map<std::string, blackboard_entry_interface::ptr> entries;
  mutable std::recursive_mutex mtx;

 public:
  using ptr = std::shared_ptr<blackboard>;

  blackboard(const blackboard&);
  blackboard() = default;
  ~blackboard() = default;

  auto contains(const std::string& key) const noexcept -> bool;
  auto remove(const std::string& key) noexcept -> void;
  auto size() const noexcept -> size_t;
  auto clear() noexcept -> void;
  auto serialize() const -> std::string;

  template <typename T>
  auto get(const std::string& key) const -> std::optional<T> {
    auto lock = std::lock_guard(mtx);
    if (!this->contains(key)) return std::nullopt;

    auto entry = std::dynamic_pointer_cast<blackboard_entry<T>>(entries.at(key));
    return entry ? std::make_optional(entry->get_value()) : std::nullopt;
  }

  template <typename T>
  auto set(const std::string& key, const T& value) -> void {
    auto lock = std::lock_guard(mtx);
    if (!this->contains(key)) {
      entries[key] = std::make_shared<blackboard_entry<T>>(value);
    } else {
      auto entry = std::dynamic_pointer_cast<blackboard_entry<T>>(entries.at(key));
      entry->set_value(value);
    }
  }

  template <typename T>
  auto operator[](const std::string& key) -> T& {
    auto lock = std::lock_guard(mtx);
    if (!this->contains(key)) {
      entries[key] = std::make_shared<blackboard_entry<T>>(T{});
    }
    auto entry = std::dynamic_pointer_cast<blackboard_entry<T>>(entries.at(key));
    if (!entry) {
      throw std::runtime_error("Type mismatch for key: " + key);
    }
    return entry->get_ref();
  }
};

}  // namespace msm