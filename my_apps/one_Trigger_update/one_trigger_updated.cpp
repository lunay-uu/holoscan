#include <memory>
#include <iostream>
#include "holoscan/holoscan.hpp"

namespace holoscan::conditions {

// add condition
// op1 fire every 1 cycle, op2 fires every 3 cycles
class NativeMessageAvailableCondition : public Condition {
 public:
  HOLOSCAN_CONDITION_FORWARD_ARGS(NativeMessageAvailableCondition)
  NativeMessageAvailableCondition() = default;

  void initialize() override { Condition::initialize(); }

  void setup(ComponentSpec& spec) override {
    spec.param(receiver_,
               "receiver",
               "Receiver",
               "Monitored input channel for message availability.");
    spec.param(min_size_,
               "min_size",
               "Minimum size",
               "Number of messages required to trigger execution",
               static_cast<uint64_t>(1));
  }

  void check(int64_t, SchedulingStatusType* type, int64_t* target_timestamp) const override {
    *type = current_state_;
    *target_timestamp = last_state_change_;
  }

  void on_execute(int64_t timestamp) override { update_state(timestamp); }

  void update_state(int64_t timestamp) override {
    const bool is_ready = check_min_size();
    if (is_ready && current_state_ != SchedulingStatusType::kReady) {
      current_state_ = SchedulingStatusType::kReady;
      last_state_change_ = timestamp;
    } else if (!is_ready && current_state_ != SchedulingStatusType::kWait) {
      current_state_ = SchedulingStatusType::kWait;
      last_state_change_ = timestamp;
    }
  }

 private:
  bool check_min_size() const {
    auto recv = receiver_.get();
    if (!recv) return false;
    return recv->back_size() + recv->size() >= min_size_.get();
  }

  Parameter<std::shared_ptr<holoscan::Receiver>> receiver_;
  Parameter<uint64_t> min_size_;

  mutable SchedulingStatusType current_state_ = SchedulingStatusType::kWait;
  mutable int64_t last_state_change_ = 0;
};

}  // namespace holoscan::conditions

class Op1 : public holoscan::Operator {
 public:
  HOLOSCAN_OPERATOR_FORWARD_ARGS(Op1)
  Op1() = default;

  void setup(holoscan::OperatorSpec& spec) override {
    spec.output<int>("out");
  }

  void compute(holoscan::InputContext&, holoscan::OutputContext& out,
               holoscan::ExecutionContext&) override {
    static int counter = 0;
    counter++;
    std::cout << "Op1 emits: " << counter << std::endl;
    out.emit(counter, "out");
  }
};


// fire every 3 op1 cycle
class Op2 : public holoscan::Operator {
 public:
  HOLOSCAN_OPERATOR_FORWARD_ARGS(Op2)
  Op2() = default;

  void setup(holoscan::OperatorSpec& spec) override {
    spec.input<int>("in")
        .queue_size(3)
        .queue_policy(holoscan::IOSpec::QueuePolicy::kPop);
  }

  void compute(holoscan::InputContext& in, holoscan::OutputContext&, holoscan::ExecutionContext&) override {
    std::vector<int> buffer;
    while (auto msg = in.receive<int>("in")) {
      buffer.push_back(msg.value());
    }
    std::cout << "Op2 received: " ;
    for (auto v : buffer) std::cout << v << " ";
    std::cout << std::endl;
  }
};


// 
class TriggerApp : public holoscan::Application {
 public:
  void compose() override {
    using namespace holoscan;

   
    auto op1 = make_operator<Op1>("op1", make_condition<CountCondition>("count_cond", 9));

    auto cond = make_condition<conditions::NativeMessageAvailableCondition>(
        "native_msg_cond",
        Arg("receiver", "in"),
        Arg("min_size", static_cast<uint64_t>(3)));

    auto op2 = make_operator<Op2>("op2", cond);

    add_flow(op1, op2, {{"out", "in"}});
  }
};


// 
int main() {
  auto app = holoscan::make_application<TriggerApp>();
  app->run();
  return 0;
}

