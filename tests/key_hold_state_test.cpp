#include "key_hold_state.h"

#include <cassert>

int main() {
    platform::KeyHoldState state;

    auto transition = state.update('A', true);
    assert(transition.started_press && transition.released_key == 0);
    assert(state.active_key() == 'A');

    transition = state.update('A', true);
    assert(!transition.started_press && transition.released_key == 0);

    transition = state.update('B', true);
    assert(!transition.started_press && transition.released_key == 0);
    assert(state.active_key() == 'A');

    transition = state.update('A', false);
    assert(!transition.started_press && transition.released_key == 'A');
    assert(state.active_key() == 0);

    transition = state.update('B', true);
    assert(transition.started_press && transition.released_key == 0);
    assert(state.active_key() == 'B');
    transition = state.update('B', false);
    assert(!transition.started_press && transition.released_key == 'B');
    assert(state.active_key() == 0);

    transition = state.update('B', false);
    assert(!transition.started_press && transition.released_key == 0);

    state.update('C', true);
    assert(state.reset() == 'C');
    assert(state.active_key() == 0);
    transition = state.update('C', false);
    assert(!transition.started_press && transition.released_key == 0);
    transition = state.update('D', true);
    assert(transition.started_press && transition.released_key == 0);
    assert(state.reset() == 'D');
    assert(state.reset() == 0);
    return 0;
}
