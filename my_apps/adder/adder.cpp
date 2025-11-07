#include <iostream>
#include <holoscan/holoscan.hpp>

using namespace holoscan;

// 
class InputOp : public Operator {
 public:
  HOLOSCAN_OPERATOR_FORWARD_ARGS(InputOp)
   InputOp()=default; 
  void setup(OperatorSpec& spec) override {
    spec.output<int>("a");
    spec.output<int>("b");
    spec.output<int>("c");
  }

  void compute(InputContext&, OutputContext& op_output, ExecutionContext&) override {
    int a, b, c;
    std::cout << "Enter a, b, c: ";
    std::cin >> a >> b >> c;

    op_output.emit(a, "a");
    op_output.emit(b, "b");
    op_output.emit(c, "c");
  }
};

// 
class GenericAddOp : public Operator {
 public:
  HOLOSCAN_OPERATOR_FORWARD_ARGS(GenericAddOp)
  GenericAddOp()=default;
  void setup(OperatorSpec& spec) override {
    spec.input<int>("in1");
    spec.input<int>("in2");
    spec.output<int>("sum");
  }

  void compute(InputContext& op_input, OutputContext& op_output, ExecutionContext&) override {
    auto v1 = op_input.receive<int>("in1").value();
    auto v2 = op_input.receive<int>("in2").value();
    op_output.emit(v1 + v2, "sum");
  }
};

// 
class PrintOp : public Operator {
 public:
  HOLOSCAN_OPERATOR_FORWARD_ARGS(PrintOp)
  PrintOp()=default;
  void setup(OperatorSpec& spec) override {
    spec.input<int>("in");
  }

  void compute(InputContext& op_input, OutputContext&, ExecutionContext&) override {
    auto value = op_input.receive<int>("in").value();
    std::cout << "Final result = " << value << std::endl;
  }
};

// 
class MyApp : public Application {
 public:
  void compose() override {
    auto input = make_operator<InputOp>("input", make_condition<CountCondition>(5));
   // auto input   = make_operator<InputOp>("input");
    auto add1    = make_operator<GenericAddOp>("add1");
    auto add2    = make_operator<GenericAddOp>("add2");
    auto printer = make_operator<PrintOp>("printer");

    //in  a, b -> add1
    add_flow(input, add1, {{"a", "in1"}, {"b", "in2"}});
    // add1 to  sum -> add2.in1
    add_flow(add1, add2, {{"sum", "in1"}});
    // in  c -> add2.in2
    add_flow(input, add2, {{"c", "in2"}});
    // add2 output  sum -> print
    add_flow(add2, printer, {{"sum", "in"}});
  }
};

int main() {
  auto app = make_application<MyApp>();
  app->run();
  return 0;
}

