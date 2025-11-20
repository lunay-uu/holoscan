#include <memory>
#include <iostream>
#include "holoscan/holoscan.hpp"

using namespace holoscan;

namespace holoscan::conditions {

// 
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

} 
class Op1 : public Operator {
 public:
  HOLOSCAN_OPERATOR_FORWARD_ARGS(Op1)
  void setup(OperatorSpec& spec) override { spec.output<int>("out1"); }
  void compute(InputContext&, OutputContext& out, ExecutionContext&) override {
    static int c = 0;
    out.emit(++c, "out1");
    auto now = std::chrono::system_clock::now();
    auto t = std::chrono::system_clock::to_time_t(now);
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()) % 1000;
    std::tm local_tm = *std::localtime(&t);
    std::cout << std::put_time(&local_tm, "%H:%M:%S") << "."
              << std::setfill('0') << std::setw(3) << ms.count()
              << "  Op1 produces: " << c << std::endl;
  }
};

//
class Op2 : public Operator {
 public:
  HOLOSCAN_OPERATOR_FORWARD_ARGS(Op2)
  void setup(OperatorSpec& spec) override { spec.output<int>("out2"); }
  void compute(InputContext&, OutputContext& out, ExecutionContext&) override {
    static int c = 100;
    out.emit(++c, "out2");
    auto now = std::chrono::system_clock::now();
    auto t = std::chrono::system_clock::to_time_t(now);
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()) % 1000;
    std::tm local_tm = *std::localtime(&t);
    std::cout << std::put_time(&local_tm, "%H:%M:%S") << "."
              << std::setfill('0') << std::setw(3) << ms.count()
              << "  Op2 produces: " << c << std::endl;
  }
};

// 
class Op3 : public Operator {
 public:
  HOLOSCAN_OPERATOR_FORWARD_ARGS(Op3)
  void setup(OperatorSpec& spec) override {
    spec.input<int>("in1").queue_size(4).queue_policy(IOSpec::QueuePolicy::kPop);
    spec.input<int>("in2").queue_size(4).queue_policy(IOSpec::QueuePolicy::kPop);
  }
  void compute(InputContext& in, OutputContext&, ExecutionContext&) override {
    std::vector<int> a, b;
    for (int i = 0; i < 2; ++i) { auto x = in.receive<int>("in1"); if (x) a.push_back(x.value()); }
    for (int i = 0; i < 3; ++i) { auto y = in.receive<int>("in2"); if (y) b.push_back(y.value()); }
    auto now = std::chrono::system_clock::now();
    auto t = std::chrono::system_clock::to_time_t(now);
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()) % 1000;
    std::tm local_tm = *std::localtime(&t);
    std::cout << std::put_time(&local_tm, "%H:%M:%S") << "."
              << std::setfill('0') << std::setw(3) << ms.count()
              << "  Op3 consumes in1:"; for (auto v: a) std::cout << " " << v;
    std::cout << "  in2:"; for (auto v: b) std::cout << " " << v; std::cout << " ]\n";
  }
};

class App : public Application {
 public:
  void compose() override {
    using namespace holoscan;

    auto src_count   = make_condition<CountCondition>("src_count", 30);
 auto src_count1   = make_condition<CountCondition>("src_count1", 30);
    auto op1 = make_operator<Op1>("op1", make_condition<PeriodicCondition>("p1", 10000.0),src_count);
    auto op2 = make_operator<Op2>("op2", make_condition<PeriodicCondition>("p2", 10.0),src_count1);

    // 
    auto c_in1 = make_condition<conditions::NativeMessageAvailableCondition>(
        "c_in1", Arg("receiver", "in1"), Arg("min_size", static_cast<uint64_t>(2)));
    auto c_in2 = make_condition<conditions::NativeMessageAvailableCondition>(
        "c_in2", Arg("receiver", "in2"), Arg("min_size", static_cast<uint64_t>(3)));

    auto op3 = make_operator<Op3>("op3", c_in1, c_in2);

    add_flow(op1, op3, {{"out1", "in1"}});
    add_flow(op2, op3, {{"out2", "in2"}});
  }
};

int main() {
  auto app = make_application<App>();
  app->run();
  return 0;
}

