#pragma once

// Enum for oder and sides
// GoodTillCancel: Remain active in market until it is either full filled or cancel
// FillAndKill: Execute/fill the order immediately (either partially or fully) or cancel it if no execution
// FillOrKill: Is diff from FillAndKill, this 1 is only being filled for the required quantity or else dw to fill it
// Market: Fill the order no matter whats the price is
// GoodForDay: Works like GoodTillCancel but with time constraint where the order will be remove when reach the set time
enum class OrderType
{
    GoodTillCancel,
    FillAndKill,
    FillOrKill,
    GoodForDay,
    Market
};
