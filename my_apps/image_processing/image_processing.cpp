#include <holoscan/holoscan.hpp>//对于已经存在的数据，不需要condition来等待，直接整存整取给定的token
#include <fstream>
#include <sstream>
#include <vector>
#include <cmath>
#include <iostream>
#include <algorithm>
#include <numeric>
#include <stdexcept>


#include <chrono>
#include <thread>
#include <string>
#include <sched.h>
#include <sys/syscall.h>
#include <unistd.h>

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

using ImageD = std::vector<std::vector<double>>;
using IntFlat = std::vector<int>;

enum class Control { Enable, Disable };
const std::string asciiLevels = " .:-=+/tzUw*0#%@";

// Utility
namespace util {
std::tuple<int,int,IntFlat> readPPM(const std::string& path) {
  std::ifstream file(path);
  if (!file.is_open()) throw std::runtime_error("Cannot open file: " + path);
  std::string magic; int w,h,maxv;
  file >> magic >> w >> h >> maxv;
  if (magic != "P3") throw std::runtime_error("Invalid PPM file");
  IntFlat pixels;
  int v;
  while (file >> v) pixels.push_back(v);
  return {w, h, pixels};
}

std::vector<std::vector<int>> toImageInt(int w, int h, const IntFlat& flat) {
  std::vector<std::vector<int>> img(h, std::vector<int>(w * 3));
  int idx = 0;
  for (int y = 0; y < h; y++)
    for (int x = 0; x < w*3; x++)
      img[y][x] = flat[idx++];
  return img;
}
} // namespace util

// Operators
// 1. ReadPPMOp
class ReadPPMOp : public holoscan::Operator {
 public:
  HOLOSCAN_OPERATOR_FORWARD_ARGS(ReadPPMOp)
  ReadPPMOp() = default;

  void setup(holoscan::OperatorSpec& spec) override {
    spec.output<int>("dimX");
    spec.output<int>("dimY");
    spec.output<IntFlat>("pixels");
    spec.param(ppm_path_, "ppm_path",
               "Path to PPM", "Input P3 PPM image path",
               std::string("/home/lunay/Desktop/forsyde-examples/forsyde-shallow-examples/src/ForSyDe/Shallow/Example/SDF/flag.ppm"));
  }

  void compute(holoscan::InputContext&,
               holoscan::OutputContext& out,
               holoscan::ExecutionContext&) override {
    auto t_start = steady::now();
    sys_begin = std::chrono::duration_cast<ns>(t_start.time_since_epoch()).count();
    HOLOSCAN_LOG_INFO( "[START readingPPM] op={} cpu={} tid={} ts(ns)={}",
      name(), cpu_id(), thread_id(), sys_begin);
    auto [w, h, flat] = util::readPPM(ppm_path_.get());
    out.emit(w, "dimX");
    out.emit(h, "dimY");
    out.emit(flat, "pixels");
  }

 private:
  holoscan::Parameter<std::string> ppm_path_;
};

// 2. GrayscaleOp
class GrayscaleOp : public holoscan::Operator {
 public:
  HOLOSCAN_OPERATOR_FORWARD_ARGS(GrayscaleOp)
  GrayscaleOp() = default;

  void setup(holoscan::OperatorSpec& spec) override {
    spec.input<int>("dimX");
    spec.input<int>("dimY");
    spec.input<IntFlat>("pixels");
    spec.output<ImageD>("grayImage");
  }

  void compute(holoscan::InputContext& in,
               holoscan::OutputContext& out,
               holoscan::ExecutionContext&) override {
    int w = in.receive<int>("dimX").value();
    int h = in.receive<int>("dimY").value();
    auto flat = in.receive<IntFlat>("pixels").value();
    // convert to image int
    auto rgb = util::toImageInt(w, h, flat);
    ImageD gray(h, std::vector<double>(w));
    for (int y = 0; y < h; y++) {
      for (int x = 0; x < w; x++) {
        double r = rgb[y][3*x];
        double g = rgb[y][3*x + 1];
        double b = rgb[y][3*x + 2];
        gray[y][x] = r * 0.3125 + g * 0.5625 + b * 0.125;
      }
    }
    out.emit(gray, "grayImage");
  }
};

// 3. ResizeOp
class ResizeOp : public holoscan::Operator {
 public:
  HOLOSCAN_OPERATOR_FORWARD_ARGS(ResizeOp)
  ResizeOp() = default;

  void setup(holoscan::OperatorSpec& spec) override {
    spec.input<ImageD>("grayImage");
    spec.output<ImageD>("resizedImage");
  }

