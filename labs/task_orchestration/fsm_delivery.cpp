#include <iostream>
#include <stdexcept>
#include <string>
#include <string_view>

enum class State { Idle, Delivering, Returning, Completed, Fault };
enum class Event { Order, Arrived, Cancel, LowBattery, Docked, Error, Reset };

std::string_view name(State state)
{
  switch (state) {
    case State::Idle: return "Idle";
    case State::Delivering: return "Delivering";
    case State::Returning: return "Returning";
    case State::Completed: return "Completed";
    case State::Fault: return "Fault";
  }
  throw std::logic_error("unknown state");
}

std::string_view name(Event event)
{
  switch (event) {
    case Event::Order: return "Order";
    case Event::Arrived: return "Arrived";
    case Event::Cancel: return "Cancel";
    case Event::LowBattery: return "LowBattery";
    case Event::Docked: return "Docked";
    case Event::Error: return "Error";
    case Event::Reset: return "Reset";
  }
  throw std::logic_error("unknown event");
}

class Delivery
{
public:
  void dispatch(Event event, int battery = 80)
  {
    const State before = state_;
    // 先处理全局故障，再处理局部规则；顺序是本例的显式策略。
    if (event == Event::Error) {
      state_ = State::Fault;
    } else if (state_ == State::Idle && event == Event::Order) {
      if (battery < 30) {
        std::cout << "REJECT Idle + Order battery=" << battery << '\n';
        return;
      }
      state_ = State::Delivering;
    } else if (state_ == State::Delivering && event == Event::Arrived) {
      state_ = State::Completed;
    } else if (state_ == State::Delivering &&
      (event == Event::Cancel || event == Event::LowBattery)) {
      // 概念演示只变更模式；真实 action 必须先确认旧目标取消。
      state_ = State::Returning;
    } else if (state_ == State::Returning && event == Event::Docked) {
      state_ = State::Idle;
    } else if ((state_ == State::Completed || state_ == State::Fault) &&
      event == Event::Reset) {
      // 本例假设故障已经排除；工程版本应增加复位条件。
      state_ = State::Idle;
    }
    if (before == state_) {
      std::cout << "IGNORE " << name(before) << " + " << name(event) << '\n';
    } else {
      std::cout << name(before) << " + " << name(event) << " -> " << name(state_) << '\n';
    }
  }

  State state() const { return state_; }

private:
  State state_{State::Idle};
};

int main()
{
  try {
    Delivery robot;
    // 每步检查业务结果，避免仅检查最终 Idle 漏掉中途非法转移。
    const auto step = [&robot](Event event, State expected, int battery = 80) {
        robot.dispatch(event, battery);
        if (robot.state() != expected) {
          throw std::runtime_error("unexpected state after " + std::string(name(event)));
        }
      };
    step(Event::Arrived, State::Idle);
    step(Event::Order, State::Idle, 20);
    step(Event::Order, State::Idle, 29);
    step(Event::Order, State::Delivering, 30);
    step(Event::Cancel, State::Returning);
    step(Event::Cancel, State::Returning);
    step(Event::Order, State::Returning);
    step(Event::Docked, State::Idle);
    step(Event::Order, State::Delivering);
    step(Event::LowBattery, State::Returning);
    step(Event::Docked, State::Idle);
    step(Event::Order, State::Delivering);
    step(Event::Arrived, State::Completed);
    step(Event::Reset, State::Idle);
    step(Event::Order, State::Delivering);
    step(Event::Error, State::Fault);
    step(Event::Order, State::Fault);
    step(Event::Reset, State::Idle);
    std::cout << "FSM checks passed\n";
  } catch (const std::exception & error) {
    std::cerr << error.what() << '\n';
    return 1;
  }
}
