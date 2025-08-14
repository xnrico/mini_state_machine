#include "state.hpp"

#include <future>
#include <stdexcept>

namespace msm {

msm_state::msm_state(const std::unordered_set<std::string>& outcomes_)
    : active{false}, cancelled{false}, outcomes{outcomes_} {
  if (outcomes.empty()) {
    throw std::logic_error("State must have at least one outcome.");
  }
}

auto msm_state::operator()(blackboard::ptr bb) -> std::string {
  cancelled.store(false);
  active.store(true);
  auto outcome = execute(bb);

  if (outcomes.find(outcome) == outcomes.end()) {
    active.store(false);
    throw std::logic_error("Invalid outcome: " + outcome + " from state: " + to_string());
  }

  active.store(false);
  return outcome;
}

auto msm_state::cancel() -> void { cancelled.store(true); }

auto msm_state::is_active() const noexcept -> bool { return active.load(); }

auto msm_state::is_cancelled() const noexcept -> bool { return cancelled.load(); }

auto msm_state::get_outcomes() const noexcept -> const std::unordered_set<std::string>& { return outcomes; }

auto callback_state::execute(blackboard::ptr bb) -> std::string {
  if (!callback_func) {
    throw std::logic_error("Callback function is not set for callback_state.");
  }
  return callback_func(bb);
}

parallel_state::parallel_state(const std::unordered_set<msm_state::ptr>& states_, const std::string& default_outcome_,
                               const std::unordered_map<std::string, state_map>& outcome_map_)
    : msm_state{generate_outcomes(outcome_map_, default_outcome_)}, states{states_}, default_outcome(default_outcome_) {
  for (const auto& [outcome, prerequisites] : outcome_map_) {
    for (const auto& [state, intermediate_outcome] : prerequisites) {
      if (state->get_outcomes().find(intermediate_outcome) == state->get_outcomes().end()) {
        throw std::logic_error("State " + state->to_string() + " does not have outcome " + intermediate_outcome);
      }

      if (states.find(state) == states.end()) {
        throw std::logic_error("State " + state->to_string() + " is not part of the parallel_state.");
      }

      intermediate_outcomes.try_emplace(state, nullptr);
    }
  }
}

auto parallel_state::execute(blackboard::ptr bb) -> std::string {
  auto futures = std::vector<std::future<void>>{};

  for (const auto& state : states) {
    futures.emplace_back(std::async(std::launch::async, [this, &state, &bb]() -> void {
      auto outcome = state->execute(bb);
      auto lock = std::lock_guard(intermediate_mutex);
      intermediate_outcomes[state] = outcome;
    }));
  }

  for (auto& future : futures) {
    future.get();  // blocks until each task is done
  }

  if (cancelled.load()) {
    return default_outcome;
  }

  // Collect all satisfied outcomes from the intermediate outcomes
  auto all_outcomes = std::unordered_set<std::string>{};
  for (const auto& [outcome, prerequisites] : outcome_map) {
    // Check if all intermediate prerequisites for this outcome are met
    auto all_met = true;

    // Iterate through the prerequisites for this outcome
    for (const auto& [state, expected_outcome] : prerequisites) {
      auto it = intermediate_outcomes.find(state);
      if (it == intermediate_outcomes.end() || it->second != expected_outcome) {
        all_met = false;
        break;
      }
    }

    if (all_met) all_outcomes.insert(outcome);
  }

  if (all_outcomes.empty()) {
    return default_outcome;
  } else if (all_outcomes.size() == 1) {
    return std::move(*all_outcomes.begin());
  } else {
    throw std::logic_error("Multiple outcomes satisfied: " + std::to_string(all_outcomes.size()));
  }

  return default_outcome;  // Fallback if no outcomes are satisfied
}

auto parallel_state::cancel() -> void {
  for (const auto& state : states) {
    state->cancel();
  }
  cancelled.store(true);
}

auto parallel_state::generate_outcomes(const std::unordered_map<std::string, state_map>& outcome_map,
                                       const std::string& default_outcome) -> std::unordered_set<std::string> {
  auto outcomes = std::unordered_set<std::string>{default_outcome};

  for (const auto& [outcome, _] : outcome_map) {
    outcomes.insert(outcome);
  }

  return outcomes;
}

auto parallel_state::to_string() const -> std::string {
  std::string result = "Parallel State with outcomes: ";
  for (const auto& outcome : outcomes) {
    result += outcome + ", ";
  }
  result += "Default outcome: " + default_outcome;
  return result;
}

}  // namespace msm