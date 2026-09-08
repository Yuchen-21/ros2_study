#include <cstddef>
#include <iostream>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

// 教学子集：无框架依赖、无线程、无 ROS；每次 tick 推进一步模拟工作。
enum class Status { Success, Failure, Running };

const char * name(Status status)
{
  switch (status) {
    case Status::Success: return "SUCCESS";
    case Status::Failure: return "FAILURE";
    case Status::Running: return "RUNNING";
  }
  throw std::logic_error("unknown status");
}

struct Context
{
  int battery{80};
  bool main_blocked{false};
};

struct Node
{
  virtual ~Node() = default;
  virtual Status tick() = 0;
  virtual void halt() = 0;
};

class BatteryOK final : public Node
{
public:
  explicit BatteryOK(const Context & context) : context_(context) {}
  Status tick() override
  {
    const auto result = context_.battery >= 30 ? Status::Success : Status::Failure;
    std::cout << "  BatteryOK=" << context_.battery << ' ' << name(result) << '\n';
    return result;
  }
  void halt() override {}

private:
  const Context & context_;
};

class Move final : public Node
{
public:
  Move(std::string label, const Context & context, bool main_route)
  : label_(std::move(label)), context_(context), main_route_(main_route) {}

  Status tick() override
  {
    if (main_route_ && context_.main_blocked) {
      std::cout << "  " << label_ << " FAILURE (blocked)\n";
      halt();
      return Status::Failure;
    }
    if (progress_ == 0) {
      ++starts;
      std::cout << "  " << label_ << " START\n";
    }
    ++progress_;
    // 不 sleep、不等待网络；真正的 ROS action 应轮询异步结果。
    if (progress_ < 3) {
      std::cout << "  " << label_ << " progress=" << progress_ << " RUNNING\n";
      return Status::Running;
    }
    progress_ = 0;
    std::cout << "  " << label_ << " SUCCESS\n";
    return Status::Success;
  }

  void halt() override
  {
    if (progress_ != 0) {
      ++halts;
      std::cout << "  " << label_ << " HALT (simulated stop)\n";
      progress_ = 0;
    }
  }

  int starts{0};
  int halts{0};

private:
  const std::string label_;
  const Context & context_;
  const bool main_route_;
  int progress_{0};
};

// RUNNING 时记住当前候选；失败才试下一个。节点引用由 main 的栈对象持有。
class Fallback final : public Node
{
public:
  explicit Fallback(std::vector<Node *> children) : children_(std::move(children)) {}
  Status tick() override
  {
    while (index_ < children_.size()) {
      const auto result = children_[index_]->tick();
      if (result == Status::Running) {
        return result;
      }
      if (result == Status::Success) {
        halt();
        return result;
      }
      children_[index_]->halt();
      ++index_;
    }
    halt();
    return Status::Failure;
  }
  void halt() override
  {
    for (auto * child : children_) {
      child->halt();
    }
    index_ = 0;
  }

private:
  const std::vector<Node *> children_;
  std::size_t index_{0};
};

// 每一轮从第一个条件重查；前置条件不再成功时中断后面的运行分支。
class ReactiveSequence final : public Node
{
public:
  explicit ReactiveSequence(std::vector<Node *> children) : children_(std::move(children)) {}
  Status tick() override
  {
    for (std::size_t i = 0; i < children_.size(); ++i) {
      const auto result = children_[i]->tick();
      if (result != Status::Success) {
        for (std::size_t j = i + 1; j < children_.size(); ++j) {
          children_[j]->halt();
        }
        if (result == Status::Failure) {
          children_[i]->halt();
        }
        return result;
      }
    }
    halt();
    return Status::Success;
  }
  void halt() override
  {
    for (auto * child : children_) {
      child->halt();
    }
  }

private:
  const std::vector<Node *> children_;
};

void require(bool condition, const char * message)
{
  if (!condition) {
    throw std::runtime_error(message);
  }
}

int main(int argc, char ** argv)
{
  try {
    const std::string scenario = argc == 2 ? argv[1] : "normal";
    require(argc <= 2 && (scenario == "normal" || scenario == "low_battery" ||
      scenario == "blocked"), "usage: bt_delivery [normal|low_battery|blocked]");
    Context context;
    context.main_blocked = scenario == "blocked";
    BatteryOK battery(context);
    Move main_route("MainRoute", context, true);
    Move detour("Detour", context, false);
    Fallback routes({&main_route, &detour});
    ReactiveSequence root({&battery, &routes});
    int round = 0;
    const auto tick = [&]() {
        std::cout << "tick " << ++round << '\n';
        const auto result = root.tick();
        std::cout << "root " << name(result) << '\n';
        return result;
      };
    require(tick() == Status::Running, "first tick must start a route");
    if (scenario == "low_battery") {
      context.battery = 20;
      require(tick() == Status::Failure, "low battery must fail the root");
      require(main_route.halts == 1 && detour.starts == 0, "must stop delivery, not try detour");
      context.battery = 80;
      // 模拟上层明确发起一次新尝试，不能偷偷沿用已中断的进度。
      std::cout << "supervisor: retry after battery restored\n";
      require(tick() == Status::Running, "retry must restart");
      require(tick() == Status::Running, "retry needs its own progress");
      require(tick() == Status::Success, "retry must finish");
      require(main_route.starts == 2, "one start per attempt");
    } else {
      require(tick() == Status::Running, "second tick must still be running");
      require(tick() == Status::Success, "third tick must finish");
      require(main_route.starts == (context.main_blocked ? 0 : 1), "wrong main route starts");
      require(detour.starts == (context.main_blocked ? 1 : 0), "wrong fallback route starts");
      require(main_route.halts == 0 && detour.halts == 0, "success is not an interruption");
    }
    std::cout << "BT checks passed: " << scenario << '\n';
  } catch (const std::exception & error) {
    std::cerr << error.what() << '\n';
    return 1;
  }
}
