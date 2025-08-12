#include "engine.hpp"

#include <sstream>
#include <stdexcept>

namespace msm {
msm_engine::msm_engine(const std::unordered_set<std::string>& outcomes) : msm_state{outcomes}, is_valid{false} {
  current_state_mtx = std::make_unique<std::mutex>();
}

msm_engine::msm_engine() : msm_engine(std::unordered_set<std::string>{}) {}

auto msm_engine::add_state(const std::string& name, msm_state::ptr state,
                           const std::unordered_map<std::string, std::string>& transitions_) -> void {
  if (states.find(name) != states.end()) return;      // State already exists, do not overwrite
  if (outcomes.find(name) != outcomes.end()) return;  // State name collides with an outcome name

  for (const auto& [source, target] : transitions_) {
    if (source.empty() || target.empty())
      throw std::invalid_argument("Transition source and target names cannot be empty strings.");
    if (state->get_outcomes().find(source) == state->get_outcomes().end())
      throw std::invalid_argument("State " + name + " references invalid outcome");
  }

  this->states.try_emplace(name, state);
  this->transitions.try_emplace(name, transitions_);

  if (this->initial_state.empty()) this->initial_state = name;  // Set the first added state as initial state by default

  this->is_valid.store(false);  // Invalidate the state machine
}

auto msm_engine::set_initial_state(const std::string& name) -> void {
  if (states.find(name) == states.end())
    throw std::invalid_argument("Cannot set initial state to '" + name + "': state not found in state machine.");

  this->initial_state = name;
  this->is_valid.store(false);  // Invalidate the state machine
}

auto msm_engine::get_initial_state() const -> std::string { return this->initial_state; }

auto msm_engine::get_current_state() const -> std::string {
  std::lock_guard<std::mutex> lock{*this->current_state_mtx};
  return this->current_state;
}

auto msm_engine::get_states() const -> const std::unordered_map<std::string, msm_state::ptr>& { return this->states; }

auto msm_engine::get_transitions(const std::string& state) const
    -> const std::unordered_map<std::string, std::unordered_map<std::string, std::string>>& {
  return this->transitions;
}

auto msm_engine::add_start_callback(start_callback_t callback, const std::vector<std::string>& args) -> void {
  this->start_callbacks.emplace_back(callback, args);
}

auto msm_engine::add_end_callback(end_callback_t callback, const std::vector<std::string>& args) -> void {
  this->end_callbacks.emplace_back(callback, args);
}

auto msm_engine::add_transition_callback(transition_callback_t callback, const std::vector<std::string>& args) -> void {
  this->transition_callbacks.emplace_back(callback, args);
}

auto msm_engine::invoke_start_callbacks(blackboard::ptr bb, const std::string& initial_state) -> void {
  try {
    for (const auto& [callback, args] : this->start_callbacks) {
      callback(bb, initial_state, args);
    }
  } catch (const std::exception& e) {
    throw std::runtime_error(std::string("Error invoking start callbacks: ") + e.what());
  }
}

auto msm_engine::invoke_end_callbacks(blackboard::ptr bb, const std::string& outcome) -> void {
  try {
    for (const auto& [callback, args] : this->end_callbacks) {
      callback(bb, outcome, args);
    }
  } catch (const std::exception& e) {
    throw std::runtime_error(std::string("Error invoking end callbacks: ") + e.what());
  }
}

auto msm_engine::invoke_transition_callbacks(blackboard::ptr bb, const std::string& from_state,
                                             const std::string& to_state, const std::string& outcome) -> void {
  try {
    for (const auto& [callback, args] : this->transition_callbacks) {
      callback(bb, from_state, to_state, outcome, args);
    }
  } catch (const std::exception& e) {
    throw std::runtime_error(std::string("Error invoking transition callbacks: ") + e.what());
  }
}

auto msm_engine::validate(bool forced = false) -> void {
  if (!forced && this->is_valid.load()) return;

  // Check that initial state is set and valid
  if (this->initial_state.empty() || this->states.find(this->initial_state) == this->states.end()) {
    throw std::runtime_error("State machine validation failed: initial state is not set or invalid.");
  }

  auto final_outcomes = std::unordered_set<std::string>{};
  for (const auto& [name, state] : this->states) {
    const auto& state_transitions = this->transitions[name];
    const auto& state_outcomes = state->get_outcomes();

    if (forced) {
      for (const auto& out : state_outcomes) {
        if (transitions.find(out) == transitions.end() &&
            this->get_outcomes().find(out) == this->get_outcomes().end()) {
          throw std::runtime_error("State machine validation failed: outcome '" + out + "' of state '" + name +
                                   "' is neither a valid transition nor a final outcome.");
        } else if (this->get_outcomes().find(out) != this->get_outcomes().end()) {
          final_outcomes.insert(out);  // outcome is a final outcome of the state machine
        }
      }
    }

    // recursively validate nested states if they are state machines
    if (std::dynamic_pointer_cast<msm_engine>(state)) {
      std::dynamic_pointer_cast<msm_engine>(state)->validate(forced);
    }

    for (const auto& [_, target_state] : state_transitions) {
      final_outcomes.insert(target_state);  // outcome is a transition to another state
    }
  }

  auto msm_outcome_set = std::unordered_set<std::string>{this->get_outcomes().begin(), this->get_outcomes().end()};
  for (const auto& out : final_outcomes) {
    if (msm_outcome_set.find(out) == msm_outcome_set.end()) {
      throw std::runtime_error("State machine validation failed: outcome '" + out +
                               "' is neither a valid transition nor a final outcome of the state machine.");
    }
  }

  this->is_valid.store(true);  // Mark the state machine as valid
}

}  // namespace msm