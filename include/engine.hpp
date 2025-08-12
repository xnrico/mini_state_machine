#pragma once

#include <atomic>
#include <memory>
#include <unordered_map>

#include "state.hpp"

namespace msm {
class msm_engine : public msm_state {
 public:
  using start_callback_t = std::function<void(blackboard::ptr, const std::string&, const std::vector<std::string>&)>;
  using end_callback_t = std::function<void(blackboard::ptr, const std::string&, const std::vector<std::string>&)>;
  using transition_callback_t = std::function<void(blackboard::ptr, const std::string&, const std::string&,
                                                   const std::string&, const std::vector<std::string>&)>;

 private:
  // State Map
  std::unordered_map<std::string, msm_state::ptr> states;

  // Transition Map
  std::unordered_map<std::string, std::unordered_map<std::string, std::string>> transitions;

  std::string initial_state;
  std::string current_state;

  std::unique_ptr<std::mutex> current_state_mtx;
  std::atomic<bool> is_valid;

  std::vector<std::pair<start_callback_t, std::vector<std::string>>>
      start_callbacks;  // executed before the state machine starts
  std::vector<std::pair<end_callback_t, std::vector<std::string>>>
      end_callbacks;  // executed after the state machine ends
  std::vector<std::pair<transition_callback_t, std::vector<std::string>>>
      transition_callbacks;  // executed on every state transition

 public:
  msm_engine(const std::unordered_set<std::string>& outcomes);
  msm_engine();
  ~msm_engine() override = default;

  auto add_state(const std::string& name, msm_state::ptr state,
                 const std::unordered_map<std::string, std::string>& transitions = {}) -> void;
  auto set_initial_state(const std::string& name) -> void;
  auto get_initial_state() const -> std::string;
  auto get_current_state() const -> std::string;
  auto get_states() const -> const std::unordered_map<std::string, msm_state::ptr>&;
  auto get_transitions(const std::string& state) const
      -> const std::unordered_map<std::string, std::unordered_map<std::string, std::string>>&;

  auto add_start_callback(start_callback_t callback, const std::vector<std::string>& args = {}) -> void;
  auto add_end_callback(end_callback_t callback, const std::vector<std::string>& args = {}) -> void;
  auto add_transition_callback(transition_callback_t callback, const std::vector<std::string>& args = {}) -> void;

  auto invoke_start_callbacks(blackboard::ptr bb, const std::string& initial_state) -> void;
  auto invoke_end_callbacks(blackboard::ptr bb, const std::string& outcome) -> void;  // outcome is the final outcome
  auto invoke_transition_callbacks(blackboard::ptr bb, const std::string& from_state, const std::string& to_state,
                                   const std::string& outcome) -> void;  // outcome is what triggered the transition

  auto validate(bool forced = false) -> void;

  using msm_state::execute;       // brings the base class execute method into scope to avoid hiding it
  auto execute() -> std::string;  // overloaded execute method in derived class, execute with default blackboard
  auto execute(blackboard::ptr bb) -> std::string override;  // execute method with blackboard parameter

  using msm_state::operator();       // brings the base class operator() into scope to avoid hiding it
  auto operator()() -> std::string;  // overloaded operator() in derived class

  auto cancel() -> void override;
  auto to_string() const -> std::string override;
};
}  // namespace msm