  void compute(holoscan::InputContext& in,
               holoscan::OutputContext& out,
               holoscan::ExecutionContext&) override {
    auto gray = in.receive<ImageD>("grayImage").value();
    int h = gray.size();
    int w = gray[0].size();
    int nh = h / 2;
    int nw = w / 2;
    ImageD outImg(nh, std::vector<double>(nw));
    for (int y=0; y<nh; y++) {
      for (int x=0; x<nw; x++) {
        double sum = gray[2*y][2*x] +
                     gray[2*y][2*x+1] +
                     gray[2*y+1][2*x] +
                     gray[2*y+1][2*x+1];
        outImg[y][x] = sum / 4.0;
      }
    }
    out.emit(outImg, "resizedImage");
  }
};

// 4. BrightnessOp
class BrightnessOp : public holoscan::Operator {
 public:
  HOLOSCAN_OPERATOR_FORWARD_ARGS(BrightnessOp)
  BrightnessOp() = default;

  void setup(holoscan::OperatorSpec& spec) override {
    spec.input<ImageD>("resizedImage");
    spec.output<double>("hmin");
    spec.output<double>("hmax");
  }

  void compute(holoscan::InputContext& in,
               holoscan::OutputContext& out,
               holoscan::ExecutionContext&) override {
    auto img = in.receive<ImageD>("resizedImage").value();
    double mn = std::numeric_limits<double>::infinity();
    double mx = -std::numeric_limits<double>::infinity();
    for (auto& row : img) {
      for (auto v : row) {
        mn = std::min(mn, v);
        mx = std::max(mx, v);
      }
    }
    out.emit(mn, "hmin");
    out.emit(mx, "hmax");
  }
};

// 
class ControlOp : public holoscan::Operator {
 public:
  HOLOSCAN_OPERATOR_FORWARD_ARGS(ControlOp)
  ControlOp() = default;

  void setup(holoscan::OperatorSpec& spec) override {
    // 输入来自 brightness（每次更新 hmin, hmax）
    spec.input<double>("hmin");
    spec.input<double>("hmax");
    // 输出发给 correction
    spec.output<Control>("ctrl");
  }

 

  void compute(holoscan::InputContext& in,
               holoscan::OutputContext& out,
               holoscan::ExecutionContext&) override {
    double hmin = in.receive<double>("hmin").value();
    double hmax = in.receive<double>("hmax").value();
    if (state_.empty()) {
      state_ = {255.0, 255.0, 255.0};
    }

    double diff = hmax - hmin;
    state_.insert(state_.begin(), diff);
    if (state_.size() > 3) state_.pop_back();
    double avg = std::accumulate(state_.begin(), state_.end(), 0.0) / state_.size();
    Control c = (avg < 128.0) ? Control::Enable : Control::Disable;

    out.emit(c, "ctrl");
  }

 private:
  std::vector<double> state_;  // internal delaySDF buffer
};


// 6. CorrectionOp
class CorrectionOp : public holoscan::Operator {
 public:
  HOLOSCAN_OPERATOR_FORWARD_ARGS(CorrectionOp)
  CorrectionOp() = default;

  void setup(holoscan::OperatorSpec& spec) override {
    spec.input<ImageD>("resizedImage");
    spec.input<double>("hmin");
    spec.input<double>("hmax");
    spec.input<Control>("ctrl");
    spec.output<ImageD>("correctedImage");
  }

  void compute(holoscan::InputContext& in,
               holoscan::OutputContext& out,
               holoscan::ExecutionContext&) override {
    auto img = in.receive<ImageD>("resizedImage").value();
    double hmin = in.receive<double>("hmin").value();
    double hmax = in.receive<double>("hmax").value();
    Control c = in.receive<Control>("ctrl").value();

    ImageD outImg = img;
    if (c == Control::Enable) {
      double diff = hmax - hmin;
      double scale;
      if      (diff > 127) scale = 1.0;
      else if (diff > 63)  scale = 2.0;
      else if (diff > 31)  scale = 4.0;
      else if (diff > 15)  scale = 8.0;
      else                 scale = 16.0;
      for (auto& row : outImg)
        for (auto& v : row)
          v = std::max(0.0, (v - hmin) * scale);
    }
    out.emit(outImg, "correctedImage");
  }
};

// 7. SobelOp
class SobelOp : public holoscan::Operator {
 public:
  HOLOSCAN_OPERATOR_FORWARD_ARGS(SobelOp)
  SobelOp() = default;

