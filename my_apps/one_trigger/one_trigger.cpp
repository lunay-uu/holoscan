#include <holoscan/holoscan.hpp>
#include <iostream>
#include <vector>

//op1 op2 fire every cycle
class Op1 : public holoscan::Operator {
 public:
  HOLOSCAN_OPERATOR_FORWARD_ARGS(Op1)
  Op1() = default;

  void setup(holoscan::OperatorSpec& spec) override {
    spec.output<int>("out");
  }

  void compute(holoscan::InputContext&, holoscan::OutputContext& out, holoscan::ExecutionContext&) override {
    static int count = 0;
    count++;
    std::cout << "Op1： " << count << std::endl;
    out.emit(count, "out");
  }
};

// 
class Op2 : public holoscan::Operator {
 public:
  HOLOSCAN_OPERATOR_FORWARD_ARGS(Op2)
  Op2() = default;

  void setup(holoscan::OperatorSpec& spec) override {
    spec.input<int>("in");
  }

   void compute(holoscan::InputContext& in,
               holoscan::OutputContext&,
               holoscan::ExecutionContext&) override {
    auto msg = in.receive<int>("in");
    if (!msg) return;

    buffer_.push_back(msg.value());

    if (buffer_.size() == 3) {  // 每收到 3 个 token 才打印一次
      std::cout << "[Op2] receives 3 tokens: ";
      for (auto v : buffer_) std::cout << v << " ";
      std::cout << std::endl;
      buffer_.clear();
    }
  }

 private:
  std::vector<int> buffer_;
};

// ---------------- Application ----------------
class TriggerApp : public holoscan::Application {
 public:
  void compose() override {
    using namespace holoscan;

    
    auto op1 = make_operator<Op1>( "op1", make_condition<CountCondition>(9) );

    auto op2 = make_operator<Op2>("op2");
    add_flow(op1, op2, {{"out", "in"}});
  }
};

int main(int argc, char** argv) {
  auto app = holoscan::make_application<TriggerApp>();
  app->run();
  return 0;
}

