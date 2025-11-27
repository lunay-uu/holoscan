#include <memory>
#include <iostream>
#include "holoscan/holoscan.hpp"
class Op1 : public holoscan::Operator {
 public:
  HOLOSCAN_OPERATOR_FORWARD_ARGS(Op1)
  Op1() = default;

  void setup(holoscan::OperatorSpec& spec) override {
    spec.output<std::vector<int>>("out");
    spec.param(x_, "x", "Tokens per execution",  "Number of tokens emitted per fire", 1);
  }

  void compute(holoscan::InputContext&, holoscan::OutputContext& out,
               holoscan::ExecutionContext&) override {
    static int counter = 0;
   std::vector<int> batch;
 batch.reserve(x_.get());
for (int i = 0; i < x_.get(); ++i) {
     batch.push_back(++counter);	
     }
out.emit(batch, "out"); 
auto now = std::chrono::system_clock::now();
    auto t = std::chrono::system_clock::to_time_t(now);
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                  now.time_since_epoch()) %
              1000;
    std::tm local_tm = *std::localtime(&t);
  //  std::cout << std::put_time(&local_tm, "%H:%M:%S")
    //          << "." << std::setfill('0') << std::setw(3) << ms.count()
       std::cout     << "  Op1 emits ";
     for (auto v : batch) std::cout << v << " ";
    std::cout << std::endl;
  }

private:
  holoscan::Parameter<int> x_;
};

class Op2 : public holoscan::Operator {
 public:
  HOLOSCAN_OPERATOR_FORWARD_ARGS(Op2)
  Op2() = default;

  void setup(holoscan::OperatorSpec& spec) override {
    spec.input<int>("in") ;
 spec.param(y_, "y", "Tokens per execution",  "Number of tokens to consume per execution", 1);

  }


 void compute(holoscan::InputContext& in,
               holoscan::OutputContext&,
               holoscan::ExecutionContext&) override {
    static std::vector<int> buffer;
 std::cout<< "OP2 triggered"<< std::endl;
    //
    auto maybe_batch = in.receive<std::vector<int>>("in");
    while (maybe_batch) {
      auto& batch = maybe_batch.value();
      buffer.insert(buffer.end(), batch.begin(), batch.end());
      maybe_batch = in.receive<std::vector<int>>("in");
    }
 
    if (buffer.size() < static_cast<size_t>(y_.get())) {
      return;
    }

auto now = std::chrono::system_clock::now();
    auto t = std::chrono::system_clock::to_time_t(now);
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                  now.time_since_epoch()) % 1000;
    std::tm local_tm = *std::localtime(&t);
   // std::cout << std::put_time(&local_tm, "%H:%M:%S") << "."
           //   << std::setfill('0') << std::setw(3) << ms.count()
     std::cout         << "  Op2 consume:";
    for (int i = 0; i < y_.get(); ++i)
      std::cout << " " << buffer[i];
    std::cout << std::endl;

    buffer.erase(buffer.begin(), buffer.begin() + y_.get());
  }

 private:
  holoscan::Parameter<int> y_;
};
class TriggerApp : public holoscan::Application {
 public:
  void compose() override {
    using namespace holoscan;

   
       auto op1 = make_operator<Op1>(
        "op1",
        make_condition<CountCondition>("count_cond", 9),
        Arg("x", 2));
    
     auto op2 = make_operator<Op2>(
    "op2",
//    make_condition<PeriodicCondition>("periodic_2",  100.0),
    Arg("y", 3));
    add_flow(op1, op2, {{"out", "in"}});
  }
};


// 
int main() {
  auto app = holoscan::make_application<TriggerApp>();
  app->run();
  return 0;
}
