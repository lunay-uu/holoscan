#include <memory>
#include <iostream>
#include <vector>
#include "holoscan/holoscan.hpp"
#include <mutex>
using namespace std;

//global token count
namespace holoscan::conditions {
std::atomic<size_t> tok_op1_op2 (0);  
std::atomic<size_t> tok_op1_op3 (0);  
std::atomic<size_t> tok_op2_op3 (0);  
std::atomic<size_t> tok_op3_op4 (0); 
std::atomic<size_t> tok_op4_op1 (6);
}
void busy_loop(int iters) {
  int x = 0;
  for (int i = 0; i < iters; ++i) {
      x += i; }}
std::vector<int> feedback={0,0,0,0,0,0};//store delay token
std::mutex feedback_mtx; 


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
    //If no receiver here and do the evaluation only based on the tokens, the condition will not be triggered by events. And may lead to unexpected
    //quitting of the app. 
    spec.param(edge_id_,
               "edge_id",
               "Edge ID",
               "indicates edge");
    spec.param(min_tokens_,
               "min_tokens",
               "Minimum total tokens required",
               "Condition READY when queue + buffer ≥ min_tokens",
               static_cast<uint64_t>(1));
             spec.param(tokens_per_msg,
               "tokens_per_msg",
               "tokens_per_msg",
               "tokens_per_msg",
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

  case 1: tokens = tok_op1_op2.load(std::memory_order_relaxed); break;
  case 2: tokens = tok_op1_op3.load(std::memory_order_relaxed); break;
  case 3: tokens = tok_op2_op3.load(std::memory_order_relaxed); break;
  case 4: tokens = tok_op3_op4.load(std::memory_order_relaxed); break;
  case 5: tokens = tok_op4_op1.load(std::memory_order_relaxed); break;
    }
   size_t queue_tokens = 0;
    auto recv = receiver_.get();
    if (!recv) return false;
    queue_tokens = (recv->back_size() + recv->size()) * tokens_per_msg.get();


    return (tokens+ queue_tokens) >= min_tokens_.get();
  }

  Parameter<std::shared_ptr<holoscan::Receiver>> receiver_;
  Parameter<int> edge_id_;
  Parameter<uint64_t> min_tokens_;
 Parameter<uint64_t>tokens_per_msg;

  mutable SchedulingStatusType current_state_ = SchedulingStatusType::kWait;
  mutable int64_t last_state_change_ = 0;
};

}  
class Op0 : public holoscan::Operator {
 public:
  HOLOSCAN_OPERATOR_FORWARD_ARGS(Op0)
  Op0() = default;

  void setup(holoscan::OperatorSpec& spec) override {
    spec.output<int>("out");
  }

  void compute(holoscan::InputContext&, holoscan::OutputContext& out,
               holoscan::ExecutionContext&) override {
    static int counter = 0;
 
  out.emit(counter++, "out"); 

  }
};


class Op1 : public holoscan::Operator {
 public:
  HOLOSCAN_OPERATOR_FORWARD_ARGS(Op1)
  Op1() = default;

  void setup(holoscan::OperatorSpec& spec) override {
  
    spec.input<int>("in");
    spec.output<std::vector<int>>("out1");
    spec.output<std::vector<int>>("out2");
    spec.param(a1_, "a1", "Tokens out1", "Tokens produced on out1 per fire", 2);
    spec.param(a2_, "a2", "Tokens out2", "Tokens produced on out2 per fire", 1);
   
    
  }