  void setup(holoscan::OperatorSpec& spec) override {
    spec.input<ImageD>("correctedImage");
    spec.output<ImageD>("edges");
  }

  void compute(holoscan::InputContext& in,
               holoscan::OutputContext& out,
               holoscan::ExecutionContext&) override {
    auto img = in.receive<ImageD>("correctedImage").value();
    int h = img.size();
    int w = img[0].size();
    ImageD edges(h-2, std::vector<double>(w-2));
    int gx[3][3] = {{-1,0,1},{-2,0,2},{-1,0,1}};
    int gy[3][3] = {{-1,-2,-1},{0,0,0},{1,2,1}};
    for (int y=1; y < h-1; y++) {
      for (int x=1; x < w-1; x++) {
        double sx = 0, sy = 0;
        for (int ky=-1; ky<=1; ky++){
          for (int kx=-1; kx<=1; kx++){
            sx += gx[ky+1][kx+1] * img[y+ky][x+kx];
            sy += gy[ky+1][kx+1] * img[y+ky][x+kx];
          }
        }
        edges[y-1][x-1] = std::sqrt(sx*sx + sy*sy) / 4.0;
      }
    }
    out.emit(edges, "edges");
  }
};

// 8. AsciiOp
class AsciiOp : public holoscan::Operator {
 public:
  HOLOSCAN_OPERATOR_FORWARD_ARGS(AsciiOp)
  AsciiOp() = default;

  void setup(holoscan::OperatorSpec& spec) override {
    spec.input<ImageD>("edges");
    spec.output<std::string>("ascii");
  }

  void compute(holoscan::InputContext& in,
               holoscan::OutputContext& out,
               holoscan::ExecutionContext&) override {
    auto edges = in.receive<ImageD>("edges").value();
    int h = edges.size();
    int w = edges[0].size();
    std::ostringstream oss;
    int nLevels = asciiLevels.size() - 1;
    for (int y = 0; y < h; y++){
      for (int x = 0; x < w; x++){
        double v = edges[y][x];
        v = std::min(255.0, std::max(0.0, v));
        int level = static_cast<int>(nLevels * (v / 255.0));
        oss << asciiLevels[level];
      }
      oss << "\n";
    }
    out.emit(oss.str(), "ascii");
  }
};

// 9. DisplayOp
class DisplayOp : public holoscan::Operator {
 public:
  HOLOSCAN_OPERATOR_FORWARD_ARGS(DisplayOp)
  DisplayOp() = default;

  void setup(holoscan::OperatorSpec& spec) override {
    spec.input<std::string>("ascii");
  }

  void compute(holoscan::InputContext& in,
               holoscan::OutputContext&,
               holoscan::ExecutionContext&) override {
    auto ascii = in.receive<std::string>("ascii").value();
    std::cout << ascii;
    // After displaying, we exit the app.
    std::exit(0);
  }
};

// Application
class ImageProcessingApp : public holoscan::Application {
 public:
  void compose() override {
    using namespace holoscan;
    auto read     = make_operator<ReadPPMOp>("read");
    auto gray     = make_operator<GrayscaleOp>("gray");
    auto resize   = make_operator<ResizeOp>("resize");
    auto bright   = make_operator<BrightnessOp>("bright");
    auto ctrl     = make_operator<ControlOp>("ctrl");
    auto corr     = make_operator<CorrectionOp>("corr");
    auto sob      = make_operator<SobelOp>("sobel");
    auto ascii    = make_operator<AsciiOp>("ascii");
    auto display  = make_operator<DisplayOp>("display");

    add_flow(read,   gray,   {{"dimX","dimX"},{"dimY","dimY"},{"pixels","pixels"}});
    add_flow(gray,   resize, {{"grayImage","grayImage"}});
    add_flow(resize, bright, {{"resizedImage","resizedImage"}});
    add_flow(bright, ctrl,   {{"hmin","hmin"},{"hmax","hmax"}});
    add_flow(resize, corr,   {{"resizedImage","resizedImage"}});
    add_flow(bright, corr,   {{"hmin","hmin"},{"hmax","hmax"}});
    add_flow(ctrl,   corr,   {{"ctrl","ctrl"}});
    add_flow(corr,   sob,    {{"correctedImage","correctedImage"}});
    add_flow(sob,    ascii,  {{"edges","edges"}});
    add_flow(ascii,  display,{ {"ascii","ascii"} });
  }
};

int main(int argc, char** argv) {
  auto app = holoscan::make_application<ImageProcessingApp>();
  app->run();
  return 0;
}

