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

// ---------- helpers ----------
inline int cpu_id() {
  return sched_getcpu();
}

inline pid_t thread_id() {
  return syscall(SYS_gettid);
}

using steady = std::chrono::steady_clock;
using ns     = std::chrono::nanoseconds;

// =======================================================
// op0 : fan-out producer
// =======================================================
class Op0 : public Operator {
 public:
  HOLOSCAN_OPERATOR_FORWARD_ARGS(Op0)
Op0() = default;
  void setup(OperatorSpec& spec) override {
    for (int i = 1; i <= 8; ++i) {
      spec.output<int>(("out" + std::to_string(i)).c_str());
    }
  }

  void compute(InputContext&,
               OutputContext& out,
               ExecutionContext&) override {

    for (int i = 1; i <= 8; ++i) {
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

// =======================================================
// op1–op8 : parallel workers
// =======================================================
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

    // ---- artificial workload: 100 ms ----
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

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

// =======================================================
// op9 : fan-in sink
// =======================================================
class Op9 : public Operator {
 public:
  HOLOSCAN_OPERATOR_FORWARD_ARGS(Op9)
Op9() = default;
  void setup(OperatorSpec& spec) override {
    for (int i = 1; i <= 8; ++i) {
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

    for (int i = 1; i <= 8; ++i) {
      std::string port = "in" + std::to_string(i);
      in.receive<int>(port.c_str());
    }
  }
};

// =======================================================
// application
// =======================================================
class FanOutFanInApp : public holoscan::Application {
 public:
  void compose() override {
    using namespace holoscan;

    auto op0 = make_operator<Op0>(
      "op0",
      make_condition<CountCondition>(1)
    );

    std::vector<std::shared_ptr<Operator>> workers;
    for (int i = 1; i <= 8; ++i) {
      workers.push_back(
        make_operator<WorkerOp>("op" + std::to_string(i))
      );
    }

    auto op9 = make_operator<Op9>("op9");

    for (int i = 1; i <= 8; ++i) {
      add_flow(op0, workers[i - 1],
        {{("out" + std::to_string(i)), "in"}});
      add_flow(workers[i - 1], op9,
        {{"out", ("in" + std::to_string(i))}});
    }
  }
};

// =======================================================
// main
// =======================================================
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

