#include <memory>
#include <iostream>
#include <vector>
#include "holoscan/holoscan.hpp"

using namespace std;

//global token count
namespace holoscan::conditions {
size_t tok_op1_op2 = 0;  
size_t tok_op1_op3 = 0;  
size_t tok_op2_op3 = 0;  
size_t tok_op3_op4 = 0; 
size_t tok_op4_op1 = 6;
}
void busy_loop(int iters) {
  int x = 0;
  for (int i = 0; i < iters; ++i) {
      x += i; }}
std::vector<int> feedback={0,0,0,0,0,0};//store delay token



namespace holoscan::conditions {

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
    spec.param(edge_id_,
               "edge_id",
               "Edge ID",
               "indicates edge");
    spec.param(min_tokens_,
               "min_tokens",
               "Minimum total tokens required",
               "Condition READY when queue + buffer ≥ min_tokens",
               static_cast<uint64_t>(1));
  }

  void check(int64_t, SchedulingStatusType* type,
             int64_t* target_timestamp) const override {
    *type = current_state_;
    *target_timestamp = last_state_change_;
  }

  void on_execute(int64_t timestamp) override {
    update_state(timestamp);
  }

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
    size_t tokens = 0;

    switch (edge_id_.get()) {

      case 1: tokens = tok_op1_op2; break;
      case 2: tokens = tok_op1_op3; break;   
      case 3: tokens = tok_op2_op3; break;   
      case 4: tokens = tok_op3_op4; break;
      case 5: tokens = tok_op4_op1; break;
      default: tokens = 0;
    }

    return tokens >= min_tokens_.get();
  }

  Parameter<std::shared_ptr<holoscan::Receiver>> receiver_;
  Parameter<int> edge_id_;
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
    spec.output<std::vector<int>>("out1");
    spec.output<std::vector<int>>("out2");
    spec.param(a1_, "a1", "Tokens out1", "Tokens produced on out1 per fire", 2);
    spec.param(a2_, "a2", "Tokens out2", "Tokens produced on out2 per fire", 1);
  }

  void compute(holoscan::InputContext&, holoscan::OutputContext& out,
               holoscan::ExecutionContext&) override {
    static int counter1 = 0;
    static int counter2 = 0;

 feedback.erase(feedback.begin());//consume one initial delay token
 tok_op4_op1 -= 1; 
  int cpu = sched_getcpu();
  pid_t tid = syscall(SYS_gettid);

//  std::cout << "[Op1] running on CPU " << cpu
  //          << " (tid=" << tid << ")" << std::endl;
    std::vector<int> b1;
    b1.reserve(a1_.get());
    for (int i = 0; i < a1_.get(); ++i)
      b1.push_back(++counter1);

    std::vector<int> b2;
    b2.reserve(a2_.get());
    for (int i = 0; i < a2_.get(); ++i)
      b2.push_back(++counter2);

   busy_loop(5e7);
    out.emit(b1, "out1");
    out.emit(b2, "out2");

    holoscan::conditions::tok_op1_op2 += a1_.get();
    holoscan::conditions::tok_op1_op3 += a2_.get();
//std::cout << holoscan::conditions::tok_op1_op3 << std::endl;
    std::cout << "[Op1] produces [out1]:";
    for (auto v : b1) std::cout << " " << v;
//     std::cout <<  std::endl;
      std::cout << "[Op1] produces [out2]:";
    for (auto v : b2) std::cout << " " << v;
     std::cout <<  std::endl;
  }

 private:
  holoscan::Parameter<int> a1_;
  holoscan::Parameter<int> a2_;
};


class Op2 : public holoscan::Operator {
 public:
  HOLOSCAN_OPERATOR_FORWARD_ARGS(Op2)
  Op2() = default;

  void setup(holoscan::OperatorSpec& spec) override {
    spec.input<std::vector<int>>("in").queue_size(2);
    spec.output<std::vector<int>>("out");

    spec.param(b1_, "b1", "Consume rate", "Tokens consumed per fire", 3);
    spec.param(a3_, "a3", "Produce rate", "Tokens produced per fire", 3);
  }

