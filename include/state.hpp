#pragma once

#include <atomic>
#include <functional>
#include <mutex>
#include <unordered_set>

#include "blackboard.hpp"

namespace msm {

class msm_state {
 private:
  std::atomic<bool> active;
  std::atomic<bool> cancelled;

 protected:
  std::unordered_set<std::string> outcomes;

 public:
  using ptr = std::shared_ptr<msm_state>;
  msm_state(const std::unordered_set<std::string>& outcomes_) : active{false}, cancelled{false}, outcomes{outcomes_} {}
  msm_state() : msm_state(std::unordered_set<std::string>{}) {}
  virtual ~msm_state() = default;

  auto operator()(blackboard::ptr bb) -> std::string;

  virtual auto execute(blackboard::ptr bb) -> std::string = 0;
  virtual auto to_string() const -> std::string = 0;
  virtual auto cancel() -> void;

  auto is_active() const noexcept -> bool;
  auto is_cancelled() const noexcept -> bool;
  auto get_outcomes() const noexcept -> const std::unordered_set<std::string>&;
};

class callback_state : public msm_state {
 private:
  std::function<std::string(blackboard::ptr)> callback_func;

 public:
  callback_state(const std::function<std::string(blackboard::ptr)>& func,
                 const std::unordered_set<std::string>& outcomes_)
      : msm_state(outcomes_), callback_func{func} {}
  callback_state(const std::function<std::string(blackboard::ptr)>& func)
      : msm_state(std::unordered_set<std::string>{}), callback_func{func} {}
  ~callback_state() override = default;

  auto execute(blackboard::ptr bb) -> std::string override;
};

class parallel_state : public msm_state {
 private:
  std::atomic<bool> active;     // this hides the variable from the base class
  std::atomic<bool> cancelled;  // this hides the variable from the base class

  std::mutex intermediate_mutex;  // mutex for intermediate outcomes

 protected:
  using state_map = std::unordered_map<msm_state::ptr, std::string>;

  const std::unordered_set<msm_state::ptr> states;
  const std::string default_outcome;

  state_map state_outcomes;  // map from state to its outcome
  std::unordered_map<std::string, state_map>
      outcome_map;  // map from outcome to (map from state to its expected outcome)

  std::unordered_map<msm_state::ptr, std::string>
      intermediate_outcomes;                 // map from state to its actual outcome after calling state->execute()
  std::unordered_set<std::string> outcomes;  // set of all outcomes

  static auto generate_outcomes(const std::unordered_map<std::string, state_map>& outcome_map,
                                const std::string& default_outcome) -> std::unordered_set<std::string>;

 public:
  parallel_state(const std::unordered_set<msm_state::ptr>& states_, const std::string& default_outcome_,
                 const std::unordered_map<std::string, state_map>& outcome_map_);

  auto execute(blackboard::ptr bb) -> std::string override;
  auto cancel() -> void override;
  auto to_string() const -> std::string override;
};

}  // namespace msm