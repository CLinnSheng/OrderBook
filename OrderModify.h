#pragma once
#include "Order.h"

// Storing a single order with multiple data structure in an order book
class OrderModify
{
public:
    OrderModify(OrderId orderId, Side side, Price price, Quantity quantity)
        : orderId_{ orderId }
        , price_{ price }
        , side_{ side }
        , quantity_{ quantity }
    { }

    OrderId GetOrderId() const { return orderId_; }
    Side GetSide() const { return side_; }
    Price GetPrice() const { return price_; }
    Quantity GetQuantity() const { return quantity_; }

    // Converting a given order that is already existed transforming this modify order into a new order
    OrderPointer ToOrderPointer(OrderType type) const
    {
        return std::make_shared<Order>(type, orderId_, side_, price_, quantity_);
    }

private:
    OrderId orderId_;
    Price price_;
    Side side_;
    Quantity quantity_;

};
