#include <memory>
#include <iostream>
#include "holoscan/holoscan.hpp"

using namespace std;

// ========= 全局 Token 计数（非原子版）=========
namespace holoscan::conditions {
size_t g_buffered_tokens = 0;
}

namespace holoscan::conditions {

// op1 produce x tokens per fire
// op2 consumes y tokens per fire
// Condition: READY if (queue + buffer) >= y
class BufferAwareCondition : public Condition {
 public:
  HOLOSCAN_CONDITION_FORWARD_ARGS(BufferAwareCondition)
  BufferAwareCondition() = default;

  void initialize() override {
    Condition::initialize();
    current_state_ = SchedulingStatusType::kWait;
    last_state_change_ = 0;
  }

  void setup(ComponentSpec& spec) override {
    spec.param(receiver_,
               "receiver",
               "Receiver",
               "Input channel to monitor for message availability.");
    spec.param(min_tokens_,
               "min_tokens",
               "Minimum total tokens required",
               "Condition READY when queue + buffer ≥ min_tokens",
               static_cast<uint64_t>(1));
  }

  void check(int64_t, SchedulingStatusType* type, int64_t* target_timestamp) const override {
    *type = current_state_;
    *target_timestamp = last_state_change_;
  }

  void on_execute(int64_t timestamp) override { update_state(timestamp); }

  void update_state(int64_t timestamp) override {
    const bool ready = check_ready();
    if (ready && current_state_ != SchedulingStatusType::kReady) {
      current_state_ = SchedulingStatusType::kReady;
      last_state_change_ = timestamp;
    } else if (!ready && current_state_ != SchedulingStatusType::kWait) {
      current_state_ = SchedulingStatusType::kWait;
      last_state_change_ = timestamp;
    }
  }

 private:
  bool check_ready() const {
    auto recv = receiver_.get();
    if (!recv) return false;
if (holoscan::conditions::g_buffered_tokens < min_tokens_.get())
    return false;
    const size_t queue_tokens = recv->back_size() + recv->size();
    const size_t total_tokens = queue_tokens + holoscan::conditions::g_buffered_tokens;

  //  std::cout << "[COND] queue=" << queue_tokens
   //           << " buffer=" << holoscan::conditions::g_buffered_tokens
 //             << " total=" << total_tokens
              //<< (total_tokens >= min_tokens_.get() ? " → READY" : " → WAIT")
            //  << std::endl;

    return total_tokens >= min_tokens_.get();
  }

  Parameter<std::shared_ptr<holoscan::Receiver>> receiver_;
  Parameter<uint64_t> min_tokens_;

  mutable SchedulingStatusType current_state_ = SchedulingStatusType::kWait;
  mutable int64_t last_state_change_ = 0;
};

}  

class Op1 : public holoscan::Operator {
 public:
  HOLOSCAN_OPERATOR_FORWARD_ARGS(Op1)
  Op1() = default;

  void setup(holoscan::OperatorSpec& spec) override {
    spec.output<std::vector<int>>("out");
    spec.param(x_, "x", "Tokens per execution", "Number of tokens emitted per fire", 2);
  }

  void compute(holoscan::InputContext&, holoscan::OutputContext& out,
               holoscan::ExecutionContext&) override {
    static int counter = 0;
    std::vector<int> batch;
    batch.reserve(x_.get());
    for (int i = 0; i < x_.get(); ++i)
      batch.push_back(++counter);

    out.emit(batch, "out");
   holoscan::conditions::g_buffered_tokens += x_.get(); 

    std::cout << "Op1 emits:";
    for (auto v : batch) std::cout << " " << v;
    std::cout <<  std::endl;
  }

 private:
  holoscan::Parameter<int> x_;
};


// ========= Op2：每次消耗 y 个 token =========
class Op2 : public holoscan::Operator {
 public:
  HOLOSCAN_OPERATOR_FORWARD_ARGS(Op2)
  Op2() = default;

  void setup(holoscan::OperatorSpec& spec) override {
    spec.input<std::vector<int>>("in").queue_size(64);
    spec.param(y_, "y", "Tokens per execution", "Tokens to consume per fire", 3);
  }

  void compute(holoscan::InputContext& in, holoscan::OutputContext&, holoscan::ExecutionContext&) override {
    static std::vector<int> buffer;
 std::cout << "Op2 triggered " << std::endl;;
    auto maybe_batch = in.receive<std::vector<int>>("in");
    while (maybe_batch) {
      auto& batch = maybe_batch.value();
      buffer.insert(buffer.end(), batch.begin(), batch.end());
      maybe_batch = in.receive<std::vector<int>>("in");
    }

    if (buffer.size() >= static_cast<size_t>(y_.get())) {
      std::cout << "Op2 consumes:";
      for (int i = 0; i < y_.get(); ++i) std::cout << " " << buffer[i];
      std::cout << std::endl;

      buffer.erase(buffer.begin(), buffer.begin() + y_.get());
      holoscan::conditions::g_buffered_tokens -= y_.get();   
    }
  }

 private:
  holoscan::Parameter<int> y_;
};


// ========= Application =========
class TokenSDFApp : public holoscan::Application {
 public:
  void compose() override {
    using namespace holoscan;

    auto op1 = make_operator<Op1>(
        "op1",
        make_condition<CountCondition>("count_cond", 9),
        Arg("x", 2));

//    extern size_t g_buffered_tokens;  // 声明

auto cond = make_condition<conditions::BufferAwareCondition>(
    "buf_cond",
    Arg("receiver", "in"),
    Arg("min_tokens", static_cast<uint64_t>(3)));

    auto op2 = make_operator<Op2>("op2", cond, Arg("y", 3));

    add_flow(op1, op2, {{"out", "in"}});
  }
};


// ========= Main =========
int main() {
  auto app = holoscan::make_application<TokenSDFApp>();
  app->run();
  return 0;
}

