#pragma once

#include <list>
#include <exception>
#include <format>
#include <stdexcept>
#include <string>

#include "OrderType.h"
#include "Side.h"
#include "Usings.h"
#include "Constants.h"

// Represents a single order in the order book system
// Encapsulates the information related to an order like price, quantity, orderId and etc
class Order
{
public:
    Order(OrderType orderType, OrderId orderId, Side side, Price price, Quantity quantity)
        : orderType_{ orderType }
        , orderId_{ orderId }
        , side_{ side }
        , price_{ price }
        , initialQuantity_{ quantity }
        , remainingQuantity_{ quantity }
    { }

    // Dealing with Market OrderType where we dont care about the price
    // Just filled the amount of quantity requested
    Order(OrderId orderId, Side side, Quantity quantity)
        : Order(OrderType::Market, orderId, side, Constants::InvalidPrice, quantity)
    { }

    OrderId GetOrderId() const { return orderId_; }
    Side GetSide() const { return side_; }
    Price GetPrice() const { return price_; }
    OrderType GetOrderType() const { return orderType_; }
    Quantity GetInitialQuantity() const { return initialQuantity_; }
    Quantity GetRemainingQuantity() const { return remainingQuantity_; }
    Quantity GetFilledQuantity() const { return GetInitialQuantity() - GetRemainingQuantity(); }
    bool IsFilled() const { return GetRemainingQuantity() == 0; }

    // Filling the Order
    void Fill(Quantity quantity)
    {
        // it doesnt make sense when the quantity want to fill exceed the reamining quantity
        if (quantity > GetRemainingQuantity())
            throw std::logic_error("Order (" + std::to_string(GetOrderId()) + ") cannot be filled for more than its remaining quantity");

        remainingQuantity_ -= quantity;
    }

    void ToGoodTillCancel(Price price)
    {
        if (GetOrderType() != OrderType::Market)
            throw std::logic_error("Order (" + std::to_string(GetOrderId()) + ") cannot have its price adjusted, only market orders can.");

        price_ = price;
        orderType_ = OrderType::GoodTillCancel;
    }

private:
    // All the attributes of an order
    OrderType orderType_;
    OrderId orderId_;
    Side side_;
    Price price_;
    Quantity initialQuantity_;
    Quantity remainingQuantity_;
};

using OrderPointer = std::shared_ptr<Order>;
using OrderPointers = std::list<OrderPointer>;

