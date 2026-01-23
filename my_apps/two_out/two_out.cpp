#include <iostream>
#include <chrono>
#include <iomanip>
#include "holoscan/holoscan.hpp"

using namespace holoscan;

// op2 consume n token and fire every n cycle
class NativeMessageAvailableCondition : public Condition {
 public:
  HOLOSCAN_CONDITION_FORWARD_ARGS(NativeMessageAvailableCondition)
  NativeMessageAvailableCondition() = default;

  void initialize() override { Condition::initialize(); }

  void setup(ComponentSpec& spec) override {
    spec.param(receiver_, "receiver", "Receiver",
               "Monitored input channel for message availability.");
    spec.param(min_size_, "min_size", "Minimum size",
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


class Op1 : public Operator {
 public:
  HOLOSCAN_OPERATOR_FORWARD_ARGS(Op1)
  void setup(OperatorSpec& spec) override {
    spec.output<int>("out1");
    spec.output<int>("out2");
  }

  void compute(InputContext&, OutputContext& out, ExecutionContext&) override {
    static int c1 = 0, c2 = 100;
    out.emit(++c1, "out1");
    out.emit(++c2, "out2");

    auto now = std::chrono::system_clock::now();
    auto t = std::chrono::system_clock::to_time_t(now);
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                  now.time_since_epoch()) % 1000;
    std::tm local_tm = *std::localtime(&t);
    std::cout << std::put_time(&local_tm, "%H:%M:%S") << "."
              << std::setfill('0') << std::setw(3) << ms.count()
              << "  Op1 produces out1=" << c1 << " out2=" << c2 << std::endl;
  }
};


class Op2 : public Operator {
 public:
  HOLOSCAN_OPERATOR_FORWARD_ARGS(Op2)
  void setup(OperatorSpec& spec) override {
    spec.input<int>("in1").queue_size(10).queue_policy(IOSpec::QueuePolicy::kPop);
    spec.param(x_, "x", "Tokens per execution", "Number of tokens read per fire", 1);
  }

  void compute(InputContext& in, OutputContext&, ExecutionContext&) override {
    std::vector<int> buffer;
    for (int i = 0; i < x_.get(); ++i) {
      auto msg = in.receive<int>("in1");
      if (!msg) break;
      buffer.push_back(msg.value());
    }

    auto now = std::chrono::system_clock::now();
    auto t = std::chrono::system_clock::to_time_t(now);
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                  now.time_since_epoch()) % 1000;
    std::tm local_tm = *std::localtime(&t);
    std::cout << std::put_time(&local_tm, "%H:%M:%S") << "."
              << std::setfill('0') << std::setw(3) << ms.count()
              << "  Op2 consumes: ";
    for (auto v : buffer) std::cout << v << " ";
    std::cout << std::endl;
  }

 private:
  Parameter<int> x_;
};


class Op3 : public Operator {
 public:
  HOLOSCAN_OPERATOR_FORWARD_ARGS(Op3)
  void setup(OperatorSpec& spec) override {
    spec.input<int>("in2").queue_size(10).queue_policy(IOSpec::QueuePolicy::kPop);
    spec.param(y_, "y", "Tokens per execution", "Number of tokens read per fire", 1);
  }

  void compute(InputContext& in, OutputContext&, ExecutionContext&) override {
    std::vector<int> buffer;
    for (int i = 0; i < y_.get(); ++i) {
      auto msg = in.receive<int>("in2");
      if (!msg) break;
      buffer.push_back(msg.value());
    }

    auto now = std::chrono::system_clock::now();
    auto t = std::chrono::system_clock::to_time_t(now);
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                  now.time_since_epoch()) % 1000;
    std::tm local_tm = *std::localtime(&t);
    std::cout << std::put_time(&local_tm, "%H:%M:%S") << "."
              << std::setfill('0') << std::setw(3) << ms.count()
              << "  Op3 consumes: ";
    for (auto v : buffer) std::cout << v << " ";
    std::cout << std::endl;
  }

 private:
  Parameter<int> y_;
};


class TokenApp : public Application {
 public:
  void compose() override {
    using namespace holoscan;

    auto op1 = make_operator<Op1>("op1", make_condition<CountCondition>("count_cond", 12));

    auto cond1 = make_condition<NativeMessageAvailableCondition>(
        "cond_in1", Arg("receiver", "in1"), Arg("min_size", static_cast<uint64_t>(2)));
    auto cond2 = make_condition<NativeMessageAvailableCondition>(
        "cond_in2", Arg("receiver", "in2"), Arg("min_size", static_cast<uint64_t>(3)));

    auto op2 = make_operator<Op2>("op2", cond1, Arg("x", 3));
    auto op3 = make_operator<Op3>("op3", cond2, Arg("y", 2));

    add_flow(op1, op2, {{"out1", "in1"}});
    add_flow(op1, op3, {{"out2", "in2"}});
  }
};


int main() {
  auto app = make_application<TokenApp>();
  app->run();
  return 0;
}

