#pragma once

template<typename T>
bool between(T x, T lo, T hi) {
    return x >= lo && x <= hi;
}