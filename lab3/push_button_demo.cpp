/**
 * ECEE 2160 Lab Assignment 3.4 - Controlling the push buttons.
 *
 * Author:  Brian Schubert
 * Date:    2020-07-26
 *
 * References
 * ==========
 *
 * - https://en.cppreference.com/w/cpp/language/operator_incdec
 * - https://en.cppreference.com/w/cpp/language/user_literal
 * - https://en.cppreference.com/w/cpp/header/thread
 * - https://stackoverflow.com/questions/1658386/
 *
 */

#include <chrono>           // for std::chrono::duration
#include <functional>       // for std::function
#include <thread>           // for std::this_thread

#include "prelab/prelab_fpga.h"


// Using anonymous namespace to given symbols internal linkage.
namespace {

// For access to duration literals.
using namespace std::literals::chrono_literals;

/**
 * The period of time elapsed between button reads.
 */
constexpr std::chrono::duration REFRESH_PERIOD = 1ms;

/**
 * The state of the board's switches that signals the program to exit.
 */
constexpr Register SWITCH_EXIT_SENTINEL = 0;

/**
 * A counter over an interval [0,max] for some max that wraps around its
 * endpoints.
 */
class WrappedCounter {
    /**
     * Integral type used to represent this counter's value.
     */
    using Count = std::uint64_t;

    /**
     * The current state of the counter.
     */
    Count m_couter{0};

    /**
     * The maximum value that the counter can reached before wrapping to 0.
     */
    const Count m_max;

  public:
    /**
     * Constructs a counter that wraps upon advancing past the given maximum.
     *
     * @param max Maximum counter value.
     */
    explicit WrappedCounter(Count max) : m_max{max} {}

    /**
     * Converts this counter into an integral value.
     *
     * We allow this conversion to occur implicitly for simplicity.
     *
     * @return Counter state.
     */
    operator Count() const { return m_couter; }

    /**
     * Applies the given callable to the internal counter and stores the
     * result, modulo (this counter max value + 1), as the new counter value.
     *
     * @param func Callable to mutate the internal counter.
     */
    void apply(std::function<Count(Count)> func)
    {
        auto new_counter = func(m_couter);
        m_couter = new_counter % (m_max + 1);
    }

    Count operator++()
    {
        m_couter = m_couter == m_max ? 0 : m_couter + 1;
        return m_couter;
    }

    Count operator--()
    {
        m_couter = m_couter == 0 ? m_max : m_couter - 1;
        return m_couter;
    }

    Count operator++(int)
    {
        auto tmp = m_couter;
        ++(*this);
        return tmp;
    }

    Count operator--(int)
    {
        auto tmp = m_couter;
        --(*this);
        return tmp;
    }
};

void run_button_demo(VirtualMappingBase virtual_base);

} // end namespace

int main()
{
    auto[virtual_base, fd] = Initialize();

    run_button_demo(virtual_base);

    Finalize(virtual_base, fd);
}

// Internal definitions.
namespace {

void run_button_demo(VirtualMappingBase virtual_base)
{
    // Counter holding the state to be written to the board's LEDs.
    WrappedCounter counter{(1u << LED_COUNT) - 1};

    // The state of the DE1SoC's button's during the previous cycle.
    auto previous_button{PushButton::None};

    // Whether to continue the button demo.
    bool run_demo{true};

    while (run_demo) {
        // Current state of the board's buttons.
        const auto button_press = PushButtonGet(virtual_base);

        // If true, the user recently pressed multiple buttons. We want to
        // wait until all buttons have been released before taking any new actions.
        const bool wait_for_unpress = previous_button == PushButton::Multiple
            && button_press != PushButton::None;
        // If true, the button state has not changed, so no action should be taken.
        const bool no_button_change = button_press == previous_button;

        if (wait_for_unpress || no_button_change) {
            std::this_thread::sleep_for(REFRESH_PERIOD);
            continue;
        }

        switch (button_press) {
            case (PushButton::None): { // No action
                break;
            }
            case (PushButton::Button0): { // Increment the LEDs
                ++counter;
                break;
            }
            case PushButton::Button1: { // Decrement the LEDs
                --counter;
                break;
            }
            case PushButton::Button2: { // Shift the LEDs to the right.
                counter.apply([](auto count) { return count >> 1u; });
                break;
            }
            case PushButton::Button3: { // Shift the LEDs to the left.
                counter.apply([](auto count) { return count << 1u; });
                break;
            }
            case PushButton::Multiple: {
                const auto switch_state = ReadAllSwitches(virtual_base);

                // Set the LEDs to match the state of the switches.
                counter.apply([=](auto) { return switch_state; });

                // Check if the current state of the switches matches the exit state.
                if (switch_state == SWITCH_EXIT_SENTINEL) {
                    run_demo = false;
                }
            }
        }

        // Update the board's LEDs.
        WriteAllLeds(virtual_base, static_cast<Register>(counter));
        previous_button = button_press;

        std::this_thread::sleep_for(REFRESH_PERIOD);
    }

}

} // end namespace