  void compute(holoscan::InputContext& in, holoscan::OutputContext& out,
               holoscan::ExecutionContext&) override {
    static int counter1 = 0;
    static int counter2 = 0;



  auto msg = in.receive<int>("in");
  while(msg){
//holoscan::conditions::tok_op4_op1 += 1;
    msg = in.receive<int>("in");
  }
    feedback_mtx.lock();
  //   feedback.erase(feedback.begin());//
holoscan::conditions::tok_op4_op1.fetch_sub(1);
    feedback_mtx.unlock();

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



    std::cout << "[Op1] produces [out1to2]:";
    for (auto v : b1) std::cout << " " << v;
      std::cout << "[Op1] produces [out1to3]:";
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

    auto msg = in.receive<std::vector<int>>("in");
    while (msg) {
      buffer.insert(buffer.end(), msg->begin(), msg->end());
     holoscan::conditions:: tok_op1_op2.fetch_add(msg->size());
      msg = in.receive<std::vector<int>>("in");
    }

    if (buffer.size() >= static_cast<size_t>(b1_.get())) {
       std::cout << "[Op2] consumes:";
      for (int i = 0; i < b1_.get(); ++i) std::cout << " " << buffer[i];
      buffer.erase(buffer.begin(), buffer.begin() + b1_.get());
  
  
      std::vector<int> produced;
      produced.reserve(a3_.get());
      for (int i = 0; i < a3_.get(); ++i)
        produced.push_back(++counter);
      out.emit(produced, "out");    

          std::cout << "[Op2] produces :";
    for (auto v : produced) std::cout << " " << v;
     std::cout <<  std::endl;
    busy_loop(5e7);
   
    holoscan::conditions:: tok_op1_op2.fetch_sub(b1_.get());

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
    spec.input<std::vector<int>>("in2").queue_size(3);
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

 auto m2 = in.receive<std::vector<int>>("in2");
    while (m2) {
      buf2.insert(buf2.end(), m2->begin(), m2->end());
      holoscan::conditions::tok_op1_op3.fetch_add(m2->size());
      m2 = in.receive<std::vector<int>>("in2");
    }

    auto m3 = in.receive<std::vector<int>>("in3");
    while (m3) {
      buf3.insert(buf3.end(), m3->begin(), m3->end());
        holoscan::conditions::tok_op2_op3.fetch_add(m3->size());
      m3 = in.receive<std::vector<int>>("in3");
    }
  std::cout << "[Op3] consumes [in1to3]:";
    if (buf2.size() >= static_cast<size_t>(b2_.get()) &&
        buf3.size() >= static_cast<size_t>(b3_.get())) {

for (int i = 0; i <  b2_.get(); ++i) std::cout << " " << buf2[i];
//std::cout << std::endl;
std::cout << "[Op3] consumes [in2to3]:";
for (int i = 0; i <  b3_.get(); ++i) std::cout << " " << buf3[i];
//std::cout << std::endl;
      buf2.erase(buf2.begin(), buf2.begin() + b2_.get());
      buf3.erase(buf3.begin(), buf3.begin() + b3_.get());


   
      std::vector<int> produced;
      produced.reserve(a4_.get());
      for (int i = 0; i < a4_.get(); ++i)
        produced.push_back(++counter);

      out.emit(produced, "out");
      std::cout << "[Op3] produces :";
    for (auto v : produced) std::cout << " " << v;
     std::cout <<  std::endl;
        holoscan::conditions::tok_op1_op3.fetch_sub( b2_.get());
      holoscan::conditions::tok_op2_op3.fetch_sub( b3_.get());
      
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

    auto msg = in.receive<std::vector<int>>("in");
    while (msg) {
      buffer.insert(buffer.end(), msg->begin(), msg->end());
      holoscan::conditions::tok_op3_op4.fetch_add(msg->size());
      msg = in.receive<std::vector<int>>("in");
    }

    
    if (buffer.size() >= static_cast<size_t>(b4_.get())) {
      std::cout << "[Op4] consumes:";
      for (int i = 0; i <  b4_.get(); ++i) std::cout << " " << buffer[i];
      std::cout << std::endl;
      buffer.erase(buffer.begin(), buffer.begin() + b4_.get());
      holoscan::conditions::tok_op3_op4.fetch_sub(b4_.get());
      feedback_mtx.lock();
      for (int i = 0; i < 4; ++i) {
   feedback.push_back(0);
   holoscan::conditions::tok_op4_op1.fetch_add(1);
    }
   feedback_mtx.unlock();
      
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
           auto op0 = make_operator<Op0>(
        "op0",
        make_condition<CountCondition>("count_cond", 96)
        );
    auto cond1 = make_condition<conditions::BufferAwareCondition>(
      "cond1",
      Arg("receiver", "in"),
      Arg("edge_id", 5),          // 
      Arg("min_tokens", uint64_t(1)),
    Arg("tokens_per_msg", uint64_t(1))//
    );
    auto op1 = make_operator<Op1>(
      "op1",
       make_condition<CountCondition>("count", 96),
     cond1,
      Arg("a1", 2),   // Op1 → Op2 : a1 = 2
      Arg("a2", 1)  // Op1 → Op3 : a2 = 1
    );

     auto cond2 = make_condition<conditions::BufferAwareCondition>(
      "cond2",
      Arg("receiver", "in"),
      Arg("edge_id", 1),          // tok_op1_op2
      Arg("min_tokens", uint64_t(3)),  // b1 = 3
      Arg("tokens_per_msg", uint64_t(2))//each msg from op1
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
      Arg("min_tokens", uint64_t(2)) ,
      Arg("tokens_per_msg", uint64_t(1))
    );

    // in3 来自 op2 → op3
    auto cond3_in3 = make_condition<conditions::BufferAwareCondition>(
      "cond3_in3",
      Arg("receiver", "in3"),
      Arg("edge_id", 3),                
      Arg("min_tokens", uint64_t(4)),
      Arg("tokens_per_msg", uint64_t(3))
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
      Arg("min_tokens", uint64_t(2)),  // b4 = 4
      Arg("tokens_per_msg", uint64_t(1))
    );

    auto op4 = make_operator<Op4>(
      "op4",
      cond4,
      Arg("b4", 2)
    );

add_flow(op0, op1, {{"out", "in"}});
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