  void compute(holoscan::InputContext& in, holoscan::OutputContext& out,
               holoscan::ExecutionContext&) override {
    static std::vector<int> buffer;
 static int counter = 0;
  int cpu = sched_getcpu();
  pid_t tid = syscall(SYS_gettid);

 // std::cout << "[Op2] running on CPU " << cpu
  //          << " (tid=" << tid << ")" << std::endl;
    auto msg = in.receive<std::vector<int>>("in");
    while (msg) {
      buffer.insert(buffer.end(), msg->begin(), msg->end());
      msg = in.receive<std::vector<int>>("in");
    }
       std::cout << "[Op2] consumes:";
    if (buffer.size() >= static_cast<size_t>(b1_.get())) {

      for (int i = 0; i < b1_.get(); ++i) std::cout << " " << buffer[i];
  //    std::cout << std::endl;
      buffer.erase(buffer.begin(), buffer.begin() + b1_.get());
      holoscan::conditions::tok_op1_op2 -= b1_.get();

      std::vector<int> produced;
      produced.reserve(a3_.get());
      for (int i = 0; i < a3_.get(); ++i)
        produced.push_back(++counter);

     


      out.emit(produced, "out");    
  holoscan::conditions::tok_op2_op3 += produced.size();
//std::cout << holoscan::conditions::tok_op2_op3 << std::endl;
          std::cout << "[Op2] produces :";
    for (auto v : produced) std::cout << " " << v;
     std::cout <<  std::endl;

 busy_loop(5e7);


    }
  }

 private:
  holoscan::Parameter<int> b1_;
  holoscan::Parameter<int> a3_;
};


class Op3 : public holoscan::Operator {
 public:
  HOLOSCAN_OPERATOR_FORWARD_ARGS(Op3)
  Op3() = default;

  void setup(holoscan::OperatorSpec& spec) override {
    spec.input<std::vector<int>>("in2").queue_size(4);
    spec.input<std::vector<int>>("in3").queue_size(2);
    spec.output<std::vector<int>>("out");

    spec.param(b2_, "b2", "Consume in2", "Tokens consumed from in2", 2);
    spec.param(b3_, "b3", "Consume in3", "Tokens consumed from in3", 4);
    spec.param(a4_, "a4", "Produce out", "Tokens produced per fire", 1);
  }

  void compute(holoscan::InputContext& in, holoscan::OutputContext& out,
               holoscan::ExecutionContext&) override {
    static std::vector<int> buf2, buf3;
    static int counter = 0;
  int cpu = sched_getcpu();
  pid_t tid = syscall(SYS_gettid);

//  std::cout << "[Op3] running on CPU " << cpu
//            << " (tid=" << tid << ")" << std::endl;   
 auto m2 = in.receive<std::vector<int>>("in2");
    while (m2) {
      buf2.insert(buf2.end(), m2->begin(), m2->end());
      m2 = in.receive<std::vector<int>>("in2");
    }

    auto m3 = in.receive<std::vector<int>>("in3");
    while (m3) {
      buf3.insert(buf3.end(), m3->begin(), m3->end());
      m3 = in.receive<std::vector<int>>("in3");
    }
  std::cout << "[Op3] consumes [in1]:";
    if (buf2.size() >= static_cast<size_t>(b2_.get()) &&
        buf3.size() >= static_cast<size_t>(b3_.get())) {

for (int i = 0; i <  b2_.get(); ++i) std::cout << " " << buf2[i];
//std::cout << std::endl;
std::cout << "[Op3] consumes [in2]:";
for (int i = 0; i <  b3_.get(); ++i) std::cout << " " << buf3[i];
//std::cout << std::endl;
      buf2.erase(buf2.begin(), buf2.begin() + b2_.get());
      buf3.erase(buf3.begin(), buf3.begin() + b3_.get());

      holoscan::conditions::tok_op1_op3 -= b2_.get();
      holoscan::conditions::tok_op2_op3 -= b3_.get();

      std::vector<int> produced;
      produced.reserve(a4_.get());
      for (int i = 0; i < a4_.get(); ++i)
        produced.push_back(++counter);

      out.emit(produced, "out");
      holoscan::conditions::tok_op3_op4 += produced.size();

              std::cout << "[Op3] produces :";
    for (auto v : produced) std::cout << " " << v;
     std::cout <<  std::endl;
 busy_loop(5e7);
    }
  }

 private:
  holoscan::Parameter<int> b2_;
  holoscan::Parameter<int> b3_;
  holoscan::Parameter<int> a4_;
};

class Op4 : public holoscan::Operator {
 public:
  HOLOSCAN_OPERATOR_FORWARD_ARGS(Op4)
  Op4() = default;

