#include <holoscan/holoscan.hpp>

#include <chrono>
#include <thread>
#include <vector>
#include <string>

#include <sched.h>
#include <sys/syscall.h>
#include <unistd.h>

using holoscan::Operator;
using holoscan::OperatorSpec;
using holoscan::InputContext;
using holoscan::OutputContext;
using holoscan::ExecutionContext;

static int worker_num=16;
static int64_t sys_begin = 0;
static int64_t sys_end   = 0;
inline int cpu_id() {
  return sched_getcpu();
}

inline pid_t thread_id() {
  return syscall(SYS_gettid);
}

using steady = std::chrono::steady_clock;
using ns     = std::chrono::nanoseconds;
void busy_loop(uint64_t iters) {
    volatile uint64_t x = 0;
    for (uint64_t i = 0; i < iters; ++i) {
      x += i;
    }
  }

class Op0 : public Operator {
 public:
  HOLOSCAN_OPERATOR_FORWARD_ARGS(Op0)
Op0() = default;
  void setup(OperatorSpec& spec) override {
    for (int i = 1; i <= worker_num; ++i) {
      spec.output<int>(("out" + std::to_string(i)).c_str());
    }
  }

  void compute(InputContext&,
               OutputContext& out,
               ExecutionContext&) override {
        auto t_start = steady::now();
     sys_begin =
      std::chrono::duration_cast<ns>(t_start.time_since_epoch()).count();
    HOLOSCAN_LOG_INFO(
      "[Sys START] op={} cpu={} tid={} ts(ns)={}",
      name(), cpu_id(), thread_id(), sys_begin
    );
    for (int i = 1; i <= worker_num; ++i) {
      std::string port = "out" + std::to_string(i);
      out.emit(i, port.c_str());
    }

    auto t = steady::now();
    auto ts =
      std::chrono::duration_cast<ns>(t.time_since_epoch()).count();

    HOLOSCAN_LOG_INFO(
      "[END]   op={} cpu={} tid={} ts(ns)={}",
      name(), cpu_id(), thread_id(), ts
    );
  }
};

class WorkerOp : public Operator {
 public:
  HOLOSCAN_OPERATOR_FORWARD_ARGS(WorkerOp)
  WorkerOp() = default;


  void setup(OperatorSpec& spec) override {
    spec.input<int>("in");
    spec.output<int>("out");

  }

  void compute(InputContext& in,
               OutputContext& out,
               ExecutionContext&) override {
        auto t_start = steady::now();
    auto start_ns =
      std::chrono::duration_cast<ns>(t_start.time_since_epoch()).count();

    HOLOSCAN_LOG_INFO(
      "[START] op={} cpu={} tid={} ts(ns)={}",
      name(), cpu_id(), thread_id(), start_ns
    );
    auto v = in.receive<int>("in").value();
    busy_loop(2e8);
    out.emit(v, "out");

    auto t_end = steady::now();
    auto end_ns =
      std::chrono::duration_cast<ns>(t_end.time_since_epoch()).count();

    HOLOSCAN_LOG_INFO(
      "[END]   op={} cpu={} tid={} ts(ns)={} duration(ns)={}",
      name(), cpu_id(), thread_id(),
      end_ns, end_ns - start_ns
    );
  }
};



class Op9 : public Operator {
 public:
  HOLOSCAN_OPERATOR_FORWARD_ARGS(Op9)
Op9() = default;
  void setup(OperatorSpec& spec) override {
    for (int i = 1; i <= worker_num; ++i) {
      spec.input<int>(("in" + std::to_string(i)).c_str());
    }
  }

  void compute(InputContext& in,
               OutputContext&,
               ExecutionContext&) override {

    auto t = steady::now();
    auto ts =
      std::chrono::duration_cast<ns>(t.time_since_epoch()).count();

    HOLOSCAN_LOG_INFO(
      "[START] op={} cpu={} tid={} ts(ns)={}",
      name(), cpu_id(), thread_id(), ts
    );

    for (int i = 1; i <= worker_num; ++i) {
      std::string port = "in" + std::to_string(i);
      in.receive<int>(port.c_str());
    }
    auto t_end = steady::now();
     sys_end =
      std::chrono::duration_cast<ns>(t_end.time_since_epoch()).count();

    HOLOSCAN_LOG_INFO(
      "[Sys end] op={} cpu={} tid={} ts(ns)={} sys duration(ns)={}",
      name(), cpu_id(), thread_id(), sys_end, sys_end-sys_begin
    );
  }
};


class FanOutFanInApp : public holoscan::Application {
 public:
  void compose() override {
    using namespace holoscan;

    auto op0 = make_operator<Op0>(
      "op0",
      make_condition<CountCondition>(1)
    );

    std::vector<std::shared_ptr<Operator>> workers;
    for (int i = 1; i <= worker_num; ++i) {
      workers.push_back(
        make_operator<WorkerOp>("op" + std::to_string(i))
      );
    }

    auto opsink = make_operator<Op9>("opsink");

    for (int i = 1; i <= worker_num; ++i) {
      add_flow(op0, workers[i - 1],
        {{("out" + std::to_string(i)), "in"}});
      add_flow(workers[i - 1], opsink,
        {{"out", ("in" + std::to_string(i))}});
    }
  }
};


int main() {
  auto app = holoscan::make_application<FanOutFanInApp>();



//app->scheduler(
 // app->make_scheduler<holoscan::GreedyScheduler>("greedy")
//);



app->scheduler(
  app->make_scheduler<holoscan::MultiThreadScheduler>(
    "mts",
    holoscan::Arg("worker_thread_number", int64_t(8))  )
);



 // app->scheduler(
  //  app->make_scheduler<holoscan::EventBasedScheduler>(
 //     "ebs",
    //  holoscan::Arg("worker_thread_number", int64_t(8))
   // )
 // );

  app->run();
  return 0;
}

