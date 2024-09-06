#pragma once
#include "Usings.h"

// Represent information about a prive level in the orderbook
// Price and total quantity available for the specific price point

struct LevelInfo
{
    Price price_;
    Quantity quantity_;
};

using LevelInfos = std::vector<LevelInfo>;