  void setup(holoscan::OperatorSpec& spec) override {
    spec.input<std::vector<int>>("in").queue_size(2);
    spec.param(b4_, "b4", "Consume rate", "Tokens consumed per fire", 2);
  }

  void compute(holoscan::InputContext& in, holoscan::OutputContext&,
               holoscan::ExecutionContext&) override {
    static std::vector<int> buffer;
  int cpu = sched_getcpu();
  pid_t tid = syscall(SYS_gettid);

 // std::cout << "[Op4] running on CPU " << cpu
   //         << " (tid=" << tid << ")" << std::endl;

    auto msg = in.receive<std::vector<int>>("in");
    while (msg) {
      buffer.insert(buffer.end(), msg->begin(), msg->end());
      msg = in.receive<std::vector<int>>("in");
    }
      std::cout << "[Op4] consumes:";

    if (buffer.size() >= static_cast<size_t>(b4_.get())) {

      for (int i = 0; i <  b4_.get(); ++i) std::cout << " " << buffer[i];
      std::cout << std::endl;
      buffer.erase(buffer.begin(), buffer.begin() + b4_.get());
      holoscan::conditions::tok_op3_op4 -= b4_.get();
    for (int i = 0; i < 4; ++i) {
     feedback.push_back(0);
      tok_op4_op1+=1;
    }
   busy_loop(5e7);
    }
  }

 private:
  holoscan::Parameter<int> b4_;
};



class TokenSDFApp : public holoscan::Application {
 public:
  void compose() override {
    using namespace holoscan;
    auto cond1 = make_condition<conditions::BufferAwareCondition>(
      "cond1",
      Arg("receiver", "in1"),
      Arg("edge_id", 5),          // 
      Arg("min_tokens", uint64_t(1))  //
    );
    auto op1 = make_operator<Op1>(
      "op1",
      make_condition<CountCondition>("count", 96),
     cond1,
      Arg("a1", 2),   // Op1 → Op2 : a1 = 2
      Arg("a2", 1)    // Op1 → Op3 : a2 = 1
    );

    auto cond2 = make_condition<conditions::BufferAwareCondition>(
      "cond2",
      Arg("receiver", "in"),
      Arg("edge_id", 1),          // tok_op1_op2
      Arg("min_tokens", uint64_t(3))  // b1 = 3
    );

    auto op2 = make_operator<Op2>(
      "op2",
      cond2,
      Arg("b1", 3),   // consume 3
      Arg("a3", 3)    // produce 2
    );

    auto cond3_in2 = make_condition<conditions::BufferAwareCondition>(
      "cond3_in2",
      Arg("receiver", "in2"),
      Arg("edge_id", 2),                 
      Arg("min_tokens", uint64_t(2))     
    );

    // in3 来自 op2 → op3
    auto cond3_in3 = make_condition<conditions::BufferAwareCondition>(
      "cond3_in3",
      Arg("receiver", "in3"),
      Arg("edge_id", 3),                
      Arg("min_tokens", uint64_t(4))    
    );

    auto op3 = make_operator<Op3>(
      "op3",
      cond3_in2,
      cond3_in3,
      Arg("b2", 2),
      Arg("b3", 4),
      Arg("a4", 1)
    );

   
    auto cond4 = make_condition<conditions::BufferAwareCondition>(
      "cond4",
      Arg("receiver", "in"),
      Arg("edge_id", 4),          // tok_op3_op4
      Arg("min_tokens", uint64_t(2))  // b4 = 4
    );

    auto op4 = make_operator<Op4>(
      "op4",
      cond4,
      Arg("b4", 2)
    );


    add_flow(op1, op2, {{"out1", "in"}});
    add_flow(op1, op3, {{"out2", "in2"}});
    add_flow(op2, op3, {{"out",  "in3"}});
    add_flow(op3, op4, {{"out",  "in"}});

 }
};



int main() {
  
auto app = holoscan::make_application<TokenSDFApp>();


//    app->scheduler( app->make_scheduler<holoscan::EventBasedScheduler>( "event-scheduler", holoscan::Arg("worker_thread_number", int64_t(4)) ));

//app->scheduler( app->make_scheduler<holoscan::GreedyScheduler>("greedy"));

   app->scheduler(app->make_scheduler<holoscan::MultiThreadScheduler>("mts", holoscan::Arg("worker_thread_number", int64_t(4))  ));

  app->run();
  return 0;
}
